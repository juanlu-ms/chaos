#include <httplib.h>
#include <sys/socket.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace chaos::adapters::infra::docker {

DockerClientAdapter::DockerClientAdapter(RequestFn requestFn) : request_(std::move(requestFn)) {}

std::vector<chaos::domain::Container> DockerClientAdapter::listContainers() {
    auto json_response = request_(HttpMethod::GET, "/containers/json");
    if (!json_response.is_array()) {
        throw std::runtime_error("Unexpected Docker API response format");
    }

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

    return containers;
}

std::unique_ptr<chaos::domain::ports::IContainerEngine> DockerClientAdapter::create(const std::string& socket_path) {
    auto client = std::make_shared<httplib::Client>(socket_path);
    client->set_address_family(AF_UNIX);
    client->set_connection_timeout(5);
    client->set_read_timeout(10);

    auto requestFn = [client](HttpMethod method, const std::string& endpoint) -> nlohmann::json {
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
            throw std::runtime_error("Failed to connect to Docker socket");
        }
        if (response->status != 200) {
            throw std::runtime_error("Docker API returned status " + std::to_string(response->status));
        }

        auto json = nlohmann::json::parse(response->body, nullptr, false);
        if (json.is_discarded()) {
            throw std::runtime_error("Failed to parse Docker API response");
        }
        return json;
    };

    return std::make_unique<DockerClientAdapter>(std::move(requestFn));
}

}  // namespace chaos::adapters::infra::docker
