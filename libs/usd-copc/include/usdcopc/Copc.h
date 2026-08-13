#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/SpatialBounds.h"
#include "usdgeo/TileId.h"
#include "usdpointcloud/Lod.h"
#include "usdlas/Las.h"
#include "usdlaz/Laz.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace usdcopc {

struct CopcInfo {
    double centerX = 0.0;
    double centerY = 0.0;
    double centerZ = 0.0;
    double halfSize = 0.0;
    double spacing = 0.0;
    std::uint64_t rootHierarchyOffset = 0;
    std::uint64_t rootHierarchySize = 0;
    double gpsTimeMin = 0.0;
    double gpsTimeMax = 0.0;

    bool IsValid() const noexcept;
};

struct CopcHierarchyEntry {
    std::int32_t level = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t pointCount = 0;
    std::uint64_t offset = 0;
    std::int32_t byteSize = 0;

    bool IsValid() const noexcept;
    bool IsHierarchyPage() const noexcept;
    bool IsPointData() const noexcept;
};

struct CopcHeader {
    usdlas::LasHeader las;
    CopcInfo info;
    std::uint64_t fileSize = 0;

    bool IsValid() const noexcept;
};

struct CopcNode {
    usdgeo::TileId tile;
    usdgeo::SpatialBounds bounds;
    double spacing = 0.0;
    std::uint64_t pointCount = 0;
    std::uint64_t pointDataOffset = 0;
    std::uint64_t pointDataSize = 0;
    std::uint64_t hierarchyPageOffset = 0;
    std::uint64_t hierarchyPageSize = 0;
    bool hasPointData = false;
    bool hasHierarchyPage = false;
    bool hasEmptyNode = false;
    std::vector<usdgeo::TileId> children;

    bool IsValid() const noexcept;
};

struct CopcHierarchy {
    usdgeo::SpatialBounds bounds;
    double spacing = 0.0;
    std::vector<CopcNode> nodes;

    bool IsValid() const noexcept;
};

struct CopcPointTile {
    usdpointcloud::PointTile tile;
    std::uint64_t pointDataOffset = 0;
    std::uint64_t pointDataSize = 0;

    bool IsValid() const noexcept;
};

enum class CopcReadFailure {
    None,
    FileOpen,
    LasMetadata,
    MissingInfo,
    InvalidInfo,
    Hierarchy,
};

class CopcReader {
public:
    explicit CopcReader(std::string filename);
    explicit CopcReader(std::shared_ptr<usdgeo::RandomAccessSource> source);

    bool ReadMetadata(CopcHeader& header,
                      std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadHierarchy(const CopcHeader& header,
                       std::vector<CopcHierarchyEntry>& entries,
                       std::vector<usdgeo::Diagnostic>& diagnostics);
    bool BuildHierarchy(const CopcHeader& header,
                        const std::vector<CopcHierarchyEntry>& entries,
                        CopcHierarchy& hierarchy,
                        std::vector<usdgeo::Diagnostic>& diagnostics) const;
    bool BuildPointTiles(
        const CopcHierarchy& hierarchy,
        std::vector<CopcPointTile>& tiles,
        std::vector<usdgeo::Diagnostic>& diagnostics) const;
    bool ReadPointData(const CopcHeader& header,
                       const CopcHierarchyEntry& entry,
                       std::vector<std::uint8_t>& bytes,
                       std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadPoints(const CopcHeader& header,
                    const CopcHierarchyEntry& entry,
                    std::vector<usdlas::LasPoint>& points,
                    std::vector<usdgeo::Diagnostic>& diagnostics);
    using PointConsumer =
        std::function<bool(const usdlas::LasPoint&, std::uint64_t)>;
    bool ReadPoints(const CopcHeader& header,
                    const CopcHierarchyEntry& entry,
                    const PointConsumer& consume,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

    CopcReadFailure FailureKind() const noexcept;

private:
    std::string filename_;
    std::shared_ptr<usdgeo::RandomAccessSource> source_;
    CopcReadFailure failureKind_ = CopcReadFailure::None;
};

class CopcPointStream final : public usdpointcloud::PointStream {
public:
    ~CopcPointStream() override;

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override;
    std::uint64_t SourceBytesRead() const noexcept override;

    const CopcHeader& Header() const noexcept;
    CopcReadFailure FailureKind() const noexcept;

private:
    friend std::unique_ptr<CopcPointStream> OpenCopcPointStream(
        const std::string&,
        const usdpointcloud::PointReadOptions&,
        CopcHeader&,
        std::vector<usdgeo::Diagnostic>&);
    friend std::unique_ptr<CopcPointStream> OpenCopcPointStream(
        std::shared_ptr<usdgeo::RandomAccessSource>,
        const usdpointcloud::PointReadOptions&,
        CopcHeader&,
        std::vector<usdgeo::Diagnostic>&,
        std::string);

    CopcPointStream(std::shared_ptr<usdgeo::RandomAccessSource> source,
                    std::string sourceName,
                    usdpointcloud::PointReadOptions options,
                    CopcHeader header,
                    std::vector<CopcHierarchyEntry> entries,
                    std::size_t maximumPoints);

    std::shared_ptr<usdgeo::RandomAccessSource> source_;
    std::string sourceName_;
    usdpointcloud::PointReadOptions options_;
    CopcHeader header_;
    std::unique_ptr<usdlaz::LazChunkDecoder> decoder_;
    std::vector<CopcHierarchyEntry> entries_;
    std::vector<usdlas::LasPoint> pendingPoints_;
    std::size_t entryIndex_ = 0;
    std::size_t pendingIndex_ = 0;
    std::size_t maximumPoints_ = 0;
    std::uint64_t pointsRead_ = 0;
    CopcReadFailure failureKind_ = CopcReadFailure::None;
    bool ended_ = false;
};

std::unique_ptr<CopcPointStream> OpenCopcPointStream(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics);

std::unique_ptr<CopcPointStream> OpenCopcPointStream(
    std::shared_ptr<usdgeo::RandomAccessSource> source,
    const usdpointcloud::PointReadOptions& options,
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics,
    std::string sourceName = {});

} // namespace usdcopc
