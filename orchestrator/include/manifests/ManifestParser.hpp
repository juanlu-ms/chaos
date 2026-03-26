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
    /**
     * @brief Parse a ChaosManifest from a file path.
     * @param filepath Path to the manifest file.
     * @return ChaosManifest object.
     * @throws ManifestParserError On parse failure.
     */
    static ChaosManifest parseFromFile(const std::string& filepath);

    /**
     * @brief Parse a ChaosManifest from a JSON string.
     * @param jsonStr JSON string to parse.
     * @return ChaosManifest object.
     * @throws ManifestParserError On parse failure.
     */
    static ChaosManifest parseFromJson(const std::string& jsonStr);
};

}  // namespace chaos::orchestrator::manifests
