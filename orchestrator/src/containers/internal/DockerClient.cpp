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

#include "containers/ContainerStatus.hpp"
#include "containers/IContainerEngine.hpp"
#include "containers/internal/DockerClientDetail.hpp"

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

std::string createTarArchive(const std::string_view dockerfile_path) {
    const auto context_dir = std::filesystem::path(dockerfile_path).parent_path();
    if (!std::filesystem::exists(context_dir)) {
        throw std::runtime_error(fmt::format("Build context directory does not exist: {}", context_dir.string()));
    }

    ArchiveWritePtr arch(archive_write_new());
    archive_write_set_format_ustar(arch.get());

    std::string tar_data;
    archive_write_open(
        arch.get(), &tar_data, nullptr,
        [](archive*, void* client, const void* buf, size_t len) {
            static_cast<std::string*>(client)->append(static_cast<const char*>(buf), len);
            return static_cast<la_ssize_t>(len);
        },
        nullptr);

    const auto canonical_context = std::filesystem::weakly_canonical(context_dir);
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             context_dir, std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_symlink()) {
            SPDLOG_INFO("Skipping symlink in build context: {}", entry.path().string());
            continue;
        }

        const auto& path = entry.path();
        if (!chaos::orchestrator::containers::detail::isWithinBuildContext(path, canonical_context)) {
            SPDLOG_INFO("Path escapes build context, skipping: {}", path.string());
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const auto relative = std::filesystem::relative(path, context_dir).string();
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

    return tar_data;
}

void parseBuildResponse(const std::string_view body) {
    std::string_view remaining = body;
    while (!remaining.empty()) {
        const auto newline_pos = remaining.find('\n');

        if (const std::string_view line =
                (newline_pos == std::string_view::npos) ? remaining : remaining.substr(0, newline_pos);
            !line.empty()) {
            auto json = nlohmann::json::parse(line, nullptr, false);
            if (!json.is_discarded() && json.is_object() && json.contains("error")) {
                throw chaos::orchestrator::containers::ContainerEngineApiError(
                    fmt::format("Docker build error: {}", json["error"].get<std::string>()));
            }
        }

        if (newline_pos == std::string_view::npos) {
            break;
        }
        remaining = remaining.substr(newline_pos + 1);
    }
}

}  // namespace

namespace chaos::orchestrator::containers::detail {

bool isWithinBuildContext(const std::filesystem::path& path, const std::filesystem::path& context_root) {
    auto canonical = std::filesystem::weakly_canonical(path);
    return canonical.string().starts_with(context_root.string());
}

}  // namespace chaos::orchestrator::containers::detail

