#include "usdgeo/cache/Cache.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace usdgeo::cache {
namespace {

std::mutex lookupStatisticsMutex;
std::uint64_t lookupCount = 0;
std::uint64_t hitCount = 0;
std::uint64_t missCount = 0;
std::uint64_t incompleteCount = 0;
std::uint64_t invalidLayoutCount = 0;

void RecordLookup(LookupStatus status) noexcept {
    const std::lock_guard lock(lookupStatisticsMutex);
    ++lookupCount;
    switch (status) {
    case LookupStatus::Hit:
        ++hitCount;
        break;
    case LookupStatus::Missing:
        ++missCount;
        break;
    case LookupStatus::Incomplete:
        ++incompleteCount;
        break;
    case LookupStatus::InvalidLayout:
        ++invalidLayoutCount;
        break;
    }
}

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

struct MarkerStatus {
    bool exists = false;
    bool regular = false;
    bool error = false;
};

MarkerStatus InspectMarker(const std::filesystem::path& path) noexcept {
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            return {};
        }
        return {false, false, true};
    }
    if (status.type() == std::filesystem::file_type::not_found) {
        return {};
    }
    return {true, status.type() == std::filesystem::file_type::regular,
            false};
}

} // namespace

const char* LookupStatusName(LookupStatus status) noexcept {
    switch (status) {
    case LookupStatus::InvalidLayout:
        return "invalid-layout";
    case LookupStatus::Missing:
        return "missing";
    case LookupStatus::Incomplete:
        return "incomplete";
    case LookupStatus::Hit:
        return "hit";
    }
    return "invalid-layout";
}

const char* ResolverIdentityStabilityName(
    ResolverIdentityStability stability) noexcept {
    switch (stability) {
    case ResolverIdentityStability::Stable:
        return "stable";
    case ResolverIdentityStability::Unstable:
        return "unstable";
    case ResolverIdentityStability::Unavailable:
        return "unavailable";
    }
    return "unavailable";
}

double LookupStatistics::HitRatio() const noexcept {
    return lookups == 0 ? 0.0
                        : static_cast<double>(hits) /
                              static_cast<double>(lookups);
}

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

ResolverIdentityStability ClassifyResolverIdentity(
    const ResolverAssetIdentity& assetIdentity) noexcept {
    if (assetIdentity.resolvedIdentifier.empty()) {
        return ResolverIdentityStability::Unavailable;
    }
    if (assetIdentity.validationToken.empty()) {
        return ResolverIdentityStability::Unstable;
    }
    return ResolverIdentityStability::Stable;
}

bool TryBuildResolverSourceIdentity(
    const ResolverAssetIdentity& assetIdentity,
    SourceIdentity& identity,
    ResolverIdentityStability& stability,
    std::string& errorMessage) {
    identity = {};
    stability = ClassifyResolverIdentity(assetIdentity);
    errorMessage.clear();
    if (stability != ResolverIdentityStability::Stable) {
        errorMessage = stability == ResolverIdentityStability::Unstable
                           ? "resolver source identity is unstable"
                           : "resolver source identity is unavailable";
        return false;
    }

    identity.identifier = assetIdentity.resolvedIdentifier;
    identity.sizeBytes = assetIdentity.sizeBytes;
    identity.validationToken = assetIdentity.validationToken;
    if (!identity.IsValid()) {
        errorMessage = "resolver source identity is invalid";
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
    LookupStatus status = LookupStatus::InvalidLayout;
    if (!layout.IsValid()) {
        RecordLookup(status);
        return {status};
    }
    const auto root = InspectMarker(layout.rootLayer);
    const auto manifest = InspectMarker(layout.manifest);
    if (root.error || manifest.error) {
        status = LookupStatus::Incomplete;
    } else if (!root.exists && !manifest.exists) {
        status = LookupStatus::Missing;
    } else if (!root.regular || !manifest.regular) {
        status = LookupStatus::Incomplete;
    } else {
        status = LookupStatus::Hit;
    }
    RecordLookup(status);
    return {status};
}

bool IsCacheHit(const Layout& layout) noexcept {
    return Inspect(layout).IsHit();
}

LookupStatistics GetLookupStatistics() noexcept {
    const std::lock_guard lock(lookupStatisticsMutex);
    return {lookupCount, hitCount, missCount, incompleteCount,
            invalidLayoutCount};
}

void ResetLookupStatistics() noexcept {
    const std::lock_guard lock(lookupStatisticsMutex);
    lookupCount = 0;
    hitCount = 0;
    missCount = 0;
    incompleteCount = 0;
    invalidLayoutCount = 0;
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