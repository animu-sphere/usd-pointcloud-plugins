#pragma once

#include "usdgeo/SpatialBounds.h"

#include <cstdint>
#include <string>
#include <vector>

namespace usdpointcloud {

enum class PointAttributeType {
    Int32,
    UInt8,
    UInt16,
    UInt32,
    Float32,
    Float64
};

struct PointAttribute {
    std::string name;
    PointAttributeType type = PointAttributeType::Float32;

    bool IsValid() const noexcept;
};

struct PointChunk {
    std::uint64_t pointCount = 0;
    usdgeo::SpatialBounds bounds;
    std::vector<PointAttribute> attributes;

    bool IsValid() const noexcept;
};

} // namespace usdpointcloud