namespace chaos::orchestrator::containers {

DockerClient::DockerClient(RequestFn request_fn) : request_(std::move(request_fn)) {}

std::shared_ptr<containers::IContainerEngine> DockerClient::create(const std::string& socket_path) {
    SPDLOG_INFO("DockerClient: using socket {}", socket_path);
    auto client = std::make_shared<httplib::Client>(socket_path);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(30);

    auto request_fn = [client](HttpMethod method, std::string_view endpoint, std::string_view body) {
        SPDLOG_DEBUG("DockerClient: request {} {}",
                     method == HttpMethod::GET        ? "GET"
                     : method == HttpMethod::POST     ? "POST"
                     : method == HttpMethod::POST_TAR ? "POST_TAR"
                                                      : "REMOVE",
                     endpoint);
        httplib::Result response;
        std::string endpoint_str(endpoint);
        switch (method) {
            using enum chaos::orchestrator::containers::HttpMethod;
            case GET:
                response = client->Get(endpoint_str);
                break;
            case POST:
                if (body.empty()) {
                    response = client->Post(endpoint_str);
                } else {
                    response = client->Post(endpoint_str, std::string(body), "application/json");
                }
                break;
            case POST_TAR:
                response = client->Post(endpoint_str, std::string(body), "application/x-tar");
                break;
            case REMOVE:
                response = client->Delete(endpoint_str);
                break;
        }

        if (!response) {
            SPDLOG_ERROR("DockerClient: connection to Docker socket failed");
            throw containers::ContainerEngineTransportError("Failed to connect to Docker socket");
        }
        return HttpResponse{.status = response->status, .body = response->body};
    };

    return std::make_shared<DockerClient>(std::move(request_fn));
}

std::vector<containers::Container> DockerClient::listContainers() const {
    SPDLOG_DEBUG("DockerClient: listing containers");
    const auto response = request_(HttpMethod::GET, "/containers/json", "");
    if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    auto json_response = parseResponse(response);

    std::vector<containers::Container> containers;
    containers.reserve(json_response.size());

    for (const auto& item : json_response) {
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

void DockerClient::buildImage(const std::string_view image_name, const std::string_view dockerfile_path) const {
    if (image_name.empty()) {
        throw std::invalid_argument("Image name cannot be empty");
    }
    if (dockerfile_path.empty()) {
        throw std::invalid_argument("Dockerfile path cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: building image {} from {}", image_name, dockerfile_path);

    const auto tar_archive = createTarArchive(dockerfile_path);
    const auto endpoint = fmt::format("/build?t={}", image_name);
    const auto response = request_(HttpMethod::POST_TAR, endpoint, tar_archive);

    if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {} while building image {}: {}", response.status, image_name,
                     response.body);
        throw containers::ContainerEngineApiError(
            fmt::format("Docker API returned status {} while building image {}", response.status, image_name));
    }

    parseBuildResponse(response.body);

    SPDLOG_INFO("DockerClient: image {} built successfully", image_name);
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

    auto json_response = parseResponse(response);
    if (!json_response.contains("Id") || !json_response["Id"].is_string()) {
        throw containers::ContainerEngineParseError("Docker create response missing Id field");
    }

    SPDLOG_INFO("DockerClient: container created with id {}", json_response["Id"].get<std::string>());
    return json_response["Id"].get<std::string>();
}

void DockerClient::startContainer(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: starting container {}", container_id);

    if (const auto response = request_(HttpMethod::POST, fmt::format("/containers/{}/start", container_id), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} started successfully", container_id);
}

void DockerClient::stopContainer(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: stopping container {}", container_id);

    constexpr int stop_timeout_seconds = 5;

    if (const auto response =
            request_(HttpMethod::POST, fmt::format("/containers/{}/stop?t={}", container_id, stop_timeout_seconds), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} stopped successfully", container_id);
}

void DockerClient::killContainer(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: killing container {}", container_id);

    const auto response = request_(HttpMethod::POST, fmt::format("/containers/{}/kill", container_id), "");
    if (response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    if (!response.body.empty()) {
        parseResponse(response);
    }

    SPDLOG_INFO("DockerClient: container {} killed successfully", container_id);
}

void DockerClient::removeContainer(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: removing container {}", container_id);

    if (const auto response =
            request_(HttpMethod::REMOVE, fmt::format("/containers/{}?force=true&v=true", container_id), "");
        response.status != 204) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    SPDLOG_INFO("DockerClient: container {} removed successfully", container_id);
}

std::string DockerClient::exec(const std::string_view container_id, const std::string_view command) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    if (command.empty()) {
        throw std::invalid_argument("Command cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: executing command {} in container {}", command, container_id);

    nlohmann::json exec_config = {
        {"AttachStdin", false}, {"AttachStdout", true},
        {"AttachStderr", true}, {"Tty", false},
        {"Privileged", false},  {"Cmd", nlohmann::json::array({"/bin/sh", "-lc", std::string(command)})},
    };

    const auto create_response =
        request_(HttpMethod::POST, fmt::format("/containers/{}/exec", container_id), exec_config.dump());
    if (create_response.status != 201) {
        SPDLOG_ERROR("Docker exec create failed with status {}: {}", create_response.status, create_response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", create_response.status));
    }

    auto create_json = parseResponse(create_response);
    if (!create_json.contains("Id") || !create_json["Id"].is_string()) {
        throw containers::ContainerEngineParseError("Docker exec create response missing Id");
    }
    const auto exec_id = create_json["Id"].get<std::string>();

    const nlohmann::json start_config = {
        {"Detach", false},
        {"Tty", false},
    };

    const auto start_response = request_(HttpMethod::POST, fmt::format("/exec/{}/start", exec_id), start_config.dump());
    if (start_response.status != 200) {
        SPDLOG_ERROR("Docker exec start failed with status {}: {}", start_response.status, start_response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", start_response.status));
    }

    const auto exit_response = request_(HttpMethod::GET, fmt::format("/exec/{}/json", exec_id), "");
    if (exit_response.status != 200) {
        SPDLOG_ERROR("Docker exec inspect failed with status {}: {}", exit_response.status, exit_response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", exit_response.status));
    }

    if (const auto exit_json = parseResponse(exit_response);
        exit_json.contains("ExitCode") && exit_json["ExitCode"].get<int>() != 0) {
        throw containers::ContainerEngineError(fmt::format("Command '{}' in container '{}' exited with code {}",
                                                           command, container_id, exit_json["ExitCode"].get<int>()));
    }

    SPDLOG_INFO("DockerClient: command executed successfully in container {}", container_id);
    return start_response.body;
}

std::string DockerClient::execInNetNs(const std::string_view container_id, const std::string_view command) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    if (command.empty()) {
        throw std::invalid_argument("Command cannot be empty");
    }

    SPDLOG_DEBUG("DockerClient: executing in netns of container {}: {}", container_id, command);

    const auto inspect_response = request_(HttpMethod::GET, fmt::format("/containers/{}/json", container_id), "");
    if (inspect_response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", container_id, inspect_response.status));
    }

    auto inspect_json = parseResponse(inspect_response);
    if (!inspect_json.contains("State") || !inspect_json["State"].is_object() ||
        !inspect_json["State"].contains("Pid") || !inspect_json["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", container_id));
    }

    const auto pid = inspect_json["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(
            fmt::format("Container '{}' is not running (PID={})", container_id, pid));
    }

    const std::string nsenter_cmd = fmt::format("nsenter -t {} -n {} 2>&1", pid, command);

    auto pipe = std::unique_ptr<FILE, decltype([](FILE* f) noexcept {
                                    if (f) {
                                        pclose(f);
                                    }
                                })>(popen(nsenter_cmd.c_str(), "r"));
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
            fmt::format("Network command in container '{}' failed (exit {}): {}", container_id, status, result));
    }

    SPDLOG_INFO("DockerClient: network command executed in container {} netns", container_id);
    return result;
}

int DockerClient::getContainerNetnsFd(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }
    SPDLOG_DEBUG("DockerClient: getting netns fd for container {}", container_id);

    const auto inspect_response = request_(HttpMethod::GET, fmt::format("/containers/{}/json", container_id), "");
    if (inspect_response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", container_id, inspect_response.status));
    }

    auto inspect_json = parseResponse(inspect_response);
    if (!inspect_json.contains("State") || !inspect_json["State"].is_object() ||
        !inspect_json["State"].contains("Pid") || !inspect_json["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", container_id));
    }

    const auto pid = inspect_json["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(
            fmt::format("Container '{}' is not running (PID={})", container_id, pid));
    }

