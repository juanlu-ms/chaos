#include "DockerClient.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <fmt/format.h>
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

#include "containers/IContainerEngine.hpp"
#include "shared/ContainerStatus.hpp"

namespace {

namespace {

struct ArchiveWriteDeleter {
    void operator()(archive* a) const noexcept {
        archive_write_close(a);
        archive_write_free(a);
    }
};

struct ArchiveEntryDeleter {
    void operator()(archive_entry* e) const noexcept { archive_entry_free(e); }
};

using ArchiveWritePtr = std::unique_ptr<archive, ArchiveWriteDeleter>;
using ArchiveEntryPtr = std::unique_ptr<archive_entry, ArchiveEntryDeleter>;

}  // namespace

std::string createTarArchive(const std::string_view dockerfilePath) {
    const auto contextDir = std::filesystem::path(dockerfilePath).parent_path();
    if (!std::filesystem::exists(contextDir)) {
        throw std::runtime_error(fmt::format("Build context directory does not exist: {}", contextDir.string()));
    }

    ArchiveWritePtr arch(archive_write_new());
    archive_write_set_format_ustar(arch.get());

    std::string tarData;
    archive_write_open(
        arch.get(), &tarData, nullptr,
        [](archive*, void* client, const void* buf, size_t len) {
            static_cast<std::string*>(client)->append(static_cast<const char*>(buf), len);
            return static_cast<la_ssize_t>(len);
        },
        nullptr);

    const auto canonicalContext = std::filesystem::weakly_canonical(contextDir);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             contextDir, std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_symlink()) {
            SPDLOG_WARN("Skipping symlink in build context: {}", entry.path().string());
            continue;
        }

        const auto& path = entry.path();
        if (auto canonical = std::filesystem::weakly_canonical(path);
            !canonical.string().starts_with(canonicalContext.string())) {
            SPDLOG_WARN("Path escapes build context, skipping: {}", path.string());
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const auto relative = std::filesystem::relative(path, contextDir).string();
        ArchiveEntryPtr ae(archive_entry_new());
        archive_entry_set_pathname(ae.get(), relative.c_str());
        archive_entry_set_size(ae.get(), static_cast<la_int64_t>(entry.file_size()));
        archive_entry_set_filetype(ae.get(), AE_IFREG);
        archive_entry_set_perm(ae.get(), 0644);
        archive_write_header(arch.get(), ae.get());

        std::ifstream file(path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        archive_write_data(arch.get(), content.data(), content.size());
    }

    return tarData;
}

void parseBuildResponse(const std::string_view body) {
    std::string_view remaining = body;
    while (!remaining.empty()) {
        const auto newlinePos = remaining.find('\n');

        if (const std::string_view line =
                (newlinePos == std::string_view::npos) ? remaining : remaining.substr(0, newlinePos);
            !line.empty()) {
            auto json = nlohmann::json::parse(line, nullptr, false);
            if (!json.is_discarded() && json.is_object() && json.contains("error")) {
                throw chaos::orchestrator::containers::ContainerEngineApiError(
                    fmt::format("Docker build error: {}", json["error"].get<std::string>()));
            }
        }

        if (newlinePos == std::string_view::npos) {
            break;
        }
        remaining = remaining.substr(newlinePos + 1);
    }
}

}  // namespace

