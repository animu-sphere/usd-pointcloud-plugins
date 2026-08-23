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

// Stable, transport-neutral categories used to explain a cache decision.
// Names and meanings are published; messages are for humans and may change.
// No value ever carries transport specifics or validation-token contents.
enum class CacheDecision {
    IdentityUnavailable,
    IdentityUnstable,
    IdentityStable,
    IdentityChanged,
    ReuseDisabled,
    Hit,
    Invalidated,
};

const char* CacheDecisionName(CacheDecision decision) noexcept;
const char* CacheDecisionMessage(CacheDecision decision) noexcept;
CacheDecision IdentityDecision(ResolverIdentityStability stability) noexcept;

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

// A descriptor has two halves, and which half a value belongs in decides
// whether two reads of one source are recognizable as revisions of each other.
//
//   caller intent    what was asked for, independent of the bytes:
//                    versions, attribute selection, tiling and LOD arguments,
//                    downsampling. These choose the generation directory.
//   source-derived   what was read out of the source: its revision metadata,
//                    the georeference resolved from its header, and any plan
//                    computed by scanning it. These choose the entry inside
//                    that directory.
//
// Putting a source-derived value in the caller-intent half is a defect, not a
// preference: a revised source would land in an unrelated generation directory,
// and `HasSupersededIdentityEntry` could no longer see that it superseded
// anything.
struct Descriptor {
    SourceIdentity source;
    std::string pluginVersion;
    std::string parserVersion;
    std::string openUsdVersion;
    // Source-derived: the georeference resolved from the source header, whose
    // local origin is the source bounding box and whose CRS may come from an
    // embedded record.
    usdgeo::CacheArguments coordinateTransform;
    usdgeo::CacheArguments attributes;
    usdgeo::CacheArguments tileAndLod;
    usdgeo::CacheArguments downsampling;
    // Source-derived: anything else a caller computed by reading the source,
    // such as a tile-plan key produced by scanning it. Planner identity and
    // version are caller intent and belong in `tileAndLod`.
    usdgeo::CacheArguments sourceDerived;

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

// True when the generation directory that owns this entry already holds a
// committed entry for a different source validation identity. It is how a
// changed validation token is distinguished from a source never generated
// before, without persisting the token or the resolved identifier.
bool HasSupersededIdentityEntry(const Layout& layout) noexcept;
bool IsCacheHit(const Layout& layout) noexcept;
LookupStatistics GetLookupStatistics() noexcept;
void ResetLookupStatistics() noexcept;
bool Invalidate(const std::filesystem::path& cacheRoot,
                const Descriptor& descriptor) noexcept;

} // namespace usdgeo::cache