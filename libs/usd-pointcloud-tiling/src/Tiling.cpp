#include "usdpointcloud/Tiling.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>

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
    const std::vector<usdgeo::Vec3d>& sourcePositions,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    if (!ValidatePointBudgetConfig(config, diagnostics)) return false;

    if (sourcePositions.empty()) return true;
    for (const auto& position : sourcePositions) {
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
            AddError(diagnostics, usdgeo::DiagnosticCode::NonFiniteCoordinate,
                     "point-budget planning requires finite horizontal coordinates");
            return false;
        }
    }

    struct Bounds {
        double minX;
        double maxX;
        double minY;
        double maxY;
    };
    const auto makeBounds = [&](const std::vector<std::size_t>& indices) {
        Bounds bounds{sourcePositions[indices.front()].x,
                      sourcePositions[indices.front()].x,
                      sourcePositions[indices.front()].y,
                      sourcePositions[indices.front()].y};
        for (const auto index : indices) {
            const auto& position = sourcePositions[index];
            bounds.minX = std::min(bounds.minX, position.x);
            bounds.maxX = std::max(bounds.maxX, position.x);
            bounds.minY = std::min(bounds.minY, position.y);
            bounds.maxY = std::max(bounds.maxY, position.y);
        }
        return bounds;
    };

    bool budgetSatisfied = true;
    const std::function<void(const std::vector<std::size_t>&, std::int32_t)>
        visit = [&](const std::vector<std::size_t>& indices,
                    std::int32_t depth) {
            plan.depth = std::max(plan.depth, depth);
            const auto bounds = makeBounds(indices);
            const bool canSplit = indices.size() > config.maxPointsPerTile &&
                                  depth < config.maxDepth &&
                                  bounds.minX != bounds.maxX &&
                                  bounds.minY != bounds.maxY;
            if (!canSplit) {
                ++plan.tileCount;
                plan.maximumPointsPerTile =
                    std::max(plan.maximumPointsPerTile, indices.size());
                if (indices.size() > config.maxPointsPerTile) {
                    budgetSatisfied = false;
                }
                return;
            }

            const double splitX = bounds.minX +
                                  (bounds.maxX - bounds.minX) * 0.5;
            const double splitY = bounds.minY +
                                  (bounds.maxY - bounds.minY) * 0.5;
            std::array<std::vector<std::size_t>, 4> children;
            for (const auto index : indices) {
                const auto& position = sourcePositions[index];
                const std::size_t child = (position.x >= splitX ? 1 : 0) +
                                          (position.y >= splitY ? 2 : 0);
                children[child].push_back(index);
            }
            std::size_t nonEmptyChildren = 0;
            bool childrenMeetMinimum = true;
            for (const auto& child : children) {
                if (!child.empty()) {
                    ++nonEmptyChildren;
                    childrenMeetMinimum =
                        childrenMeetMinimum &&
                        child.size() >= config.minPointsPerTile;
                }
            }
            if (nonEmptyChildren < 2 || !childrenMeetMinimum) {
                ++plan.tileCount;
                plan.maximumPointsPerTile =
                    std::max(plan.maximumPointsPerTile, indices.size());
                budgetSatisfied = false;
                return;
            }
            for (const auto& child : children) {
                if (!child.empty()) visit(child, depth + 1);
            }
        };

    std::vector<std::size_t> indices(sourcePositions.size());
    std::iota(indices.begin(), indices.end(), 0);
    visit(indices, 0);
    if (!budgetSatisfied) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "point budget cannot be satisfied for the source distribution");
    }
    return budgetSatisfied;
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
