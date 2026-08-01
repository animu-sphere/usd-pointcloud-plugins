#pragma once

#include "usdgeo/GeoReference.h"
#include "usdpointcloud/PointCloud.h"

#include <pxr/usd/usd/stage.h>

#include <string>
#include <vector>

namespace usdgeo {

class PointCloudLayer {
public:
    static pxr::UsdStageRefPtr CreateStage();

    static bool AuthorPointCloud(const pxr::UsdStageRefPtr& stage,
                                 const std::string& primPath,
                                 const GeoReference& reference,
                                 const SpatialBounds& bounds,
                                 const usdpointcloud::PointChunk& chunk,
                                 const std::vector<Vec3d>& positions);
};

} // namespace usdgeo