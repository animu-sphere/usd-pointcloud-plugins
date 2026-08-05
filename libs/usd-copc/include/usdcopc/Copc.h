#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdgeo/SpatialBounds.h"
#include "usdgeo/TileId.h"
#include "usdlas/Las.h"

#include <cstdint>
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

    bool ReadMetadata(CopcHeader& header,
                      std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadHierarchy(const CopcHeader& header,
                       std::vector<CopcHierarchyEntry>& entries,
                       std::vector<usdgeo::Diagnostic>& diagnostics);
    bool BuildHierarchy(const CopcHeader& header,
                        const std::vector<CopcHierarchyEntry>& entries,
                        CopcHierarchy& hierarchy,
                        std::vector<usdgeo::Diagnostic>& diagnostics) const;
    bool ReadPointData(const CopcHeader& header,
                       const CopcHierarchyEntry& entry,
                       std::vector<std::uint8_t>& bytes,
                       std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadPoints(const CopcHeader& header,
                    const CopcHierarchyEntry& entry,
                    std::vector<usdlas::LasPoint>& points,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

    CopcReadFailure FailureKind() const noexcept;

private:
    std::string filename_;
    CopcReadFailure failureKind_ = CopcReadFailure::None;
};

} // namespace usdcopc
