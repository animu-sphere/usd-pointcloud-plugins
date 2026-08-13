#pragma once

#include "usdgeo/CacheKey.h"
#include "usdgeo/Diagnostic.h"
#include "usdgeo/TileId.h"
#include "usdgeo/SpatialBounds.h"
#include "usdpointcloud/Lod.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace usdpointcloud {

struct TileGridConfig {
    double tileSize = 0.0;
    std::int32_t level = 0;

    bool IsValid() const noexcept;
};

bool ValidateTileGridConfig(const TileGridConfig& config,
                            std::vector<usdgeo::Diagnostic>& diagnostics);

struct PointBudgetConfig {
    std::size_t maxPointsPerTile = 0;
    std::size_t minPointsPerTile = 0;
    std::int32_t maxDepth = 0;

    bool IsValid() const noexcept;
};

constexpr std::int32_t kMaxPointBudgetDepth = 31;
constexpr const char* kAdaptivePointBudgetPlannerId =
    "adaptive-point-budget";
constexpr std::uint32_t kAdaptivePointBudgetPlannerVersion = 1;

struct PointBudgetTile {
    PointTileId id;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    std::size_t pointCount = 0;

    bool IsValid() const noexcept;
};

struct TilePlanSourceRange {
    std::uint64_t offset = 0;
    std::uint64_t length = 0;

    bool IsValid() const noexcept;
};

struct TilePlanNode {
    PointTileId id;
    usdgeo::SpatialBounds bounds;
    std::uint64_t pointCount = 0;
    PointTileId parent{-1, 0, 0, 0};
    std::vector<PointTileId> children;
    std::vector<TilePlanSourceRange> sourceRanges;
    bool isLeaf = false;

    bool IsValid() const noexcept;
};

struct TilePlan {
    std::string plannerId;
    std::uint32_t plannerVersion = 0;
    std::vector<TilePlanNode> nodes;

    bool IsValid() const noexcept;
};

struct PointBudgetPlan {
    std::size_t tileCount = 0;
    std::size_t maximumPointsPerTile = 0;
    std::int32_t depth = 0;
    std::size_t pointCount = 0;
    std::size_t minimumPointsPerTile = 0;
    double averagePointsPerTile = 0.0;
    std::size_t splitCount = 0;
    std::vector<PointBudgetTile> tiles;
};

constexpr const char* kPointTileManifestFormat =
    "usd-pointcloud-tile-manifest-v1";

struct PointTileManifestEntry {
    PointTileId id;
    std::uint32_t lod = 0;
    usdgeo::SpatialBounds bounds;
    std::uint64_t pointCount = 0;
    std::string payloadPath;

    bool IsValid() const noexcept;
};

struct PointTileManifest {
    std::vector<PointTileManifestEntry> entries;
};

bool ValidatePointTileManifest(
    const PointTileManifest& manifest,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool SerializePointTileManifest(
    const PointTileManifest& manifest,
    std::string& serialized,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool ValidatePointBudgetConfig(
    const PointBudgetConfig& config,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool BuildPointBudgetPlan(
    const std::vector<usdgeo::Vec3d>& sourcePositions,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics);

using PointStreamFactory =
    std::function<std::unique_ptr<PointStream>()>;

bool BuildPointBudgetPlan(
    const PointStreamFactory& streamFactory,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool ValidateTilePlan(
    const TilePlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool BuildTilePlan(
    const PointBudgetPlan& budgetPlan,
    TilePlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics);

usdgeo::CacheArguments TilePlanCacheArguments(const TilePlan& plan);
std::string StableTilePlanKey(const TilePlan& plan);

class TileRouter {
public:
    virtual ~TileRouter() = default;

    virtual PointTileId GetTileId(
        const usdgeo::Vec3d& sourcePosition) const noexcept = 0;
    virtual bool IsValid() const noexcept = 0;
};

class FixedGridTileRouter final : public TileRouter {
public:
    explicit FixedGridTileRouter(TileGridConfig config) noexcept;

    PointTileId GetTileId(
        const usdgeo::Vec3d& sourcePosition) const noexcept override;
    bool IsValid() const noexcept override;

private:
    TileGridConfig config_;
};

class PointBudgetTileRouter final : public TileRouter {
public:
    explicit PointBudgetTileRouter(const PointBudgetPlan& plan);
    explicit PointBudgetTileRouter(const TilePlan& plan);

    PointTileId GetTileId(
        const usdgeo::Vec3d& sourcePosition) const noexcept override;
    bool IsValid() const noexcept override;

private:
    std::vector<PointBudgetTile> tiles_;
    double rootMinX_ = 0.0;
    double rootMaxX_ = 0.0;
    double rootMinY_ = 0.0;
    double rootMaxY_ = 0.0;
};

} // namespace usdpointcloud