namespace chaos::orchestrator::containers::internal {

DockerClient::DockerClient(RequestFn requestFn) : request_(std::move(requestFn)) {}

std::shared_ptr<containers::IContainerEngine> DockerClient::create(const std::string& socketPath) {
    SPDLOG_INFO("DockerClient: using socket {}", socketPath);
    auto client = std::make_shared<httplib::Client>(socketPath);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(30);

    auto requestFn = [client](HttpMethod method, std::string_view endpoint, std::string_view body) {
        SPDLOG_DEBUG("DockerClient: request {} {}",
                     method == HttpMethod::GET        ? "GET"
                     : method == HttpMethod::POST     ? "POST"
                     : method == HttpMethod::POST_TAR ? "POST_TAR"
                                                      : "REMOVE",
                     endpoint);
        httplib::Result response;
        std::string endpointStr(endpoint);
        switch (method) {
            using enum chaos::orchestrator::containers::internal::HttpMethod;
            case GET:
                response = client->Get(endpointStr);
                break;
            case POST:
                if (body.empty()) {
                    response = client->Post(endpointStr);
                } else {
                    response = client->Post(endpointStr, std::string(body), "application/json");
                }
                break;
            case POST_TAR:
                response = client->Post(endpointStr, std::string(body), "application/x-tar");
                break;
            case REMOVE:
                response = client->Delete(endpointStr);
                break;
        }

        if (!response) {
            SPDLOG_ERROR("DockerClient: connection to Docker socket failed");
            throw containers::ContainerEngineTransportError("Failed to connect to Docker socket");
        }
        return HttpResponse{.status = response->status, .body = response->body};
    };

    return std::make_shared<DockerClient>(std::move(requestFn));
}

std::vector<containers::Container> DockerClient::listContainers() const {
    SPDLOG_DEBUG("DockerClient: listing containers");
    const auto response = request_(HttpMethod::GET, "/containers/json", "");
    if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    auto jsonResponse = parseResponse(response);

    std::vector<containers::Container> containers;
    containers.reserve(jsonResponse.size());

    for (const auto& item : jsonResponse) {
        containers::Container container;
        if (item.contains("Id") && item["Id"].is_string()) {
            container.id = item["Id"].get<std::string>();
        }
        if (item.contains("Names") && item["Names"].is_array() && !item["Names"].empty()) {
            container.name = item["Names"][0].get<std::string>();
            if (!container.name.empty() && container.name.front() == '/') {
                container.name.erase(0, 1);
            }
        }
        if (item.contains("State") && item["State"].is_string()) {
            container.state = item["State"].get<std::string>();
        }
        containers.push_back(std::move(container));
    }

    SPDLOG_INFO("DockerClient: {} containers found", containers.size());
    return containers;
}

void DockerClient::pullImage(const std::string_view image) const {
    if (image.empty()) {
        throw std::invalid_argument("Image name cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: pulling image {}", image);

    nlohmann::json body = {
        {"Image", std::string(image)},
    };

    if (const auto response = request_(HttpMethod::POST, "/images/create", body.dump()); response.status == 404) {
        SPDLOG_ERROR("Docker API returned status {}: Repository not found", response.status, response.body);
        throw std::invalid_argument(fmt::format("Repository not found ({})", response.status));
    } else if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: image {} pulled successfully", image);
}

void DockerClient::buildImage(const std::string_view imageName, const std::string_view dockerfilePath) const {
    if (imageName.empty()) {
        throw std::invalid_argument("Image name cannot be empty");
    }
    if (dockerfilePath.empty()) {
        throw std::invalid_argument("Dockerfile path cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: building image {} from {}", imageName, dockerfilePath);

    const auto tarArchive = createTarArchive(dockerfilePath);
    const auto endpoint = fmt::format("/build?t={}", imageName);
    const auto response = request_(HttpMethod::POST_TAR, endpoint, tarArchive);

    if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {} while building image {}: {}", response.status, imageName,
                     response.body);
        throw containers::ContainerEngineApiError(
            fmt::format("Docker API returned status {} while building image {}", response.status, imageName));
    }

    parseBuildResponse(response.body);

    SPDLOG_INFO("DockerClient: image {} built successfully", imageName);
}

std::string DockerClient::createContainer(const std::string_view image, const std::vector<std::string>& options) const {
    if (image.empty()) {
        throw std::invalid_argument("Image name cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: creating container from image {}", image);

    nlohmann::json body = {
        {"Image", std::string(image)},
    };

    if (!options.empty()) {
        body["Env"] = options;
    }

    const auto response = request_(HttpMethod::POST, "/containers/create", body.dump());
    if (response.status != 201) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    auto jsonResponse = parseResponse(response);
    if (!jsonResponse.contains("Id") || !jsonResponse["Id"].is_string()) {
        throw containers::ContainerEngineParseError("Docker create response missing Id field");
    }

    SPDLOG_INFO("DockerClient: container created with id {}", jsonResponse["Id"].get<std::string>());
    return jsonResponse["Id"].get<std::string>();
}

void DockerClient::startContainer(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: starting container {}", containerId);

    if (const auto response = request_(HttpMethod::POST, fmt::format("/containers/{}/start", containerId), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} started successfully", containerId);
}

void DockerClient::stopContainer(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: stopping container {}", containerId);

    constexpr int stopTimeoutSeconds = 5;

    if (const auto response =
            request_(HttpMethod::POST, fmt::format("/containers/{}/stop?t={}", containerId, stopTimeoutSeconds), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} stopped successfully", containerId);
}

void DockerClient::killContainer(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: killing container {}", containerId);

    const auto response = request_(HttpMethod::POST, fmt::format("/containers/{}/kill", containerId), "");
    if (response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    if (!response.body.empty()) {
        parseResponse(response);
    }

    SPDLOG_INFO("DockerClient: container {} killed successfully", containerId);
}

void DockerClient::removeContainer(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: removing container {}", containerId);

    if (const auto response =
            request_(HttpMethod::REMOVE, fmt::format("/containers/{}?force=true&v=true", containerId), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} removed successfully", containerId);
}

std::string DockerClient::exec(const std::string_view containerId, const std::string_view command) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    if (command.empty()) {
        throw std::invalid_argument("Command cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: executing command {} in container {}", command, containerId);

    nlohmann::json execConfig = {
        {"AttachStdin", false}, {"AttachStdout", true},
        {"AttachStderr", true}, {"Tty", false},
        {"Privileged", false},  {"Cmd", nlohmann::json::array({"/bin/sh", "-lc", std::string(command)})},
    };

    const auto createResponse =
        request_(HttpMethod::POST, fmt::format("/containers/{}/exec", containerId), execConfig.dump());
    if (createResponse.status != 201) {
        SPDLOG_ERROR("Docker exec create failed with status {}: {}", createResponse.status, createResponse.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", createResponse.status));
    }

    auto createJson = parseResponse(createResponse);
    if (!createJson.contains("Id") || !createJson["Id"].is_string()) {
        throw containers::ContainerEngineParseError("Docker exec create response missing Id");
    }
    const auto execId = createJson["Id"].get<std::string>();

    const nlohmann::json startConfig = {
        {"Detach", false},
        {"Tty", false},
    };

    const auto startResponse = request_(HttpMethod::POST, fmt::format("/exec/{}/start", execId), startConfig.dump());
    if (startResponse.status != 200) {
        SPDLOG_ERROR("Docker exec start failed with status {}: {}", startResponse.status, startResponse.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", startResponse.status));
    }

    const auto exitResponse = request_(HttpMethod::GET, fmt::format("/exec/{}/json", execId), "");
    if (exitResponse.status != 200) {
        SPDLOG_ERROR("Docker exec inspect failed with status {}: {}", exitResponse.status, exitResponse.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", exitResponse.status));
    }

    if (const auto exitJson = parseResponse(exitResponse);
        exitJson.contains("ExitCode") && exitJson["ExitCode"].get<int>() != 0) {
        throw containers::ContainerEngineError(fmt::format("Command '{}' in container '{}' exited with code {}",
                                                           command, containerId, exitJson["ExitCode"].get<int>()));
    }

    SPDLOG_INFO("DockerClient: command executed successfully in container {}", containerId);
    return startResponse.body;
}

std::string DockerClient::execInNetNs(const std::string_view containerId, const std::string_view command) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    if (command.empty()) {
        throw std::invalid_argument("Command cannot be empty");
    }

    SPDLOG_DEBUG("DockerClient: executing in netns of container {}: {}", containerId, command);

    const auto inspectResponse = request_(HttpMethod::GET, fmt::format("/containers/{}/json", containerId), "");
    if (inspectResponse.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", containerId, inspectResponse.status));
    }

    auto inspectJson = parseResponse(inspectResponse);
    if (!inspectJson.contains("State") || !inspectJson["State"].is_object() || !inspectJson["State"].contains("Pid") ||
        !inspectJson["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", containerId));
    }

    const auto pid = inspectJson["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(fmt::format("Container '{}' is not running (PID={})", containerId, pid));
    }

    const std::string nsenterCmd = fmt::format("nsenter -t {} -n {} 2>&1", pid, command);

    auto pipe = std::unique_ptr<FILE, decltype([](FILE* f) noexcept {
                                    if (f) {
                                        pclose(f);
                                    }
                                })>(popen(nsenterCmd.c_str(), "r"));
    if (!pipe) {
        throw containers::ContainerEngineError(fmt::format("Failed to execute: nsenter -t {} -n {}", pid, command));
    }

    std::array<char, 4096> buffer{};
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    if (const int status = pclose(pipe.release()); status != 0) {
        throw containers::ContainerEngineError(
            fmt::format("Network command in container '{}' failed (exit {}): {}", containerId, status, result));
    }

    SPDLOG_INFO("DockerClient: network command executed in container {} netns", containerId);
    return result;
}

int DockerClient::getContainerNetnsFd(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }

    const auto inspectResponse = request_(HttpMethod::GET, fmt::format("/containers/{}/json", containerId), "");
    if (inspectResponse.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", containerId, inspectResponse.status));
    }

    auto inspectJson = parseResponse(inspectResponse);
    if (!inspectJson.contains("State") || !inspectJson["State"].is_object() || !inspectJson["State"].contains("Pid") ||
        !inspectJson["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", containerId));
    }

    const auto pid = inspectJson["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(fmt::format("Container '{}' is not running (PID={})", containerId, pid));
    }

    const std::string nsPath = fmt::format("/proc/{}/ns/net", pid);
    const int fd = open(nsPath.c_str(), O_RDONLY);
    if (fd < 0) {
        throw containers::ContainerEngineError(
            fmt::format("Failed to open network namespace for container '{}': {}", containerId, std::strerror(errno)));
    }

    return fd;
}

int DockerClient::getContainerPid(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }

    const auto inspectResponse = request_(HttpMethod::GET, fmt::format("/containers/{}/json", containerId), "");
    if (inspectResponse.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", containerId, inspectResponse.status));
    }

    auto inspectJson = parseResponse(inspectResponse);
    if (!inspectJson.contains("State") || !inspectJson["State"].is_object() || !inspectJson["State"].contains("Pid") ||
        !inspectJson["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", containerId));
    }

    const auto pid = inspectJson["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(fmt::format("Container '{}' is not running (PID={})", containerId, pid));
    }

    SPDLOG_DEBUG("DockerClient: container {} has host PID {}", containerId, pid);
    return pid;
}

void DockerClient::updateMemoryLimit(const std::string_view containerId, int64_t memory_bytes) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    nlohmann::json updateConfig = nlohmann::json::object();
    if (memory_bytes >= 0) {
        updateConfig["Memory"] = memory_bytes;
        updateConfig["MemorySwap"] = memory_bytes;
    }

    SPDLOG_DEBUG("Updating memory limit for container {}: {}", containerId, updateConfig.dump());

    const std::string endpoint = fmt::format("/containers/{}/update", containerId);

    if (const auto response = request_(HttpMethod::POST, endpoint, updateConfig.dump()); response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to update memory limit for container '{}': HTTP {}", containerId, response.status));
    }

    SPDLOG_INFO("Updated memory limit for container '{}'", containerId);
}

void DockerClient::updateCpuQuota(const std::string_view containerId, int64_t cpu_quota, int64_t cpu_period) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    nlohmann::json updateConfig = nlohmann::json::object();
    if (cpu_quota >= 0) {
        updateConfig["CpuQuota"] = cpu_quota;
    } else if (cpu_quota == -1) {
        updateConfig["CpuQuota"] = -1;
    }
    if (cpu_period > 0) {
        updateConfig["CpuPeriod"] = cpu_period;
    }

    SPDLOG_DEBUG("Updating CPU quota for container {}: {}", containerId, updateConfig.dump());

    const std::string endpoint = fmt::format("/containers/{}/update", containerId);

    if (const auto response = request_(HttpMethod::POST, endpoint, updateConfig.dump()); response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to update CPU quota for container '{}': HTTP {}", containerId, response.status));
    }

    SPDLOG_INFO("Updated CPU quota for container '{}'", containerId);
}

shared::ContainerStatus DockerClient::getStatus(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }

