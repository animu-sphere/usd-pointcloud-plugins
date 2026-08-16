#pragma once

#include "usdgeo/CacheKey.h"
#include "usdgeo/TileId.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace usdgeo::cache {

enum class ResolverIdentityStability {
    Stable,
    Unstable,
    Unavailable,
};

const char* ResolverIdentityStabilityName(
    ResolverIdentityStability stability) noexcept;

struct ResolverAssetIdentity {
    std::string resolvedIdentifier;
    std::uintmax_t sizeBytes = 0;
    std::string validationToken;
};

struct SourceIdentity {
    std::string identifier;
    std::uintmax_t sizeBytes = 0;
    std::int64_t modifiedTime = 0;
    std::string validationToken;
    std::string canonicalPath;
    std::string contentIdentity;

    bool IsValid() const noexcept;
};

struct Descriptor {
    SourceIdentity source;
    std::string pluginVersion;
    std::string parserVersion;
    std::string openUsdVersion;
    usdgeo::CacheArguments coordinateTransform;
    usdgeo::CacheArguments attributes;
    usdgeo::CacheArguments tileAndLod;
    usdgeo::CacheArguments downsampling;

    bool IsValid() const noexcept;
};

struct Layout {
    std::filesystem::path entryDirectory;
    std::filesystem::path rootLayer;
    std::filesystem::path manifest;
    std::filesystem::path payloadDirectory;

    bool IsValid() const noexcept;
};

enum class LookupStatus {
    InvalidLayout,
    Missing,
    Incomplete,
    Hit,
};

const char* LookupStatusName(LookupStatus status) noexcept;

struct LookupResult {
    LookupStatus status = LookupStatus::InvalidLayout;

    bool IsHit() const noexcept { return status == LookupStatus::Hit; }
};

struct LookupStatistics {
    std::uint64_t lookups = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t incomplete = 0;
    std::uint64_t invalidLayouts = 0;

    double HitRatio() const noexcept;
};

usdgeo::CacheArguments MakeCacheArguments(const Descriptor& descriptor);
std::string StableCacheKey(const Descriptor& descriptor);

bool TryBuildLocalSourceIdentity(const std::filesystem::path& sourcePath,
                                 SourceIdentity& identity,
                                 std::string& errorMessage);

ResolverIdentityStability ClassifyResolverIdentity(
    const ResolverAssetIdentity& assetIdentity) noexcept;

bool TryBuildResolverSourceIdentity(
    const ResolverAssetIdentity& assetIdentity,
    SourceIdentity& identity,
    ResolverIdentityStability& stability,
    std::string& errorMessage);

bool TryBuildLayout(const std::filesystem::path& cacheRoot,
                    const Descriptor& descriptor,
                    Layout& layout);

std::filesystem::path TilePayloadPath(const Layout& layout,
                                      const usdgeo::TileId& tile,
                                      int lodLevel);

LookupResult Inspect(const Layout& layout) noexcept;
bool IsCacheHit(const Layout& layout) noexcept;
LookupStatistics GetLookupStatistics() noexcept;
void ResetLookupStatistics() noexcept;
bool Invalidate(const std::filesystem::path& cacheRoot,
                const Descriptor& descriptor) noexcept;

} // namespace usdgeo::cache