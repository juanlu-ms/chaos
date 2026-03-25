#include "DockerClient.hpp"

#include <fmt/format.h>
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace chaos::orchestrator::containers::internal {

DockerClient::DockerClient(RequestFn requestFn) : request_(std::move(requestFn)) {}

std::vector<chaos::orchestrator::containers::Container> DockerClient::listContainers() {
    SPDLOG_DEBUG("DockerClient: listing containers");
    const auto response = request_(HttpMethod::GET, "/containers/json", "");
    if (response.status != 200) {
        SPDLOG_ERROR("Docker API returned status {}: {}", response.status, response.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", response.status));
    }

    auto jsonResponse = parseResponse(response);

    std::vector<chaos::orchestrator::containers::Container> containers;
    containers.reserve(jsonResponse.size());

    for (const auto& item : jsonResponse) {
        chaos::orchestrator::containers::Container container;
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

void DockerClient::createContainer(const std::string_view image, const std::vector<std::string>& options) {
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
}

void DockerClient::startContainer(const std::string_view containerId) {
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

void DockerClient::stopContainer(const std::string_view containerId) {
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

void DockerClient::killContainer(const std::string_view containerId) {
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

std::string DockerClient::exec(const std::string_view containerId, const std::string_view command) {
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
        throw containers::ContainerEngineApiError(
            fmt::format("Docker API returned status {}", createResponse.status));
    }

    auto createJson = parseResponse(createResponse);
    if (!createJson.contains("Id") || !createJson["Id"].is_string()) {
        throw containers::ContainerEngineParseError("Docker exec create response missing Id");
    }
    const auto execId = createJson["Id"].get<std::string>();

    nlohmann::json startConfig = {
        {"Detach", false},
        {"Tty", false},
    };

    const auto startResponse = request_(HttpMethod::POST, fmt::format("/exec/{}/start", execId), startConfig.dump());
    if (startResponse.status != 200) {
        SPDLOG_ERROR("Docker exec start failed with status {}: {}", startResponse.status, startResponse.body);
        throw containers::ContainerEngineApiError(fmt::format("Docker API returned status {}", startResponse.status));
    }

    SPDLOG_INFO("DockerClient: command executed successfully in container {}", containerId);
    return startResponse.body;
}

std::shared_ptr<chaos::orchestrator::containers::IContainerEngine> DockerClient::create(const std::string& socketPath) {
    SPDLOG_INFO("DockerClient: using socket {}", socketPath);
    auto client = std::make_shared<httplib::Client>(socketPath);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(30);

    auto requestFn = [client](HttpMethod method, std::string_view endpoint, std::string_view body) {
        SPDLOG_DEBUG("DockerClient: request {} {}", method == HttpMethod::GET ? "GET" : "POST", endpoint);
        httplib::Result response;
        std::string endpointStr(endpoint);
        switch (method) {
            case HttpMethod::GET:
                response = client->Get(endpointStr);
                break;
            case HttpMethod::POST:
                if (body.empty()) {
                    response = client->Post(endpointStr);
                } else {
                    response = client->Post(endpointStr, std::string(body), "application/json");
                }
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

std::string DockerClient::getLogs(const std::string_view containerId) {
    if (containerId.empty()) {
        throw std::invalid_argument("Container ID must not be empty");
    }

    SPDLOG_DEBUG("Fetching logs for container: {}", containerId);

    const std::string endpoint = fmt::format("/containers/{}/logs?stdout=1&stderr=1&timestamps=0", containerId);
    const auto response = request_(HttpMethod::GET, endpoint, "");

    if (response.status != 200) {
        throw containers::ContainerEngineApiError(
            fmt::format("Failed to get logs for container '{}': HTTP {}", containerId, response.status));
    }

    SPDLOG_INFO("Fetched logs for container '{}'", containerId);
    return response.body;
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