    SPDLOG_DEBUG("DockerClient: getting status for container {}", containerId);

    const std::string endpoint = fmt::format("/containers/{}/json", containerId);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get status for container '{}': HTTP {}", containerId, response.status));
    }

    if (auto jsonResponse = parseResponse(response);
        jsonResponse.contains("State") && jsonResponse["State"].is_object() &&
        jsonResponse["State"].contains("Status") && jsonResponse["State"]["Status"].is_string()) {
        std::string stateStr = jsonResponse["State"]["Status"].get<std::string>();
        SPDLOG_INFO("DockerClient: container {} status is {}", containerId, stateStr);
        return shared::parseContainerStatus(stateStr);
    }

    throw containers::ContainerEngineParseError(
        fmt::format("Docker inspect response missing State.Status for container '{}'", containerId));
}

std::string DockerClient::getLogs(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching logs for container: {}", containerId);

    constexpr int kLogTailLines = 50;

    // tail=50 avoids dumping the entire container log on every poll; demuxing
    // is done below by stripping the 8-byte frame headers.
    const std::string endpoint =
        fmt::format("/containers/{}/logs?stdout=1&stderr=1&timestamps=0&tail={}", containerId, kLogTailLines);
    const auto response = request_(HttpMethod::GET, endpoint, "");

    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get logs for container '{}': HTTP {}", containerId, response.status));
    }

    SPDLOG_INFO("Fetched logs for container '{}'", containerId);

    // The Docker Engine logs API returns a multiplexed stream where each log
    // entry is prefixed by an 8-byte frame header:
    //   byte 0: stream type (1 = stdout, 2 = stderr)
    //   bytes 1-3: padding (zero)
    //   bytes 4-7: payload length (big-endian uint32)
    // Strip these headers and extract the raw log text.
    std::string result;
    result.reserve(response.body.size());

    size_t i = 0;
    while (i + 8 <= response.body.size()) {
        const uint32_t len = (static_cast<uint32_t>(static_cast<uint8_t>(response.body[i + 4])) << 24) |
                             (static_cast<uint32_t>(static_cast<uint8_t>(response.body[i + 5])) << 16) |
                             (static_cast<uint32_t>(static_cast<uint8_t>(response.body[i + 6])) << 8) |
                             static_cast<uint32_t>(static_cast<uint8_t>(response.body[i + 7]));

        const size_t remaining = response.body.size() - i - 8;
        const size_t clen = (len <= remaining) ? static_cast<size_t>(len) : remaining;
        result.append(response.body, i + 8, clen);
        i += 8 + clen;
    }

    return result;
}

