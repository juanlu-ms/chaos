#pragma once

#include <domain/entities/Container.hpp>
#include <vector>

namespace kaos::domain::ports {

using kaos::domain::Container;

class IContainerEngine {
public:
    virtual ~IContainerEngine() = default;

    virtual std::vector<Container> listContainers() = 0;
};

}  // namespace kaos::domain::ports
