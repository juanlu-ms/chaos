#pragma once

#include <domain/ports/IContainerEngine.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "domain/entities/Container.hpp"

namespace chaos::adapters::infra::docker {

class DockerClientAdapter : public chaos::domain::ports::IContainerEngine {
private:
    std::string socket_path_;

    explicit DockerClientAdapter(std::string socket_path);

    nlohmann::json request(std::string method, const std::string& endpoint);

public:
    ~DockerClientAdapter() = default;

    DockerClientAdapter(const DockerClientAdapter&) = delete;
    DockerClientAdapter& operator=(const DockerClientAdapter&) = delete;

    DockerClientAdapter(DockerClientAdapter&& other) noexcept = default;

    static DockerClientAdapter create();

    std::vector<chaos::domain::Container> listContainers() override;
};

}  // namespace chaos::adapters::infra::docker
