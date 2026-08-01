#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/SpatialBounds.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usdlas {

struct LasVariableLengthRecord {
    std::string userId;
    std::uint16_t recordId = 0;
    std::string description;
    std::vector<std::uint8_t> data;
    bool isExtended = false;
};

struct LasGeoTiffKey {
    std::uint16_t keyId = 0;
    std::uint16_t tiffTagLocation = 0;
    std::uint16_t count = 0;
    std::uint16_t valueOffset = 0;
};

struct LasGeoTiffMetadata {
    std::uint16_t keyDirectoryVersion = 0;
    std::uint16_t keyRevision = 0;
    std::uint16_t minorRevision = 0;
    std::vector<LasGeoTiffKey> keys;
    std::vector<double> doubleParameters;
    std::string asciiParameters;
};

struct LasExtraBytesDescriptor {
    std::uint8_t dataType = 0;
    std::uint8_t options = 0;
    std::string name;
    usdgeo::Vec3d noData;
    usdgeo::Vec3d minimum;
    usdgeo::Vec3d maximum;
    usdgeo::Vec3d scale;
    usdgeo::Vec3d offset;
    std::string description;
};

struct LasWaveformPacket {
    std::uint8_t descriptorIndex = 0;
    std::uint64_t dataOffset = 0;
    std::uint32_t packetSize = 0;
    float returnPointLocation = 0.0f;
    float xt = 0.0f;
    float yt = 0.0f;
    float zt = 0.0f;
    bool external = false;
};

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
    std::uint32_t variableLengthRecordCount = 0;
    std::uint64_t firstExtendedVariableLengthRecordOffset = 0;
    std::uint32_t extendedVariableLengthRecordCount = 0;
    std::vector<LasVariableLengthRecord> variableLengthRecords;
    std::string crsWkt;
    std::optional<LasGeoTiffMetadata> geoTiffMetadata;
    std::vector<LasExtraBytesDescriptor> extraBytes;

    bool IsValid() const noexcept;
};

struct LasPoint {
    usdgeo::Vec3d sourcePosition;
    std::uint16_t intensity = 0;
    std::uint8_t returnNumber = 0;
    std::uint8_t numberOfReturns = 0;
    std::uint8_t classification = 0;
    std::uint8_t classificationFlags = 0;
    std::uint8_t scannerChannel = 0;
    std::uint8_t scanDirectionFlag = 0;
    std::uint8_t edgeOfFlightLine = 0;
    std::uint8_t userData = 0;
    std::int16_t scanAngle = 0;
    std::uint16_t pointSourceId = 0;
    std::uint16_t red = 0;
    std::uint16_t green = 0;
    std::uint16_t blue = 0;
    std::uint16_t nir = 0;
    double gpsTime = 0.0;
    LasWaveformPacket waveform;
    bool hasColor = false;
    bool hasGpsTime = false;
    bool hasWaveform = false;
};

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::string& error);

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics);

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::string& error);

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics);

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::string& error);

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

bool ParseKnownMetadata(const std::vector<LasVariableLengthRecord>& records,
                        LasHeader& header,
                        std::string& error);

bool ParseKnownMetadata(const std::vector<LasVariableLengthRecord>& records,
                        LasHeader& header,
                        std::vector<usdgeo::Diagnostic>& diagnostics);

std::string ExtractWktCrs(const std::vector<LasVariableLengthRecord>& records);

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::string& error);

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::vector<usdgeo::Diagnostic>& diagnostics);

} // namespace usdlas