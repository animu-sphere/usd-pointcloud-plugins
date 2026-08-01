#pragma once

#include "usdgeo/SpatialBounds.h"

#include <cstdint>
#include <string>
#include <vector>

namespace usdlas {

struct LasHeader {
    std::uint8_t versionMajor = 0;
    std::uint8_t versionMinor = 0;
    std::uint16_t headerSize = 0;
    std::uint32_t pointDataOffset = 0;
    std::uint8_t pointFormat = 0;
    std::uint16_t pointRecordLength = 0;
    std::uint64_t pointCount = 0;
    double xScale = 0.0;
    double yScale = 0.0;
    double zScale = 0.0;
    double xOffset = 0.0;
    double yOffset = 0.0;
    double zOffset = 0.0;
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();

    bool IsValid() const noexcept;
};

struct LasPoint {
    usdgeo::Vec3d sourcePosition;
    std::uint16_t intensity = 0;
    std::uint8_t returnNumber = 0;
    std::uint8_t numberOfReturns = 0;
    std::uint8_t classification = 0;
    std::uint16_t red = 0;
    std::uint16_t green = 0;
    std::uint16_t blue = 0;
    double gpsTime = 0.0;
    bool hasColor = false;
    bool hasGpsTime = false;
};

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::string& error);

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::string& error);

} // namespace usdlas