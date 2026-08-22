#pragma once

#include "usdgeo/GeoReference.h"
#include "usdgeo/cache/Cache.h"
#include "usdpointcloud/FileFormatArguments.h"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>

#include <filesystem>
#include <string>

namespace usdgeo {

std::filesystem::path PointCloudCacheRootFromEnvironment();

bool TryBuildPointCloudCacheLayout(
    const std::filesystem::path& cacheRoot,
    const cache::SourceIdentity& sourceIdentity,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    cache::Layout& layout,
    std::string& errorMessage);

bool TryBuildResolverSourceIdentity(
    const pxr::ArResolver& resolver,
    const std::string& assetPath,
    const pxr::ArResolvedPath& resolvedPath,
    const pxr::ArAsset& asset,
    cache::SourceIdentity& identity,
    cache::ResolverIdentityStability& stability,
    std::string& errorMessage);

// `decision`, when given, receives the stable category that explains what the
// lookup did: `Hit` when a committed entry was reused, `Invalidated` when one
// was rejected and removed, `IdentityChanged` when the same generation inputs
// already hold an entry for a different source validation identity. A plain
// first-time miss leaves it untouched, so a caller that has already classified
// the source identity keeps that classification.
bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const std::filesystem::path& sourcePath,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    bool& hit,
    std::string& errorMessage,
    cache::CacheDecision* decision = nullptr);

bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const cache::SourceIdentity& sourceIdentity,
    const std::filesystem::path& payloadBaseDirectory,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    bool& hit,
    std::string& errorMessage,
    cache::CacheDecision* decision = nullptr);

} // namespace usdgeo
