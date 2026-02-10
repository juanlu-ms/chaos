#include <httplib.h>
#include <sys/socket.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace kaos::adapters::infra::docker {

DockerClientAdapter::DockerClientAdapter(std::string socket_path) : socket_path_(std::move(socket_path)) {}

nlohmann::json DockerClientAdapter::request(std::string method, const std::string& endpoint) {
    httplib::Client client(socket_path_);
    client.set_address_family(AF_UNIX);

    httplib::Result response;
    if (method == "GET") {
        response = client.Get(endpoint.c_str());
        if (!response) {
            throw std::runtime_error("Failed to connect to Docker socket");
        }
    } else if (method == "POST") {
        response = client.Post(endpoint.c_str());
        if (!response) {
            throw std::runtime_error("Failed to connect to Docker socket");
        }
    } else {
        throw std::runtime_error("Unsupported HTTP method");
    }
    if (response->status != 200) {
        throw std::runtime_error("Docker API returned non-OK status");
    }

    return nlohmann::json::parse(response->body, nullptr, false);
}

std::vector<kaos::domain::Container> DockerClientAdapter::listContainers() {
    std::vector<kaos::domain::Container> containers;

    auto json_response = request("GET", "/containers/json");
    if (!json_response.is_array()) {
        throw std::runtime_error("Unexpected Docker API response format");
    }

    for (const auto& item : json_response) {
        kaos::domain::Container container;
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
        containers.push_back(container);
    }

    return containers;
}

DockerClientAdapter DockerClientAdapter::create() {
    return DockerClientAdapter("/var/run/docker.sock");
}

}  // namespace kaos::adapters::infra::docker
