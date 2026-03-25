/// @file PerturbationUtils.cpp
/// @brief Shared internal utilities for perturbation implementations.

#include "PerturbationUtils.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

namespace chaos::orchestrator::perturbations::internal {

namespace {

// Use a lambda deleter to avoid -Wignored-attributes on function pointer type.
struct PipeDeleter {
    void operator()(FILE* f) const {
        if (f != nullptr) {
            pclose(f);
        }
    }
};

using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

}  // namespace

std::string fetchContainerPid(const std::string& containerId) {
    const std::string cmd = fmt::format("docker inspect --format '{{{{.State.Pid}}}}' {}", containerId);

    PipePtr pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("Failed to run docker inspect to fetch PID for: " + containerId);
    }

    std::array<char, 64> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Trim trailing whitespace / newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }

    if (result.empty() || result == "0") {
        throw std::runtime_error("Failed to resolve container PID for: " + containerId);
    }

    return result;
}

void runCommand(const std::string& cmd, const std::string& errorMsg) {
    PipePtr pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                errorMsg + ": failed to launch process");
    }

    std::array<char, 256> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    // Release the raw pointer so we can get the exit status from pclose.
    // PipeDeleter would call pclose again otherwise.
    FILE* raw = pipe.release();
    const int exitStatus = pclose(raw);

    if (exitStatus != 0) {
        SPDLOG_ERROR("Command failed (exit {}): {}\nOutput: {}", exitStatus, cmd, output);
        throw std::system_error(std::make_error_code(std::errc::operation_not_supported),
                                errorMsg + (output.empty() ? "" : ": " + output));
    }
}

void writeCgroupFile(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::system_error(std::make_error_code(std::errc::permission_denied),
                                "Failed to open cgroup file: " + path);
    }
    file << value;
    if (!file) {
        throw std::system_error(std::make_error_code(std::errc::io_error),
                                "Failed to write to cgroup file: " + path);
    }
}

}  // namespace chaos::orchestrator::perturbations::internal
