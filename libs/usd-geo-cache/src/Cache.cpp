#include "usdgeo/cache/Cache.h"

#include <array>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
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
    return (!identifier.empty() || !canonicalPath.empty()) &&
           (!validationToken.empty() || !contentIdentity.empty());
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

bool TryBuildLocalSourceIdentity(const std::filesystem::path& sourcePath,
                                 SourceIdentity& identity,
                                 std::string& errorMessage) {
    identity = {};
    errorMessage.clear();
    std::error_code error;
    const auto canonicalPath =
        std::filesystem::weakly_canonical(sourcePath, error);
    if (error) {
        errorMessage = "unable to canonicalize input for cache identity: " +
                       error.message();
        return false;
    }
    const auto size = std::filesystem::file_size(sourcePath, error);
    if (error) {
        errorMessage = "unable to inspect input for cache identity: " +
                       error.message();
        return false;
    }
    const auto modified = std::filesystem::last_write_time(sourcePath, error);
    if (error) {
        errorMessage = "unable to inspect input timestamp for cache identity: " +
                       error.message();
        return false;
    }

    std::ifstream input(sourcePath, std::ios::binary);
    if (!input) {
        errorMessage = "unable to open input for cache identity";
        return false;
    }
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(
                buffer[static_cast<std::size_t>(index)]);
            hash *= prime;
        }
    }
    if (!input.eof()) {
        errorMessage = "unable to read input for cache identity";
        return false;
    }

    std::ostringstream token;
    token << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
          << hash;
    identity.identifier = canonicalPath.generic_string();
    identity.sizeBytes = size;
    identity.modifiedTime =
        static_cast<std::int64_t>(modified.time_since_epoch().count());
    identity.validationToken = token.str();
    identity.canonicalPath = identity.identifier;
    identity.contentIdentity = identity.validationToken;
    if (!identity.IsValid()) {
        errorMessage = "generated source identity is invalid";
        return false;
    }
    return true;
}

usdgeo::CacheArguments MakeCacheArguments(const Descriptor& descriptor) {
    const auto& sourceIdentifier = descriptor.source.identifier.empty()
                                       ? descriptor.source.canonicalPath
                                       : descriptor.source.identifier;
    const auto& sourceValidation = descriptor.source.validationToken.empty()
                                       ? descriptor.source.contentIdentity
                                       : descriptor.source.validationToken;
    usdgeo::CacheArguments arguments{
        {"source.identifier", sourceIdentifier},
        {"source.size", std::to_string(descriptor.source.sizeBytes)},
        {"source.modified", std::to_string(descriptor.source.modifiedTime)},
        {"source.validation", sourceValidation},
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

LookupResult Inspect(const Layout& layout) noexcept {
    if (!layout.IsValid()) {
        return {LookupStatus::InvalidLayout};
    }
    std::error_code rootError;
    const auto rootExists =
        std::filesystem::is_regular_file(layout.rootLayer, rootError);

    std::error_code manifestError;
    const auto manifestExists =
        std::filesystem::is_regular_file(layout.manifest, manifestError);
    if (!rootExists && !manifestExists) {
        return {LookupStatus::Missing};
    }
    if (rootError || manifestError || !rootExists || !manifestExists) {
        return {LookupStatus::Incomplete};
    }
    return {LookupStatus::Hit};
}

bool IsCacheHit(const Layout& layout) noexcept {
    return Inspect(layout).IsHit();
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