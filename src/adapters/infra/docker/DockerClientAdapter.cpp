#include <curl/curl.h>

#include <adapters/infra/docker/DockerClientAdapter.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace kaos::adapters::infra::docker {

DockerClientAdapter::DockerClientAdapter(CURL* curl_handle) : curl_handle_(curl_handle) {}

DockerClientAdapter::~DockerClientAdapter() {
    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
    }
}

DockerClientAdapter::DockerClientAdapter(DockerClientAdapter&& other) noexcept : curl_handle_(other.curl_handle_) {
    other.curl_handle_ = nullptr;
}

size_t DockerClientAdapter::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::vector<kaos::domain::Container> DockerClientAdapter::listContainers() {
    if (!curl_handle_) {
        throw std::runtime_error("CURL handle is not initialized");
    }

    std::string response;
    curl_easy_setopt(curl_handle_, CURLOPT_URL, "http://localhost/containers/json");
    curl_easy_setopt(curl_handle_, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, &DockerClientAdapter::WriteCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl_handle_);
    if (result != CURLE_OK) {
        throw std::runtime_error(curl_easy_strerror(result));
    }

    long status_code = 0;
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code != 200) {
        throw std::runtime_error("Docker API returned non-OK status");
    }

    std::vector<kaos::domain::Container> containers;
    auto json_response = nlohmann::json::parse(response, nullptr, false);
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
        }
        if (item.contains("State") && item["State"].is_string()) {
            container.state = item["State"].get<std::string>();
        }
        containers.push_back(container);
    }

    return containers;
}

std::expected<DockerClientAdapter, std::string> DockerClientAdapter::create() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return std::unexpected("Failed to initialize CURL");
    }
    return DockerClientAdapter(curl);
}

}  // namespace kaos::adapters::infra::docker