    const std::string ns_path = fmt::format("/proc/{}/ns/net", pid);
    const int fd = open(ns_path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw containers::ContainerEngineError(
            fmt::format("Failed to open network namespace for container '{}': {}", container_id, std::strerror(errno)));
    }
    SPDLOG_DEBUG("DockerClient: obtained netns fd {} for container {}", fd, container_id);

    return fd;
}

int DockerClient::getContainerPid(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }

    const auto inspect_response = request_(HttpMethod::GET, fmt::format("/containers/{}/json", container_id), "");
    if (inspect_response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", container_id, inspect_response.status));
    }

    auto inspect_json = parseResponse(inspect_response);
    if (!inspect_json.contains("State") || !inspect_json["State"].is_object() ||
        !inspect_json["State"].contains("Pid") || !inspect_json["State"]["Pid"].is_number_integer()) {
        throw containers::ContainerEngineParseError(
            fmt::format("Failed to get State.Pid for container '{}'", container_id));
    }

    const auto pid = inspect_json["State"]["Pid"].get<int>();
    if (pid <= 0) {
        throw containers::ContainerEngineError(
            fmt::format("Container '{}' is not running (PID={})", container_id, pid));
    }

    SPDLOG_DEBUG("DockerClient: container {} has host PID {}", container_id, pid);
    return pid;
}

void DockerClient::updateMemoryLimit(const std::string_view container_id, int64_t memory_bytes) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    nlohmann::json update_config = nlohmann::json::object();
    if (memory_bytes >= 0) {
        update_config["Memory"] = memory_bytes;
        update_config["MemorySwap"] = memory_bytes;
    }

    SPDLOG_DEBUG("Updating memory limit for container {}: {}", container_id, update_config.dump());

    const std::string endpoint = fmt::format("/containers/{}/update", container_id);

    if (const auto response = request_(HttpMethod::POST, endpoint, update_config.dump()); response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to update memory limit for container '{}': HTTP {}", container_id, response.status));
    }

    SPDLOG_INFO("Updated memory limit for container '{}'", container_id);
}

