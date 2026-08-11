#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/RandomAccessSource.h"
#include "usdgeo/SpatialBounds.h"
#include "usdpointcloud/PointCloud.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
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
    std::optional<int> epsgCode;
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
    std::vector<double> extraBytes;
    LasWaveformPacket waveform;
    bool hasColor = false;
    bool hasGpsTime = false;
    bool hasWaveform = false;
};

using LasReadOptions = usdpointcloud::PointReadOptions;
using LasPointChunkConsumer =
    std::function<bool(const LasHeader&, const std::vector<LasPoint>&)>;
using LasPointChunkErrorConsumer = std::function<bool(
    const LasHeader&, const std::vector<LasPoint>&, std::string&)>;

bool MatchesReadOptions(const LasPoint& point,
                        const LasReadOptions& options) noexcept;

bool AppendPointData(const LasHeader& header,
                     const std::vector<LasPoint>& points,
                     const std::string& sourceFilename,
                     usdpointcloud::PointData& data,
                     std::string& error);

bool BuildPointCloudAsset(const LasHeader& header,
                          const usdpointcloud::PointData& data,
                          const std::string& missingCrsMessage,
                          usdpointcloud::PointCloudAsset& asset,
                          std::string& error);

enum class LasReadFailure;

bool ReadPointCloud(const std::string& filename,
                    const LasReadOptions& options,
                    const std::vector<std::string>& attributes,
                    const std::string& missingCrsMessage,
                    usdpointcloud::PointCloudAsset& asset,
                    LasReadFailure& failure,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

bool BuildPointCloudMetadata(const LasHeader& header,
                             usdpointcloud::PointChunk& chunk,
                             usdgeo::GeoReference& reference,
                             usdgeo::SpatialBounds& bounds,
                             std::string& error);

enum class LasReadFailure {
    None,
    InvalidRequest,
    Asset,
    FileOpen,
    FileSize,
    Header,
    Vlr,
    EvlrOffset,
    Evlr,
    PointDataTruncated,
    PointDataSeek,
    PointDataRead,
    PointDecode,
    Other,
};

class LasReader {
public:
    explicit LasReader(std::string filename);
    explicit LasReader(std::shared_ptr<usdgeo::RandomAccessSource> source);

    bool Read(const LasReadOptions& options,
              const LasPointChunkConsumer& consume,
              LasHeader& header,
              std::string& error);
    bool Read(const LasReadOptions& options,
              const LasPointChunkConsumer& consume,
              LasHeader& header,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool Read(const LasReadOptions& options,
              const LasPointChunkErrorConsumer& consume,
              LasHeader& header,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadMetadata(LasHeader& header,
                      std::vector<usdgeo::Diagnostic>& diagnostics,
                      std::size_t memoryBudgetBytes =
                          (std::numeric_limits<std::size_t>::max)());

    LasReadFailure FailureKind() const noexcept;

private:
    friend class LasPointStream;

    bool ReadPoints(const LasReadOptions& options,
                    const LasPointChunkConsumer& consume,
                    const LasHeader& header,
                    std::string& error);

    std::string filename_;
    std::shared_ptr<usdgeo::RandomAccessSource> source_;
    std::optional<usdgeo::Diagnostic> failureDiagnostic_;
    std::optional<std::uint64_t> failureByteOffset_;
    std::optional<std::uint64_t> failurePointIndex_;
    LasReadFailure failureKind_ = LasReadFailure::None;
};

class LasPointStream final : public usdpointcloud::PointStream {
public:
    ~LasPointStream() override;

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override;

    const LasHeader& Header() const noexcept;
    LasReadFailure FailureKind() const noexcept;

private:
    friend std::unique_ptr<LasPointStream> OpenLasPointStream(
        const std::string&,
        const LasReadOptions&,
        LasHeader&,
        std::vector<usdgeo::Diagnostic>&);

    LasPointStream(std::string filename,
                   LasReadOptions options,
                   LasHeader header,
                   std::size_t effectiveChunkPointLimit);

    std::string filename_;
    LasReader reader_;
    LasReadOptions options_;
    LasHeader header_;
    std::uint64_t nextPoint_ = 0;
    std::uint64_t endPoint_ = 0;
    std::size_t effectiveChunkPointLimit_ = 0;
    LasReadFailure failureKind_ = LasReadFailure::None;
};

std::unique_ptr<LasPointStream> OpenLasPointStream(
    const std::string& filename,
    const LasReadOptions& options,
    LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics);

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