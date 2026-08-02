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
