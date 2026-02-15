#include <httplib.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace chaos::adapters::infra::docker {

/**
 * @brief Construct the adapter with an injected request function.
 * @param requestFn Function that performs API requests.
 */
DockerClientAdapter::DockerClientAdapter(RequestFn requestFn) : request_(std::move(requestFn)) {}

/**
 * @brief List containers from the Docker Engine API.
 * @return Vector of container summaries.
 * @throws std::runtime_error On unexpected response formats.
 */
std::vector<chaos::domain::Container> DockerClientAdapter::listContainers() {
    spdlog::debug("DockerClientAdapter: listing containers");
    const auto response = request_(HttpMethod::GET, "/containers/json");
    if (response.status != 200) {
        spdlog::warn("Docker API returned status {}", response.status);
        throw std::runtime_error("Docker API returned status " + std::to_string(response.status));
    }

    auto json_response = validateResponse(response);

    std::vector<chaos::domain::Container> containers;
    containers.reserve(json_response.size());

    for (const auto& item : json_response) {
        chaos::domain::Container container;
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

    spdlog::info("DockerClientAdapter: {} containers found", containers.size());
    return containers;
}

/**
 * @brief Kill a container by ID.
 * @param containerId Docker container ID.
 * @throws std::runtime_error On API errors.
 */
void DockerClientAdapter::killContainer(const std::string& containerId) {
    spdlog::debug("DockerClientAdapter: killing container {}", containerId);

    const auto response = request_(HttpMethod::POST, "/containers/" + containerId + "/kill");
    if (response.status != 204) {
        spdlog::warn("Docker API returned status {}", response.status);
        throw std::runtime_error("Docker API returned status " + std::to_string(response.status));
    }

    if (!response.body.empty()) {
        auto json_response = validateResponse(response);
    }

    spdlog::info("DockerClientAdapter: container {} killed successfully", containerId);
}

/**
 * @brief Create an adapter backed by a Unix socket client.
 * @param socket_path Path to the Docker Engine socket.
 * @return A container engine instance.
 * @throws std::runtime_error On connection or parsing errors.
 */
std::unique_ptr<chaos::domain::ports::IContainerEngine> DockerClientAdapter::create(const std::string& socket_path) {
    spdlog::info("DockerClientAdapter: using socket {}", socket_path);
    auto client = std::make_shared<httplib::Client>(socket_path);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(10);

    auto requestFn = [client](HttpMethod method, const std::string& endpoint) -> HttpResponse {
        spdlog::debug("DockerClientAdapter: request {} {}", method == HttpMethod::GET ? "GET" : "POST", endpoint);
        httplib::Result response;
        switch (method) {
            case HttpMethod::GET:
                response = client->Get(endpoint);
                break;
            case HttpMethod::POST:
                response = client->Post(endpoint);
                break;
        }

        if (!response) {
            spdlog::error("DockerClientAdapter: connection to Docker socket failed");
            throw std::runtime_error("Failed to connect to Docker socket");
        }
        return HttpResponse{response->status, response->body};
    };

    return std::make_unique<DockerClientAdapter>(std::move(requestFn));
}

/**
 * @brief Validate the HTTP response from Docker API.
 * @param response The HTTP response to validate.
 * @throws std::runtime_error If the response indicates an error or is malformed.
 */
nlohmann::json DockerClientAdapter::validateResponse(const HttpResponse& response) {
    if (response.body.empty()) {
        spdlog::error("Docker API response body is empty");
        throw std::runtime_error("Docker API response body is empty");
    }
    auto json_response = nlohmann::json::parse(response.body, nullptr, false);
    if (json_response.is_discarded()) {
        spdlog::error("Docker API response could not be parsed as JSON");
        throw std::runtime_error("Failed to parse Docker API response");
    }
    if (json_response.is_object() && json_response.contains("error")) {
        spdlog::error("Docker API error: {}", json_response["error"].get<std::string>());
        for (const auto& [key, value] : json_response.items()) {
            spdlog::error("  {}: {}", key, value.dump());
        }
        throw std::runtime_error("Docker API error: " + json_response["error"].get<std::string>());
    }
    return json_response;
}

}  // namespace chaos::adapters::infra::docker
