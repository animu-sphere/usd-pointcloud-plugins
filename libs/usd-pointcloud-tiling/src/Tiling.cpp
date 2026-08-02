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
    const auto minimum = static_cast<long double>(
        (std::numeric_limits<std::int64_t>::min)());
    const auto maximum = static_cast<long double>(
        (std::numeric_limits<std::int64_t>::max)());
    if (index < minimum || index > maximum) {
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
