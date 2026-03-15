#include <fmt/format.h>
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <containers/internal/DockerClient.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace chaos::orchestrator::containers::internal {

DockerClient::DockerClient(RequestFn requestFn) : request_(std::move(requestFn)) {}

std::vector<chaos::orchestrator::containers::Container> DockerClient::listContainers() {
    spdlog::debug("DockerClient: listing containers");
    const auto response = request_(HttpMethod::GET, "/containers/json");
    if (response.status != 200) {
        spdlog::warn("Docker API returned status {}", response.status);
        throw std::runtime_error(fmt::format("Docker API returned status {}", response.status));
    }

    auto jsonResponse = validateResponse(response);

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

    spdlog::info("DockerClient: {} containers found", containers.size());
    return containers;
}

void DockerClient::stopContainer(const std::string_view containerId) {
    if (containerId.empty()) {
        throw std::runtime_error("Container ID cannot be empty");
    }
    spdlog::debug("DockerClient: stopping container {}", containerId);

    constexpr int stopTimeoutSeconds = 5;
    const auto response =
        request_(HttpMethod::POST, fmt::format("/containers/{}/stop?t={}", containerId, stopTimeoutSeconds));
    if (response.status == 304) {
        spdlog::warn("DockerClient: container {} is already stopped", containerId);
    } else if (response.status != 204) {
        spdlog::warn("Docker API returned status {}", response.status);
        throw std::runtime_error(fmt::format("Docker API returned status {}", response.status));
    }

    spdlog::info("DockerClient: container {} stopped successfully", containerId);
}

void DockerClient::killContainer(const std::string_view containerId) {
    if (containerId.empty()) {
        throw std::runtime_error("Container ID cannot be empty");
    }
    spdlog::debug("DockerClient: killing container {}", containerId);

    const auto response = request_(HttpMethod::POST, fmt::format("/containers/{}/kill", containerId));
    if (response.status != 204) {
        spdlog::warn("Docker API returned status {}", response.status);
        throw std::runtime_error(fmt::format("Docker API returned status {}", response.status));
    }

    if (!response.body.empty()) {
        validateResponse(response);
    }

    spdlog::info("DockerClient: container {} killed successfully", containerId);
}

std::unique_ptr<chaos::orchestrator::containers::IContainerEngine> DockerClient::create(const std::string& socketPath) {
    spdlog::info("DockerClient: using socket {}", socketPath);
    auto client = std::make_shared<httplib::Client>(socketPath);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(30);

    auto requestFn = [client](HttpMethod method, std::string_view endpoint) {
        spdlog::debug("DockerClient: request {} {}", method == HttpMethod::GET ? "GET" : "POST", endpoint);
        httplib::Result response;
        std::string endpointStr(endpoint);
        switch (method) {
            case HttpMethod::GET:
                response = client->Get(endpointStr);
                break;
            case HttpMethod::POST:
                response = client->Post(endpointStr);
                break;
        }

        if (!response) {
            spdlog::error("DockerClient: connection to Docker socket failed");
            throw std::runtime_error("Failed to connect to Docker socket");
        }
        return HttpResponse{response->status, response->body};
    };

    return std::make_unique<DockerClient>(std::move(requestFn));
}

nlohmann::json DockerClient::validateResponse(const HttpResponse& response) const {
    if (response.body.empty()) {
        spdlog::error("Docker API response body is empty");
        throw std::runtime_error("Docker API response body is empty");
    }

    auto jsonResponse = nlohmann::json::parse(response.body, nullptr, false);
    if (jsonResponse.is_discarded()) {
        spdlog::error("Docker API response could not be parsed as JSON");
        throw std::runtime_error("Failed to parse Docker API response");
    }

    if (jsonResponse.is_object() && jsonResponse.contains("error")) {
        spdlog::error("Docker API error: {}", jsonResponse["error"].get<std::string>());
        for (const auto& [key, value] : jsonResponse.items()) {
            spdlog::error("  {}: {}", key, value.dump());
        }
        throw std::runtime_error(fmt::format("Docker API error: {}", jsonResponse["error"].get<std::string>()));
    }

    return jsonResponse;
}

}  // namespace chaos::orchestrator::containers::internal