containers::ContainerStats DockerClient::getStats(const std::string_view containerId) {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching container stats for: {}", containerId);

    const std::string endpoint = fmt::format("/containers/{}/stats?stream=false", containerId);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get stats for container '{}': HTTP {}", containerId, response.status));
    }

    auto jsonResponse = parseResponse(response);
    containers::ContainerStats stats;

    // CPU usage (delta between precpu_stats and cpu_stats).
    if (jsonResponse.contains("cpu_stats") && jsonResponse["cpu_stats"].is_object() &&
        jsonResponse["cpu_stats"].contains("cpu_usage") && jsonResponse["cpu_stats"]["cpu_usage"].is_object() &&
        jsonResponse["cpu_stats"]["cpu_usage"].contains("total_usage") &&
        jsonResponse["cpu_stats"]["cpu_usage"]["total_usage"].is_number() &&
        jsonResponse["cpu_stats"].contains("system_cpu_usage") &&
        jsonResponse["cpu_stats"]["system_cpu_usage"].is_number() && jsonResponse.contains("precpu_stats") &&
        jsonResponse["precpu_stats"].is_object() && jsonResponse["precpu_stats"].contains("cpu_usage") &&
        jsonResponse["precpu_stats"]["cpu_usage"].is_object() &&
        jsonResponse["precpu_stats"]["cpu_usage"].contains("total_usage") &&
        jsonResponse["precpu_stats"]["cpu_usage"]["total_usage"].is_number() &&
        jsonResponse["precpu_stats"].contains("system_cpu_usage") &&
        jsonResponse["precpu_stats"]["system_cpu_usage"].is_number() &&
        jsonResponse["cpu_stats"].contains("online_cpus") && jsonResponse["cpu_stats"]["online_cpus"].is_number()) {
        const auto cpu_delta = static_cast<int64_t>(jsonResponse["cpu_stats"]["cpu_usage"]["total_usage"]) -
                               static_cast<int64_t>(jsonResponse["precpu_stats"]["cpu_usage"]["total_usage"]);
        const auto system_cpu_delta = static_cast<int64_t>(jsonResponse["cpu_stats"]["system_cpu_usage"]) -
                                      static_cast<int64_t>(jsonResponse["precpu_stats"]["system_cpu_usage"]);
        const int number_cpus = jsonResponse["cpu_stats"]["online_cpus"];
        if (system_cpu_delta > 0 && cpu_delta > 0) {
            stats.cpu_percent = (static_cast<double>(cpu_delta) / static_cast<double>(system_cpu_delta)) *
                                static_cast<double>(number_cpus) * 100.0;
        }
    }

    // Memory usage.
    if (jsonResponse.contains("memory_stats") && jsonResponse["memory_stats"].is_object() &&
        jsonResponse["memory_stats"].contains("usage") && jsonResponse["memory_stats"]["usage"].is_number()) {
        stats.memory_mb = jsonResponse["memory_stats"]["usage"].get<double>() / (1024.0 * 1024.0);
    }

    // Network I/O — cumulative bytes across all interfaces.
    double rx_bytes = 0;
    double tx_bytes = 0;
    if (jsonResponse.contains("networks") && jsonResponse["networks"].is_object()) {
        for (const auto& [iface, net] : jsonResponse["networks"].items()) {
            if (net.contains("rx_bytes") && net["rx_bytes"].is_number()) {
                rx_bytes += net["rx_bytes"].get<double>();
            }
            if (net.contains("tx_bytes") && net["tx_bytes"].is_number()) {
                tx_bytes += net["tx_bytes"].get<double>();
            }
        }
    }
    // Convert cumulative bytes to B/s using the stored previous values.
    const auto now = std::chrono::steady_clock::now();
    if (prev_net_valid_) {
        const double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_net_timestamp_).count();
        if (dt > 0.001) {
            stats.network_rx_bps = (rx_bytes - prev_net_rx_) / dt;
            stats.network_tx_bps = (tx_bytes - prev_net_tx_) / dt;
        }
    }
    prev_net_rx_ = rx_bytes;
    prev_net_tx_ = tx_bytes;
    prev_net_timestamp_ = now;
    prev_net_valid_ = true;

    SPDLOG_INFO("Fetched stats for container '{}': CPU={}% Mem={:.0f}MB", containerId,
                stats.cpu_percent ? *stats.cpu_percent : -1.0, stats.memory_mb ? *stats.memory_mb : -1.0);
    return stats;
}

