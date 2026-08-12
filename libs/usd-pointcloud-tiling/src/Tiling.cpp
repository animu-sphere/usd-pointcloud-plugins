#include "usdpointcloud/Tiling.h"

#include <cmath>
#include <limits>

namespace usdpointcloud {
namespace {

void AddError(std::vector<usdgeo::Diagnostic>& diagnostics,
              usdgeo::DiagnosticCode code,
              const char* message) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message});
}

bool ToTileCoordinate(double coordinate,
                      double tileSize,
                      std::int64_t& result) noexcept {
    const long double index = std::floor(static_cast<long double>(coordinate) /
                                         static_cast<long double>(tileSize));
    // Use the exclusive power-of-two upper bound because INT64_MAX can round
    // to 2^63 when long double has double precision, as it does on MSVC.
    constexpr long double minimum = -9223372036854775808.0L;
    constexpr long double maximumExclusive = 9223372036854775808.0L;
    if (index < minimum || index >= maximumExclusive) {
        return false;
    }

    result = static_cast<std::int64_t>(index);
    return true;
}

} // namespace

bool TileGridConfig::IsValid() const noexcept {
    return level >= 0 && std::isfinite(tileSize) && tileSize > 0.0;
}

bool ValidateTileGridConfig(const TileGridConfig& config,
                            std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!std::isfinite(config.tileSize) || config.tileSize <= 0.0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "tile size must be finite and greater than zero");
    }
    if (config.level < 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTileId,
                 "tile level must not be negative");
    }
    return config.IsValid();
}

bool PointBudgetConfig::IsValid() const noexcept {
    return maxPointsPerTile > 0 && minPointsPerTile <= maxPointsPerTile &&
           maxDepth >= 0;
}

bool ValidatePointBudgetConfig(
    const PointBudgetConfig& config,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (config.maxPointsPerTile == 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "maximum points per tile must be greater than zero");
    }
    if (config.minPointsPerTile > config.maxPointsPerTile) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "minimum points per tile must not exceed the maximum");
    }
    if (config.maxDepth < 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTileId,
                 "maximum tile depth must not be negative");
    }
    return config.IsValid();
}

bool BuildPointBudgetPlan(
    std::size_t sourcePointCount,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    if (!ValidatePointBudgetConfig(config, diagnostics)) return false;

    plan.sourcePointCount = sourcePointCount;
    if (sourcePointCount == 0) return true;

    std::size_t tileCount = 1;
    for (std::int32_t depth = 0; depth <= config.maxDepth; ++depth) {
        plan.depth = depth;
        plan.tileCount = tileCount;
        plan.maximumPointsPerTile = sourcePointCount / tileCount;
        if (sourcePointCount % tileCount != 0) ++plan.maximumPointsPerTile;
        if (plan.maximumPointsPerTile <= config.maxPointsPerTile ||
            plan.maximumPointsPerTile <= config.minPointsPerTile) {
            return true;
        }
        if (tileCount > std::numeric_limits<std::size_t>::max() / 4) break;
        tileCount *= 4;
    }

    AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
             "point budget cannot be satisfied within maximum tile depth");
    plan = {};
    return false;
}

FixedGridTileRouter::FixedGridTileRouter(TileGridConfig config) noexcept
    : config_(config) {}

PointTileId FixedGridTileRouter::GetTileId(
    const usdgeo::Vec3d& sourcePosition) const noexcept {
    PointTileId tile;
    tile.level = config_.level;
    if (!IsValid() || !std::isfinite(sourcePosition.x) ||
        !std::isfinite(sourcePosition.y) ||
        !ToTileCoordinate(sourcePosition.x, config_.tileSize, tile.x) ||
        !ToTileCoordinate(sourcePosition.y, config_.tileSize, tile.y)) {
        tile.level = -1;
    }
    return tile;
}

bool FixedGridTileRouter::IsValid() const noexcept {
    return config_.IsValid();
}

} // namespace usdpointcloud
