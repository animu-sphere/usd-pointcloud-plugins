#pragma once

#include "usdgeo/SpatialBounds.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace usdpointcloud {

enum class PointAttributeType {
    Int32,
    Int16,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
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
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    std::vector<PointAttribute> attributes;

    bool IsValid() const noexcept;
};

struct PointRange {
    std::uint64_t firstPoint = 0;
    std::uint64_t pointCount = 0;

    bool IsValid() const noexcept;
};

struct PointReadOptions {
    std::size_t chunkPointLimit = 65536;
    std::size_t memoryBudgetBytes = 64 * 1024 * 1024;
    PointRange range;
    std::function<bool()> isCancelled;

    bool IsValid() const noexcept;
};

} // namespace usdpointcloud