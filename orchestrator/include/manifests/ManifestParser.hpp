#pragma once

#include <string>

#include "Manifest.hpp"

namespace chaos::orchestrator::manifests {

class ManifestParser {
public:
    static ChaosManifest parse(const std::string& filepath);
};

}  // namespace chaos::orchestrator::manifests
