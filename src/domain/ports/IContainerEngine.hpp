#pragma once

#include <vector>

#include "domain/entities/Container.hpp"

namespace chaos::domain::ports {

using chaos::domain::Container;

class IContainerEngine {
public:
    virtual ~IContainerEngine() = default;

    virtual std::vector<Container> listContainers() = 0;
};

}  // namespace chaos::domain::ports
