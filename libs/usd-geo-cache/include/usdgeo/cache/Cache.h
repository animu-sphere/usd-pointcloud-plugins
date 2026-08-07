#pragma once

#include "usdgeo/CacheKey.h"
#include "usdgeo/TileId.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace usdgeo::cache {

struct SourceIdentity {
    std::string canonicalPath;
    std::uintmax_t sizeBytes = 0;
    std::int64_t modifiedTime = 0;
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

usdgeo::CacheArguments MakeCacheArguments(const Descriptor& descriptor);
std::string StableCacheKey(const Descriptor& descriptor);

bool TryBuildLayout(const std::filesystem::path& cacheRoot,
                    const Descriptor& descriptor,
                    Layout& layout);

std::filesystem::path TilePayloadPath(const Layout& layout,
                                      const usdgeo::TileId& tile,
                                      int lodLevel);

bool IsCacheHit(const Layout& layout) noexcept;
bool Invalidate(const Layout& layout) noexcept;

} // namespace usdgeo::cache