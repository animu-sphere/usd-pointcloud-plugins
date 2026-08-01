#pragma once

#include "usdgeo/GeoReference.h"
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

struct PointData {
    std::vector<usdgeo::Vec3d> positions;
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
    std::vector<std::uint8_t> waveformDescriptorIndex;
    std::vector<std::uint64_t> waveformDataOffset;
    std::vector<std::uint32_t> waveformPacketSize;
    std::vector<float> returnPointWaveformLocation;
    std::vector<float> waveformXt;
    std::vector<float> waveformYt;
    std::vector<float> waveformZt;
    std::vector<std::uint8_t> waveformDataExternal;
    std::string waveformDataFile;

    bool IsValid() const noexcept;
};

struct PointCloudAsset {
    usdgeo::GeoReference reference;
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    PointChunk chunk;
    PointData data;

    bool IsValid() const noexcept;
};

PointChunk MakePointChunk(const PointData& data,
                         const usdgeo::SpatialBounds& bounds);

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