#pragma once

#include <domain/entities/Container.hpp>
#include <domain/ports/IContainerEngine.hpp>
#include <expected>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

namespace kaos::adapters::infra::docker {

class DockerClientAdapter : public kaos::domain::ports::IContainerEngine {
private:
    std::string socket_path_;

    explicit DockerClientAdapter(std::string socket_path);

    nlohmann::json request(std::string method, const std::string& endpoint);

public:
    ~DockerClientAdapter() = default;

    DockerClientAdapter(const DockerClientAdapter&) = delete;
    DockerClientAdapter& operator=(const DockerClientAdapter&) = delete;

    DockerClientAdapter(DockerClientAdapter&& other) noexcept = default;

    static std::expected<DockerClientAdapter, std::string> create();

    std::vector<kaos::domain::Container> listContainers() override;
};

}  // namespace kaos::adapters::infra::docker
