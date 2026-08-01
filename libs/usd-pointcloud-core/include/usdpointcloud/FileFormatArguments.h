#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdpointcloud/PointCloud.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace usdpointcloud {

struct PointReadRequest {
    PointReadOptions readOptions;
    std::vector<std::string> attributes;
    std::map<std::string, std::string> canonicalArguments;
    std::string normalizedArguments;
};

bool ParseFileFormatArgumentString(
    std::string_view encoded,
    std::map<std::string, std::string>& arguments,
    std::string& error);

bool NormalizeFileFormatArguments(
    const std::map<std::string, std::string>& arguments,
    PointReadRequest& request,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool SelectPointDataAttributes(PointData& data,
                               const std::vector<std::string>& attributes,
                               std::string& error);

} // namespace usdpointcloud