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

bool HasNonWhitespace(const std::string& value) {
    return value.find_first_not_of(" \t\r\n") != std::string::npos;
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

constexpr const char* kRootLayerName = "root.usdc";
constexpr const char* kManifestName = "cache.manifest";
constexpr const char* kPayloadDirectoryName = "payloads";

// Layout keys are the 16 lowercase hex characters `usdgeo::StableCacheKey`
// emits. Anything else in a generation directory - a converter's temporary
// entry, an unrelated file - is not a committed sibling entry.
bool IsLayoutKeyName(const std::string& name) noexcept {
    if (name.size() != 16) {
        return false;
    }
    for (const char character : name) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        if (!digit && !lower) {
            return false;
        }
    }
    return true;
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

const char* CacheDecisionName(CacheDecision decision) noexcept {
    switch (decision) {
    case CacheDecision::IdentityUnavailable:
        return "resolver-identity-unavailable";
    case CacheDecision::IdentityUnstable:
        return "resolver-identity-unstable";
    case CacheDecision::IdentityStable:
        return "resolver-identity-stable";
    case CacheDecision::IdentityChanged:
        return "resolver-identity-changed";
    case CacheDecision::ReuseDisabled:
        return "generated-cache-reuse-disabled";
    case CacheDecision::Hit:
        return "generated-cache-hit";
    case CacheDecision::Invalidated:
        return "generated-cache-invalidated";
    }
    return "generated-cache-reuse-disabled";
}

const char* CacheDecisionMessage(CacheDecision decision) noexcept {
    switch (decision) {
    case CacheDecision::IdentityUnavailable:
        return "Source identity unavailable: the active resolver exposed no "
               "usable identity metadata.";
    case CacheDecision::IdentityUnstable:
        return "Source identity unstable: the active resolver identified the "
               "source but could not guarantee its freshness.";
    case CacheDecision::IdentityStable:
        return "Source identity stable: generated cache reuse is permitted.";
    case CacheDecision::IdentityChanged:
        return "Source identity changed: a generated entry exists for a "
               "different source validation identity, so output is "
               "regenerated.";
    case CacheDecision::ReuseDisabled:
        return "Generated cache reuse disabled: the active resolver did not "
               "provide a stable source validation identity.";
    case CacheDecision::Hit:
        return "Generated cache hit: the committed entry matched and was "
               "reused.";
    case CacheDecision::Invalidated:
        return "Generated cache invalidated: the committed entry did not "
               "validate and was removed.";
    }
    return "Generated cache reuse disabled: the active resolver did not "
           "provide a stable source validation identity.";
}

CacheDecision IdentityDecision(
    ResolverIdentityStability stability) noexcept {
    switch (stability) {
    case ResolverIdentityStability::Stable:
        return CacheDecision::IdentityStable;
    case ResolverIdentityStability::Unstable:
        return CacheDecision::IdentityUnstable;
    case ResolverIdentityStability::Unavailable:
        return CacheDecision::IdentityUnavailable;
    }
    return CacheDecision::IdentityUnavailable;
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
           !HasArgumentsWithEmptyNames(downsampling) &&
           !HasArgumentsWithEmptyNames(sourceDerived);
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
    if (!HasNonWhitespace(assetIdentity.resolvedIdentifier)) {
        return ResolverIdentityStability::Unavailable;
    }
    if (!HasNonWhitespace(assetIdentity.validationToken)) {
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

namespace {

const std::string& SourceValidation(const Descriptor& descriptor) {
    return descriptor.source.validationToken.empty()
               ? descriptor.source.contentIdentity
               : descriptor.source.validationToken;
}

void AppendPrefixed(usdgeo::CacheArguments& arguments,
                    const char* prefix,
                    const usdgeo::CacheArguments& values) {
    for (const auto& [name, value] : values) {
        arguments.emplace_back(std::string(prefix) + "." + name, value);
    }
}

// The caller-intent half: what was asked for, independent of what the source
// turned out to contain. Nothing here may vary between two revisions of one
// source, or those revisions stop being siblings and a changed validation
// identity becomes indistinguishable from a source never seen before.
usdgeo::CacheArguments MakeGenerationArguments(const Descriptor& descriptor) {
    const auto& sourceIdentifier = descriptor.source.identifier.empty()
                                       ? descriptor.source.canonicalPath
                                       : descriptor.source.identifier;
    usdgeo::CacheArguments arguments{
        {"source.identifier", sourceIdentifier},
        {"plugin.version", descriptor.pluginVersion},
        {"parser.version", descriptor.parserVersion},
        {"openusd.version", descriptor.openUsdVersion}};
    AppendPrefixed(arguments, "attributes", descriptor.attributes);
    AppendPrefixed(arguments, "tile-lod", descriptor.tileAndLod);
    AppendPrefixed(arguments, "downsampling", descriptor.downsampling);
    return arguments;
}

// The source-derived half: the revision metadata the filesystem or a resolver
// reports, the georeference resolved out of the source header - its local
// origin is the source bounding box, and its CRS may be an embedded record -
// and anything else a caller computed by scanning the source.
usdgeo::CacheArguments MakeIdentityArguments(const Descriptor& descriptor) {
    usdgeo::CacheArguments arguments{
        {"source.size", std::to_string(descriptor.source.sizeBytes)},
        {"source.modified", std::to_string(descriptor.source.modifiedTime)},
        {"source.validation", SourceValidation(descriptor)}};
    AppendPrefixed(arguments, "transform", descriptor.coordinateTransform);
    AppendPrefixed(arguments, "source-derived", descriptor.sourceDerived);
    return arguments;
}

} // namespace

usdgeo::CacheArguments MakeCacheArguments(const Descriptor& descriptor) {
    auto arguments = MakeGenerationArguments(descriptor);
    for (auto& [name, value] : MakeIdentityArguments(descriptor)) {
        arguments.emplace_back(name, value);
    }
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

    // Two levels: the generation inputs choose the directory, the source
    // validation identity chooses the entry inside it. Equal generation inputs
    // therefore collect every revision of the same source side by side, which
    // is what lets a changed validation token be reported as changed instead of
    // as never seen. Neither level stores the identifier or the token itself;
    // both are hashes.
    const auto generationKey =
        usdgeo::StableCacheKey(MakeGenerationArguments(descriptor));
    const auto identityKey =
        usdgeo::StableCacheKey(MakeIdentityArguments(descriptor));
    if (generationKey.empty() || identityKey.empty()) {
        return false;
    }

    layout.entryDirectory = cacheRoot / generationKey / identityKey;
    layout.rootLayer = layout.entryDirectory / kRootLayerName;
    layout.manifest = layout.entryDirectory / kManifestName;
    layout.payloadDirectory = layout.entryDirectory / kPayloadDirectoryName;
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

bool HasSupersededIdentityEntry(const Layout& layout) noexcept {
    if (!layout.IsValid()) {
        return false;
    }
    const auto generationDirectory = layout.entryDirectory.parent_path();
    if (generationDirectory.empty()) {
        return false;
    }
    std::error_code error;
    std::filesystem::directory_iterator iterator(generationDirectory, error);
    const std::filesystem::directory_iterator end;
    if (error) {
        return false;
    }
    const auto entryName = layout.entryDirectory.filename().string();
    for (; iterator != end; iterator.increment(error)) {
        if (error) {
            return false;
        }
        const auto& path = iterator->path();
        const auto name = path.filename().string();
        if (name == entryName || !IsLayoutKeyName(name)) {
            continue;
        }
        const auto root = InspectMarker(path / kRootLayerName);
        const auto manifest = InspectMarker(path / kManifestName);
        if (root.exists && root.regular && manifest.exists &&
            manifest.regular) {
            return true;
        }
    }
    return false;
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
    if (error) {
        return false;
    }
    // Drop the generation directory once its last identity entry is gone, so
    // an invalidated cache root does not accumulate empty parents.
    std::error_code cleanupError;
    std::filesystem::remove(layout.entryDirectory.parent_path(), cleanupError);
    return true;
}

} // namespace usdgeo::cache