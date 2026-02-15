#pragma once

#include <domain/ports/IContainerEngine.hpp>
#include <functional>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "domain/entities/Container.hpp"

namespace chaos::adapters::infra::docker {

enum class HttpMethod { GET, POST };

class DockerClientAdapter : public chaos::domain::ports::IContainerEngine {
public:
    using RequestFn = std::function<nlohmann::json(HttpMethod, const std::string&)>;

    explicit DockerClientAdapter(RequestFn requestFn);
    ~DockerClientAdapter() override = default;

    DockerClientAdapter(const DockerClientAdapter&) = delete;
    DockerClientAdapter& operator=(const DockerClientAdapter&) = delete;
    DockerClientAdapter(DockerClientAdapter&&) noexcept = default;
    DockerClientAdapter& operator=(DockerClientAdapter&&) noexcept = default;

    static std::unique_ptr<chaos::domain::ports::IContainerEngine> create(
        const std::string& socket_path = "/var/run/docker.sock");

    std::vector<chaos::domain::Container> listContainers() override;

private:
    RequestFn request_;
};

}  // namespace chaos::adapters::infra::docker
