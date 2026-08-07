#include "usdgeo/cache/Cache.h"

#include <filesystem>
#include <string>

namespace usdgeo::cache {
namespace {

bool HasArgumentsWithEmptyNames(const usdgeo::CacheArguments& arguments) {
    for (const auto& [name, value] : arguments) {
        static_cast<void>(value);
        if (name.find_first_not_of(" \t\r\n") == std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string AxisName(std::int64_t value) {
    if (value >= 0) {
        return "p" + std::to_string(static_cast<std::uint64_t>(value));
    }
    const auto magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1;
    return "n" + std::to_string(magnitude);
}

std::string TileName(const usdgeo::TileId& tile) {
    return "Tile_L" + std::to_string(tile.level) + "_" + AxisName(tile.x) +
           "_" + AxisName(tile.y) + "_" + AxisName(tile.z);
}

} // namespace

bool SourceIdentity::IsValid() const noexcept {
    return !canonicalPath.empty() && !contentIdentity.empty();
}

bool Descriptor::IsValid() const noexcept {
    return source.IsValid() && !pluginVersion.empty() &&
           !parserVersion.empty() && !openUsdVersion.empty() &&
           !HasArgumentsWithEmptyNames(coordinateTransform) &&
           !HasArgumentsWithEmptyNames(attributes) &&
           !HasArgumentsWithEmptyNames(tileAndLod) &&
           !HasArgumentsWithEmptyNames(downsampling);
}

bool Layout::IsValid() const noexcept {
    return !entryDirectory.empty() && !rootLayer.empty() &&
           !manifest.empty() && !payloadDirectory.empty();
}

usdgeo::CacheArguments MakeCacheArguments(const Descriptor& descriptor) {
    usdgeo::CacheArguments arguments{
        {"source.path", descriptor.source.canonicalPath},
        {"source.size", std::to_string(descriptor.source.sizeBytes)},
        {"source.modified", std::to_string(descriptor.source.modifiedTime)},
        {"source.content", descriptor.source.contentIdentity},
        {"plugin.version", descriptor.pluginVersion},
        {"parser.version", descriptor.parserVersion},
        {"openusd.version", descriptor.openUsdVersion}};

    const auto append = [&arguments](const char* prefix,
                                     const usdgeo::CacheArguments& values) {
        for (const auto& [name, value] : values) {
            arguments.emplace_back(std::string(prefix) + "." + name, value);
        }
    };
    append("transform", descriptor.coordinateTransform);
    append("attributes", descriptor.attributes);
    append("tile-lod", descriptor.tileAndLod);
    append("downsampling", descriptor.downsampling);
    return arguments;
}

std::string StableCacheKey(const Descriptor& descriptor) {
    if (!descriptor.IsValid()) {
        return {};
    }
    return usdgeo::StableCacheKey(MakeCacheArguments(descriptor));
}

bool TryBuildLayout(const std::filesystem::path& cacheRoot,
                    const Descriptor& descriptor,
                    Layout& layout) {
    layout = {};
    if (cacheRoot.empty() || !descriptor.IsValid()) {
        return false;
    }

    const auto key = StableCacheKey(descriptor);
    if (key.empty()) {
        return false;
    }

    layout.entryDirectory = cacheRoot / key;
    layout.rootLayer = layout.entryDirectory / "root.usdc";
    layout.manifest = layout.entryDirectory / "cache.manifest";
    layout.payloadDirectory = layout.entryDirectory / "payloads";
    return true;
}

std::filesystem::path TilePayloadPath(const Layout& layout,
                                      const usdgeo::TileId& tile,
                                      int lodLevel) {
    if (!layout.IsValid() || !tile.IsValid() || lodLevel < 0) {
        return {};
    }
    return layout.payloadDirectory /
        (TileName(tile) + "_LOD" + std::to_string(lodLevel) + ".usdc");
}

bool IsCacheHit(const Layout& layout) noexcept {
    if (!layout.IsValid()) {
        return false;
    }
    std::error_code rootError;
    const auto rootExists =
        std::filesystem::is_regular_file(layout.rootLayer, rootError);
    if (rootError || !rootExists) {
        return false;
    }

    std::error_code manifestError;
    const auto manifestExists =
        std::filesystem::is_regular_file(layout.manifest, manifestError);
    return !manifestError && manifestExists;
}

bool Invalidate(const std::filesystem::path& cacheRoot,
                const Descriptor& descriptor) noexcept {
    Layout layout;
    if (!TryBuildLayout(cacheRoot, descriptor, layout)) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove_all(layout.entryDirectory, error);
    return !error;
}

} // namespace usdgeo::cache