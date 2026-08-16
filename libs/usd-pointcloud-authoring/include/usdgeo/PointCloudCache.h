#pragma once

#include "usdgeo/GeoReference.h"
#include "usdgeo/cache/Cache.h"
#include "usdpointcloud/FileFormatArguments.h"

#include <pxr/usd/sdf/layer.h>

#include <filesystem>
#include <string>

namespace usdgeo {

std::filesystem::path PointCloudCacheRootFromEnvironment();

bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const std::filesystem::path& sourcePath,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    bool& hit,
    std::string& errorMessage);

bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const std::filesystem::path& sourcePath,
    const cache::SourceIdentity& sourceIdentity,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    bool& hit,
    std::string& errorMessage);

} // namespace usdgeo