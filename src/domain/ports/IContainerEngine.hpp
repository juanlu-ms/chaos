#pragma once

#include <vector>

#include "domain/entities/Container.hpp"

namespace kaos::domain::ports {

using kaos::domain::Container;

class IContainerEngine {
public:
    virtual ~IContainerEngine() = default;

    virtual std::vector<Container> listContainers() = 0;
};

}  // namespace kaos::domain::ports
