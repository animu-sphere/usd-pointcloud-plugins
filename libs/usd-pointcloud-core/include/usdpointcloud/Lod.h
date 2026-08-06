#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/SpatialBounds.h"
#include "usdgeo/TileId.h"
#include "usdpointcloud/PointCloud.h"

#include <vector>

namespace usdpointcloud {

using PointTileId = usdgeo::TileId;

using PointSourceRange = PointRange;

struct PointLodItem {
    std::uint32_t index = 0;
    std::uint64_t pointCount = 0;
    usdgeo::SpatialBounds bounds;
    PointSourceRange sourceRange;
    double spacing = 0.0;

    bool IsValid() const noexcept;
};

struct PointLodHierarchy {
    usdgeo::SpatialBounds bounds;
    std::vector<PointLodItem> items;
    std::uint32_t defaultIndex = 0;
    std::vector<float> screenSizeThresholds;

    bool IsValid() const noexcept;
};

struct PointTile {
    PointTileId id;
    usdgeo::SpatialBounds bounds;
    std::vector<PointTileId> children;
    PointLodHierarchy lod;

    bool IsValid() const noexcept;
};

bool ValidatePointLodHierarchy(
    const PointLodHierarchy& hierarchy,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool ValidatePointTile(const PointTile& tile,
                       std::vector<usdgeo::Diagnostic>& diagnostics);

} // namespace usdpointcloud