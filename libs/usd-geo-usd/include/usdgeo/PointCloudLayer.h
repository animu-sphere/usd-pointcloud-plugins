#pragma once

#include "usdgeo/GeoReference.h"
#include "usdpointcloud/PointCloud.h"

#include <pxr/usd/usd/stage.h>

#include <string>
#include <cstdint>
#include <vector>

namespace usdgeo {

class PointCloudLayer {
public:
    struct Data {
        std::vector<Vec3d> positions;
        std::vector<std::uint16_t> intensity;
        std::vector<std::uint8_t> returnNumber;
        std::vector<std::uint8_t> numberOfReturns;
        std::vector<std::uint8_t> classification;
        std::vector<std::uint8_t> classificationFlags;
        std::vector<std::uint8_t> scannerChannel;
        std::vector<std::uint8_t> scanDirectionFlag;
        std::vector<std::uint8_t> edgeOfFlightLine;
        std::vector<std::uint8_t> userData;
        std::vector<std::int16_t> scanAngle;
        std::vector<std::uint16_t> pointSourceId;
        std::vector<std::uint16_t> red;
        std::vector<std::uint16_t> green;
        std::vector<std::uint16_t> blue;
        std::vector<std::uint16_t> nir;
        std::vector<double> gpsTime;
    };

    static pxr::UsdStageRefPtr CreateStage();

    static bool AuthorPointCloud(const pxr::UsdStageRefPtr& stage,
                                 const std::string& primPath,
                                 const GeoReference& reference,
                                 const SpatialBounds& bounds,
                                 const usdpointcloud::PointChunk& chunk,
                                 const std::vector<Vec3d>& positions);

    static bool AuthorPointCloud(const pxr::UsdStageRefPtr& stage,
                                 const std::string& primPath,
                                 const GeoReference& reference,
                                 const SpatialBounds& bounds,
                                 const usdpointcloud::PointChunk& chunk,
                                 const Data& data);
};

} // namespace usdgeo