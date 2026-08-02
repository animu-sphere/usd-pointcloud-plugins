#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/TileId.h"
#include "usdgeo/SpatialBounds.h"
#include "usdpointcloud/Lod.h"

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