void DockerClient::updateCpuQuota(const std::string_view container_id, int64_t cpu_quota, int64_t cpu_period) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    nlohmann::json update_config = nlohmann::json::object();
    if (cpu_quota >= 0) {
        update_config["CpuQuota"] = cpu_quota;
    } else if (cpu_quota == -1) {
        update_config["CpuQuota"] = -1;
    }
    if (cpu_period > 0) {
        update_config["CpuPeriod"] = cpu_period;
    }

    SPDLOG_DEBUG("Updating CPU quota for container {}: {}", container_id, update_config.dump());

    const std::string endpoint = fmt::format("/containers/{}/update", container_id);

    if (const auto response = request_(HttpMethod::POST, endpoint, update_config.dump()); response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to update CPU quota for container '{}': HTTP {}", container_id, response.status));
    }

    SPDLOG_INFO("Updated CPU quota for container '{}'", container_id);
}

containers::ContainerStatus DockerClient::getStatus(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID cannot be empty");
    }

    SPDLOG_DEBUG("DockerClient: getting status for container {}", container_id);

    const std::string endpoint = fmt::format("/containers/{}/json", container_id);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get status for container '{}': HTTP {}", container_id, response.status));
    }

    if (auto json_response = parseResponse(response);
        json_response.contains("State") && json_response["State"].is_object() &&
        json_response["State"].contains("Status") && json_response["State"]["Status"].is_string()) {
        std::string state_str = json_response["State"]["Status"].get<std::string>();
        SPDLOG_DEBUG("DockerClient: container {} status is {}", container_id, state_str);
        return parseContainerStatus(state_str);
    }

    throw containers::ContainerEngineParseError(
        fmt::format("Docker inspect response missing State.Status for container '{}'", container_id));
}

std::string DockerClient::getLogs(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching logs for container: {}", container_id);

    constexpr int kLogTailLines = 50;

    // tail=50 avoids dumping the entire container log on every poll; demuxing
    // is done below by stripping the 8-byte frame headers.
    const std::string endpoint =
        fmt::format("/containers/{}/logs?stdout=1&stderr=1&timestamps=0&tail={}", container_id, kLogTailLines);
    const auto response = request_(HttpMethod::GET, endpoint, "");

    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get logs for container '{}': HTTP {}", container_id, response.status));
    }

    SPDLOG_DEBUG("Fetched logs for container '{}'", container_id);

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

containers::ContainerStats DockerClient::getStats(const std::string_view container_id) {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching container stats for: {}", container_id);

    const std::string endpoint = fmt::format("/containers/{}/stats?stream=false", container_id);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get stats for container '{}': HTTP {}", container_id, response.status));
    }

    auto json_response = parseResponse(response);
    containers::ContainerStats stats;

    stats.cpu_percent = parseCpuDelta(json_response);
    stats.memory_mb = parseMemoryFromStats(json_response);
    auto [rx, tx] = parseNetworkFromStats(json_response);
    stats.network_rx_bps = rx;
    stats.network_tx_bps = tx;

    SPDLOG_DEBUG("Fetched stats for container '{}': CPU={}% Mem={:.0f}MB", container_id,
                 stats.cpu_percent ? *stats.cpu_percent : -1.0, stats.memory_mb ? *stats.memory_mb : -1.0);
    return stats;
}

std::optional<double> DockerClient::parseCpuDelta(const nlohmann::json& json) const {
    if (json.contains("cpu_stats") && json["cpu_stats"].is_object() && json["cpu_stats"].contains("cpu_usage") &&
        json["cpu_stats"]["cpu_usage"].is_object() && json["cpu_stats"]["cpu_usage"].contains("total_usage") &&
        json["cpu_stats"]["cpu_usage"]["total_usage"].is_number() && json["cpu_stats"].contains("system_cpu_usage") &&
        json["cpu_stats"]["system_cpu_usage"].is_number() && json.contains("precpu_stats") &&
        json["precpu_stats"].is_object() && json["precpu_stats"].contains("cpu_usage") &&
        json["precpu_stats"]["cpu_usage"].is_object() && json["precpu_stats"]["cpu_usage"].contains("total_usage") &&
        json["precpu_stats"]["cpu_usage"]["total_usage"].is_number() &&
        json["precpu_stats"].contains("system_cpu_usage") && json["precpu_stats"]["system_cpu_usage"].is_number() &&
        json["cpu_stats"].contains("online_cpus") && json["cpu_stats"]["online_cpus"].is_number()) {
        const auto cpu_delta = static_cast<int64_t>(json["cpu_stats"]["cpu_usage"]["total_usage"]) -
                               static_cast<int64_t>(json["precpu_stats"]["cpu_usage"]["total_usage"]);
        const auto system_cpu_delta = static_cast<int64_t>(json["cpu_stats"]["system_cpu_usage"]) -
                                      static_cast<int64_t>(json["precpu_stats"]["system_cpu_usage"]);
        const int number_cpus = json["cpu_stats"]["online_cpus"];
        if (system_cpu_delta > 0 && cpu_delta > 0) {
            return (static_cast<double>(cpu_delta) / static_cast<double>(system_cpu_delta)) *
                   static_cast<double>(number_cpus) * 100.0;
        }
    }
    return std::nullopt;
}