std::string DockerClient::getContainerIp(const std::string_view containerId) const {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching IP for container: {}", containerId);

    const std::string endpoint = fmt::format("/containers/{}/json", containerId);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", containerId, response.status));
    }

    if (auto jsonResponse = parseResponse(response);
        jsonResponse.contains("NetworkSettings") && jsonResponse["NetworkSettings"].contains("Networks")) {
        auto& networks = jsonResponse["NetworkSettings"]["Networks"];
        if (networks.is_object() && !networks.empty()) {
            auto firstNetwork = networks.begin().value();
            if (firstNetwork.contains("IPAddress") && firstNetwork["IPAddress"].is_string()) {
                std::string ip = firstNetwork["IPAddress"].get<std::string>();
                if (!ip.empty()) {
                    SPDLOG_INFO("Fetched IP {} for container '{}'", ip, containerId);
                    return ip;
                }
            }
        }
    }

    throw containers::ContainerEngineParseError(
        fmt::format("Docker inspect response missing IP for container '{}'", containerId));
}

SystemInfo DockerClient::getSystemInfo() const {
    const auto response = request_(HttpMethod::GET, "/info", "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(fmt::format("Docker API error {} on GET /info", response.status));
    }
    auto jsonResponse = parseResponse(response);

    SystemInfo info;
    if (jsonResponse.contains("MemTotal") && jsonResponse["MemTotal"].is_number()) {
        info.memTotal = jsonResponse["MemTotal"].get<int64_t>();
    }
    return info;
}

nlohmann::json DockerClient::parseResponse(const HttpResponse& response) const {
    if (response.body.empty()) {
        SPDLOG_ERROR("Docker API response body is empty");
        throw containers::ContainerEngineParseError("Docker API response body is empty");
    }

    auto jsonResponse = nlohmann::json::parse(response.body, nullptr, false);
    if (jsonResponse.is_discarded()) {
        SPDLOG_ERROR("Docker API response could not be parsed as JSON");
        throw containers::ContainerEngineParseError("Failed to parse Docker API response");
    }

    if (jsonResponse.is_object() && jsonResponse.contains("error")) {
        SPDLOG_ERROR("Docker API error: {}", jsonResponse["error"].get<std::string>());
        for (const auto& [key, value] : jsonResponse.items()) {
            SPDLOG_ERROR("  {}: {}", key, value.dump());
        }
        throw containers::ContainerEngineApiError(
            fmt::format("Docker API error: {}", jsonResponse["error"].get<std::string>()));
    }

    return jsonResponse;
}

}  // namespace chaos::orchestrator::containers::internal
