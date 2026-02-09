#pragma once

#include <curl/curl.h>

#include <domain/entities/Container.hpp>
#include <domain/ports/IContainerEngine.hpp>
#include <expected>
#include <vector>

namespace kaos::adapters::infra::docker {

class DockerClientAdapter : public kaos::domain::ports::IContainerEngine {
private:
    CURL* curl_handle_;

    explicit DockerClientAdapter(CURL* curl_handle);

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

public:
    ~DockerClientAdapter();

    DockerClientAdapter(const DockerClientAdapter&) = delete;
    DockerClientAdapter& operator=(const DockerClientAdapter&) = delete;

    DockerClientAdapter(DockerClientAdapter&& other) noexcept;

    static std::expected<DockerClientAdapter, std::string> create();

    std::vector<kaos::domain::Container> listContainers() override;
};

}  // namespace kaos::adapters::infra::docker