std::optional<double> DockerClient::parseMemoryFromStats(const nlohmann::json& json) const {
    if (json.contains("memory_stats") && json["memory_stats"].is_object() && json["memory_stats"].contains("usage") &&
        json["memory_stats"]["usage"].is_number()) {
        return json["memory_stats"]["usage"].get<double>() / (1024.0 * 1024.0);
    }
    return std::nullopt;
}

std::pair<double, double> DockerClient::parseNetworkFromStats(const nlohmann::json& json) {
    double rx_bytes = 0;
    double tx_bytes = 0;
    if (json.contains("networks") && json["networks"].is_object()) {
        for (const auto& [iface, net] : json["networks"].items()) {
            if (net.contains("rx_bytes") && net["rx_bytes"].is_number()) {
                rx_bytes += net["rx_bytes"].get<double>();
            }
            if (net.contains("tx_bytes") && net["tx_bytes"].is_number()) {
                tx_bytes += net["tx_bytes"].get<double>();
            }
        }
    }
    double rx_bps = 0.0;
    double tx_bps = 0.0;
    const auto now = std::chrono::steady_clock::now();
    if (prev_net_valid_) {
        const double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - prev_net_timestamp_).count();
        if (dt > 0.001) {
            rx_bps = (rx_bytes - prev_net_rx_) / dt;
            tx_bps = (tx_bytes - prev_net_tx_) / dt;
        }
    }
    prev_net_rx_ = rx_bytes;
    prev_net_tx_ = tx_bytes;
    prev_net_timestamp_ = now;
    prev_net_valid_ = true;
    return {rx_bps, tx_bps};
}

std::string DockerClient::getContainerIp(const std::string_view container_id) const {
    if (container_id.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching IP for container: {}", container_id);

    const std::string endpoint = fmt::format("/containers/{}/json", container_id);
    const auto response = request_(HttpMethod::GET, endpoint, "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to inspect container '{}': HTTP {}", container_id, response.status));
    }

    if (auto json_response = parseResponse(response);
        json_response.contains("NetworkSettings") && json_response["NetworkSettings"].contains("Networks")) {
        auto& networks = json_response["NetworkSettings"]["Networks"];
        if (networks.is_object() && !networks.empty()) {
            for (const auto& [name, net] : networks.items()) {
                if (net.contains("IPAddress") && net["IPAddress"].is_string()) {
                    std::string ip = net["IPAddress"].get<std::string>();
                    if (!ip.empty()) {
                        SPDLOG_INFO("Fetched IP {} for container '{}' from network '{}'", ip, container_id, name);
                        return ip;
                    }
                }
            }
        }
    }

    throw containers::ContainerEngineParseError(
        fmt::format("Docker inspect response missing IP for container '{}'", container_id));
}

SystemInfo DockerClient::getSystemInfo() const {
    const auto response = request_(HttpMethod::GET, "/info", "");
    if (response.status != 200) {
        throw containers::ContainerEngineApiError(fmt::format("Docker API error {} on GET /info", response.status));
    }
    auto json_response = parseResponse(response);

    SystemInfo info;
    if (json_response.contains("MemTotal") && json_response["MemTotal"].is_number()) {
        info.mem_total = json_response["MemTotal"].get<int64_t>();
    }
    return info;
}

nlohmann::json DockerClient::parseResponse(const HttpResponse& response) const {
    if (response.body.empty()) {
        SPDLOG_ERROR("Docker API response body is empty");
        throw containers::ContainerEngineParseError("Docker API response body is empty");
    }

    auto json_response = nlohmann::json::parse(response.body, nullptr, false);
    if (json_response.is_discarded()) {
        SPDLOG_ERROR("Docker API response could not be parsed as JSON");
        throw containers::ContainerEngineParseError("Failed to parse Docker API response");
    }

    if (json_response.is_object() && json_response.contains("error")) {
        SPDLOG_ERROR("Docker API error: {}", json_response["error"].get<std::string>());
        for (const auto& [key, value] : json_response.items()) {
            SPDLOG_ERROR("  {}: {}", key, value.dump());
        }
        throw containers::ContainerEngineApiError(
            fmt::format("Docker API error: {}", json_response["error"].get<std::string>()));
    }

    return json_response;
}

}  // namespace chaos::orchestrator::containers
