#pragma once

#include <stdexcept>
#include <string>

#include "Manifest.hpp"

namespace chaos::orchestrator::manifests {

class ManifestParserError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ManifestParser {
public:
    static ChaosManifest parse(const std::string& filepath);
};

}  // namespace chaos::orchestrator::manifests
