#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdpointcloud/PointCloud.h"
#include "usdpointcloud/Lod.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <variant>
#include <vector>

namespace usdpointcloud {

constexpr std::uint32_t kPointSpoolSchemaVersion = 2;

enum class SpoolCoordinateSpace : std::uint8_t {
    SourceAndStage = 1,
};

struct SpoolSchema {
    std::uint32_t version = kPointSpoolSchemaVersion;
    SpoolCoordinateSpace coordinateSpace = SpoolCoordinateSpace::SourceAndStage;
    std::vector<PointAttribute> attributes;

    bool IsValid() const noexcept;
};

using SpoolAttributeValue = std::variant<
    std::int32_t,
    std::int16_t,
    std::uint8_t,
    std::uint16_t,
    std::uint32_t,
    std::uint64_t,
    float,
    double>;

struct SpoolPoint {
    usdgeo::Vec3d sourcePosition;
    usdgeo::Vec3d stagePosition;
    std::vector<SpoolAttributeValue> attributes;
};

class TileSpoolWriter final {
public:
    TileSpoolWriter();
    ~TileSpoolWriter();

    bool Open(const std::filesystem::path& path,
              const PointTileId& tile,
              const SpoolSchema& schema,
              std::size_t memoryLimitBytes,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool Append(const SpoolPoint& point,
                std::vector<usdgeo::Diagnostic>& diagnostics);
    bool Flush(std::vector<usdgeo::Diagnostic>& diagnostics);
    bool Close(std::vector<usdgeo::Diagnostic>& diagnostics);

    std::size_t BufferedBytes() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class TileSpoolReader final {
public:
    TileSpoolReader();
    ~TileSpoolReader();

    bool Open(const std::filesystem::path& path,
              PointTileId& tile,
              SpoolSchema& schema,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadNext(SpoolPoint& point,
                  std::vector<usdgeo::Diagnostic>& diagnostics);
    bool IsComplete() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

bool RemoveSpoolDirectory(const std::filesystem::path& directory,
                          std::vector<usdgeo::Diagnostic>& diagnostics);

} // namespace usdpointcloud