#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/TileId.h"
#include "usdgeo/SpatialBounds.h"
#include "usdpointcloud/Lod.h"

#include <cstddef>
#include <cstdint>
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

struct PointBudgetPlan {
    std::size_t pointCount = 0;
    std::size_t tileCount = 0;
    std::size_t minimumPointsPerTile = 0;
    std::size_t maximumPointsPerTile = 0;
    double averagePointsPerTile = 0.0;
    std::size_t splitCount = 0;
    std::int32_t depth = 0;
};

bool ValidatePointBudgetConfig(
    const PointBudgetConfig& config,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool BuildPointBudgetPlan(
    const std::vector<usdgeo::Vec3d>& sourcePositions,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics);

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

} // namespace usdpointcloud
