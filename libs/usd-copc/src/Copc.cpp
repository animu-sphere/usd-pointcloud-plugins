#include "usdcopc/Copc.h"

#include "usdlaz/Laz.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::size_t kCopcInfoSize = 160;
constexpr std::size_t kHierarchyEntrySize = 32;

bool Has(const std::vector<std::uint8_t>& bytes,
         std::size_t offset,
         std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

template <typename T>
T ReadLittle(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(std::uint64_t));
    std::array<std::uint8_t, sizeof(T)> encoded{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        encoded[index] = bytes[offset + index];
    }
    const std::uint16_t marker = 1;
    const bool nativeLittleEndian =
        *reinterpret_cast<const std::uint8_t*>(&marker) == 1;
    if (!nativeLittleEndian) {
        std::reverse(encoded.begin(), encoded.end());
    }
    T value{};
    std::memcpy(&value, encoded.data(), sizeof(T));
    return value;
}

void AddDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                   usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::optional<std::uint64_t> byteOffset = std::nullopt) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message, byteOffset,
                           std::nullopt});
}

void AddStreamDiagnostic(usdgeo::DiagnosticCode code,
                         const std::string& message,
                         std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, std::nullopt, std::nullopt});
}

bool IsFileRangeValid(std::uint64_t fileSize,
                      std::uint64_t offset,
                      std::uint64_t size) {
    return offset <= fileSize && size <= fileSize - offset;
}

bool ReadFileRange(usdgeo::RandomAccessSource& source,
                   std::uint64_t fileSize,
                   std::uint64_t offset,
                   std::uint64_t size,
                   std::vector<std::uint8_t>& bytes,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (!IsFileRangeValid(fileSize, offset, size) ||
        size > static_cast<std::uint64_t>(maxSize)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC file range is outside the file", offset);
        return false;
    }

    return source.Read(offset, static_cast<std::size_t>(size), bytes,
                       diagnostics);
}

std::unique_ptr<usdlaz::LazChunkDecoder> OpenLazChunkDecoder(
    const std::shared_ptr<usdgeo::RandomAccessSource>& source,
    const usdcopc::CopcHeader& header,
    const usdcopc::CopcHierarchyEntry& entry,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!header.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC header is not valid");
        return nullptr;
    }
    if (!entry.IsValid() || !entry.IsPointData()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC entry does not identify point data");
        return nullptr;
    }

    if (!source) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC random-access source is missing");
        return nullptr;
    }

    auto offset = std::make_shared<std::uint64_t>(entry.offset);
    auto remaining = std::make_shared<std::uint64_t>(entry.byteSize);
    const usdlaz::LazInput input =
        [source, offset, remaining](unsigned char* output,
                                    std::size_t size) mutable {
            if (static_cast<std::uint64_t>(size) > *remaining) {
                throw std::runtime_error("COPC point data range is truncated");
            }
            std::vector<std::uint8_t> bytes;
            std::vector<usdgeo::Diagnostic> diagnostics;
            if (!source->Read(*offset, size, bytes, diagnostics) ||
                bytes.size() != size) {
                throw std::runtime_error(
                    diagnostics.empty()
                        ? "COPC point data range is truncated"
                        : diagnostics.front().message);
            }
            std::memcpy(output, bytes.data(), size);
            *offset += static_cast<std::uint64_t>(size);
            *remaining -= static_cast<std::uint64_t>(size);
        };
    return usdlaz::CreateLazChunkDecoder(
        header.las, static_cast<std::uint64_t>(entry.pointCount), input,
        diagnostics);
}

const usdlas::LasVariableLengthRecord* FindCopcInfo(
    const std::vector<usdlas::LasVariableLengthRecord>& records) {
    for (const auto& record : records) {
        if (record.userId == "copc" && record.recordId == 1) {
            return &record;
        }
    }
    return nullptr;
}

bool HasCopcHierarchyVlr(
    const std::vector<usdlas::LasVariableLengthRecord>& records) {
    for (const auto& record : records) {
        if (record.userId == "copc" && record.recordId == 1000) {
            return true;
        }
    }
    return false;
}

bool HasZeroRange(const std::vector<std::uint8_t>& bytes,
                  std::size_t offset) {
    for (std::size_t index = offset; index < bytes.size(); ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }
    return true;
}

bool ContainsBounds(const usdgeo::SpatialBounds& outer,
                    const usdgeo::SpatialBounds& inner) noexcept {
    return outer.minimum.x <= inner.minimum.x &&
           outer.minimum.y <= inner.minimum.y &&
           outer.minimum.z <= inner.minimum.z &&
           outer.maximum.x >= inner.maximum.x &&
           outer.maximum.y >= inner.maximum.y &&
           outer.maximum.z >= inner.maximum.z;
}

bool BuildNodeBounds(const usdcopc::CopcHeader& header,
                     const usdcopc::CopcHierarchyEntry& entry,
                     usdgeo::SpatialBounds& bounds,
                     double& spacing) {
    if (entry.level < 0 || entry.level > 31 || entry.x < 0 || entry.y < 0 ||
        entry.z < 0) {
        return false;
    }
    const auto gridSize = std::int64_t{1} << entry.level;
    if (entry.x >= gridSize || entry.y >= gridSize || entry.z >= gridSize) {
        return false;
    }

    const auto side = std::ldexp(header.info.halfSize * 2.0, -entry.level);
    spacing = std::ldexp(header.info.spacing, -entry.level);
    if (!std::isfinite(side) || side <= 0.0 || !std::isfinite(spacing) ||
        spacing <= 0.0) {
        return false;
    }

    const auto rootMinimum = usdgeo::Vec3d{
        header.info.centerX - header.info.halfSize,
        header.info.centerY - header.info.halfSize,
        header.info.centerZ - header.info.halfSize};
    bounds.minimum = {
        rootMinimum.x + static_cast<double>(entry.x) * side,
        rootMinimum.y + static_cast<double>(entry.y) * side,
        rootMinimum.z + static_cast<double>(entry.z) * side};
    bounds.maximum = {bounds.minimum.x + side, bounds.minimum.y + side,
                      bounds.minimum.z + side};
    return bounds.IsValid();
}

bool SameTile(const usdgeo::TileId& left,
              const usdgeo::TileId& right) noexcept {
    return left.level == right.level && left.x == right.x &&
           left.y == right.y && left.z == right.z;
}

} // namespace

namespace usdcopc {

bool CopcInfo::IsValid() const noexcept {
    return std::isfinite(centerX) && std::isfinite(centerY) &&
           std::isfinite(centerZ) && std::isfinite(halfSize) &&
           std::isfinite(spacing) && halfSize > 0.0 && spacing > 0.0 &&
           rootHierarchyOffset > 0 && rootHierarchySize > 0 &&
           std::isfinite(gpsTimeMin) && std::isfinite(gpsTimeMax) &&
           gpsTimeMin <= gpsTimeMax;
}

bool CopcHierarchyEntry::IsValid() const noexcept {
    return level >= 0 && pointCount >= -1 && byteSize >= 0 &&
           ((pointCount == 0 && offset == 0 && byteSize == 0) ||
            (pointCount != 0 && offset > 0 && byteSize > 0));
}

bool CopcHierarchyEntry::IsHierarchyPage() const noexcept {
    return pointCount == -1;
}

bool CopcHierarchyEntry::IsPointData() const noexcept {
    return pointCount > 0;
}

bool CopcHeader::IsValid() const noexcept {
    return las.IsValid() && las.versionMajor == 1 && las.versionMinor == 4 &&
           las.pointFormat >= 6 && las.pointFormat <= 8 && info.IsValid() &&
           fileSize > 0 &&
           info.rootHierarchySize % kHierarchyEntrySize == 0 &&
           IsFileRangeValid(fileSize, info.rootHierarchyOffset,
                            info.rootHierarchySize);
}

bool CopcNode::IsValid() const noexcept {
    const auto nodeKindCount = static_cast<int>(hasPointData) +
                               static_cast<int>(hasHierarchyPage) +
                               static_cast<int>(hasEmptyNode);
    if (!tile.IsValid() || !bounds.IsValid() || !std::isfinite(spacing) ||
        spacing <= 0.0 || nodeKindCount != 1) {
        return false;
    }
    if (hasPointData &&
        (pointCount == 0 || pointDataOffset == 0 || pointDataSize == 0)) {
        return false;
    }
    if (hasHierarchyPage &&
        (hierarchyPageOffset == 0 || hierarchyPageSize == 0)) {
        return false;
    }
    for (std::size_t index = 0; index < children.size(); ++index) {
        if (!children[index].IsValid()) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (SameTile(children[index], children[previous])) {
                return false;
            }
        }
    }
    return true;
}

bool CopcHierarchy::IsValid() const noexcept {
    if (!bounds.IsValid() || !std::isfinite(spacing) || spacing <= 0.0 ||
        nodes.empty()) {
        return false;
    }
    const usdgeo::TileId root{0, 0, 0, 0};
    bool hasRoot = false;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        if (!node.IsValid() || !ContainsBounds(bounds, node.bounds)) {
            return false;
        }
        if (SameTile(node.tile, root)) {
            hasRoot = true;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (SameTile(node.tile, nodes[previous].tile)) {
                return false;
            }
        }
    }
    return hasRoot;
}

bool CopcPointTile::IsValid() const noexcept {
    return tile.IsValid() && pointDataOffset > 0 && pointDataSize > 0;
}

bool BuildTilePlan(
    const CopcHierarchy& hierarchy,
    usdpointcloud::TilePlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    diagnostics.clear();
    if (!hierarchy.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC tile planning requires a valid hierarchy");
        return false;
    }

    std::map<std::string, std::size_t> nodeIndices;
    for (std::size_t index = 0; index < hierarchy.nodes.size(); ++index) {
        if (!nodeIndices.emplace(hierarchy.nodes[index].tile.ToString(), index)
                 .second) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                          "COPC hierarchy contains a duplicate tile identity");
            return false;
        }
    }

    std::vector<bool> required(hierarchy.nodes.size(), false);
    for (const auto& node : hierarchy.nodes) {
        if (!node.hasPointData) continue;

        auto current = node.tile;
        for (;;) {
            const auto found = nodeIndices.find(current.ToString());
            if (found == nodeIndices.end()) {
                AddDiagnostic(
                    diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                    "COPC point-data node has a missing hierarchy ancestor");
                return false;
            }
            required[found->second] = true;
            if (current.level == 0) break;
            current = {current.level - 1, current.x / 2, current.y / 2,
                       current.z / 2};
        }
    }

    const auto root = nodeIndices.find("L0/0/0/0");
    if (root == nodeIndices.end() || !required[root->second]) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC hierarchy has no point-data path from its root");
        return false;
    }

    std::vector<std::size_t> order;
    order.reserve(hierarchy.nodes.size());
    for (std::size_t index = 0; index < hierarchy.nodes.size(); ++index) {
        if (required[index]) order.push_back(index);
    }
    std::sort(order.begin(), order.end(), [&](std::size_t left,
                                               std::size_t right) {
        if (hierarchy.nodes[left].tile.level !=
            hierarchy.nodes[right].tile.level) {
            return hierarchy.nodes[left].tile.level >
                   hierarchy.nodes[right].tile.level;
        }
        return hierarchy.nodes[left].tile.ToString() >
               hierarchy.nodes[right].tile.ToString();
    });

    std::map<std::string, std::uint64_t> pointCounts;
    std::map<std::string, std::vector<usdgeo::TileId>> children;
    for (const auto index : order) {
        const auto& node = hierarchy.nodes[index];
        auto& pointCount = pointCounts[node.tile.ToString()];
        if (node.hasPointData) pointCount = node.pointCount;
        for (const auto& child : hierarchy.nodes) {
            if (child.tile.level != node.tile.level + 1 ||
                child.tile.x / 2 != node.tile.x ||
                child.tile.y / 2 != node.tile.y ||
                child.tile.z / 2 != node.tile.z ||
                !required[nodeIndices.at(child.tile.ToString())]) {
                continue;
            }
            const auto childCount = pointCounts[child.tile.ToString()];
            if (childCount > (std::numeric_limits<std::uint64_t>::max)() -
                                 pointCount) {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::InvalidPointTile,
                              "COPC tile-plan point count overflow");
                return false;
            }
            pointCount += childCount;
            children[node.tile.ToString()].push_back(child.tile);
        }
    }

    for (auto& entry : children) {
        std::sort(entry.second.begin(), entry.second.end(),
                  [](const auto& left, const auto& right) {
                      if (left.level != right.level) {
                          return left.level < right.level;
                      }
                      if (left.x != right.x) return left.x < right.x;
                      if (left.y != right.y) return left.y < right.y;
                      return left.z < right.z;
                  });
    }

    plan.plannerId = kCopcNativeHierarchyPlannerId;
    plan.plannerVersion = kCopcNativeHierarchyPlannerVersion;
    plan.nodes.reserve(order.size());
    for (const auto index : order) {
        const auto& node = hierarchy.nodes[index];
        usdpointcloud::TilePlanNode planNode;
        planNode.id = node.tile;
        planNode.bounds = node.bounds;
        planNode.pointCount = pointCounts[node.tile.ToString()];
        planNode.parent = {-1, 0, 0, 0};
        if (node.tile.level > 0) {
            planNode.parent = {node.tile.level - 1, node.tile.x / 2,
                               node.tile.y / 2, node.tile.z / 2};
        }
        planNode.children = children[node.tile.ToString()];
        planNode.isLeaf = planNode.children.empty();
        if (node.hasPointData) {
            planNode.sourceRanges.push_back(
                {node.pointDataOffset, node.pointDataSize});
        }
        plan.nodes.push_back(std::move(planNode));
    }

    std::sort(plan.nodes.begin(), plan.nodes.end(),
              [](const auto& left, const auto& right) {
                  if (left.id.level != right.id.level) {
                      return left.id.level < right.id.level;
                  }
                  if (left.id.x != right.id.x) return left.id.x < right.id.x;
                  if (left.id.y != right.id.y) return left.id.y < right.id.y;
                  return left.id.z < right.id.z;
              });
    if (!usdpointcloud::ValidateTilePlan(plan, diagnostics)) {
        plan = {};
        return false;
    }
    return true;
}

CopcReader::CopcReader(std::string filename)
    : filename_(std::move(filename)),
      source_(std::make_shared<usdgeo::LocalRandomAccessSource>(filename_)) {}

CopcReader::CopcReader(
    std::shared_ptr<usdgeo::RandomAccessSource> source)
    : source_(std::move(source)) {}

CopcPointStream::CopcPointStream(
    std::shared_ptr<usdgeo::RandomAccessSource> source,
    std::string sourceName,
    usdpointcloud::PointReadOptions options,
    CopcHeader header,
    std::vector<CopcHierarchyEntry> entries,
    std::size_t maximumPoints)
        : source_(std::move(source)),
            sourceName_(std::move(sourceName)),
      options_(std::move(options)),
      header_(std::move(header)),
      entries_(std::move(entries)),
      maximumPoints_(maximumPoints) {}

CopcPointStream::~CopcPointStream() = default;

usdpointcloud::PointStreamStatus CopcPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    chunk = {};
    data = {};
    diagnostic = {};
    if (ended_) {
        return usdpointcloud::PointStreamStatus::End;
    }

    while (true) {
        if (pendingIndex_ < pendingPoints_.size()) {
            const auto count = (std::min)(
                maximumPoints_, pendingPoints_.size() - pendingIndex_);
            std::vector<usdlas::LasPoint> points(
                pendingPoints_.begin() +
                    static_cast<std::ptrdiff_t>(pendingIndex_),
                pendingPoints_.begin() +
                    static_cast<std::ptrdiff_t>(pendingIndex_ + count));
            pendingIndex_ += count;

            std::string error;
            if (!usdlas::AppendPointData(header_.las, points, sourceName_, data,
                                         error)) {
                failureKind_ = CopcReadFailure::Hierarchy;
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error, error, std::nullopt,
                              pointsRead_};
                ended_ = true;
                return usdpointcloud::PointStreamStatus::Error;
            }
            usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
            for (const auto& position : data.positions) {
                bounds.Expand(position);
            }
            chunk = usdpointcloud::MakePointChunk(data, bounds);
            if (!chunk.IsValid() || chunk.pointCount == 0) {
                failureKind_ = CopcReadFailure::Hierarchy;
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error,
                              "COPC stream produced an invalid point chunk",
                              std::nullopt, pointsRead_};
                ended_ = true;
                return usdpointcloud::PointStreamStatus::Error;
            }
            pointsRead_ += chunk.pointCount;
            return usdpointcloud::PointStreamStatus::Chunk;
        }

        pendingPoints_.clear();
        pendingIndex_ = 0;
        if (decoder_) {
            if (options_.isCancelled && options_.isCancelled()) {
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error, "COPC read cancelled",
                              std::nullopt, pointsRead_};
                failureKind_ = CopcReadFailure::Hierarchy;
                ended_ = true;
                return usdpointcloud::PointStreamStatus::Error;
            }

            std::vector<usdlas::LasPoint> points;
            bool complete = false;
            std::vector<usdgeo::Diagnostic> diagnostics;
            if (!decoder_->ReadChunk(maximumPoints_, points, complete,
                                     diagnostics)) {
                diagnostic = diagnostics.empty()
                                 ? usdgeo::Diagnostic{
                                       usdgeo::DiagnosticCode::DecodeFailure,
                                       usdgeo::Severity::Error,
                                       "COPC point data decode failed",
                                       std::nullopt, pointsRead_}
                                 : diagnostics.front();
                failureKind_ = CopcReadFailure::Hierarchy;
                ended_ = true;
                return usdpointcloud::PointStreamStatus::Error;
            }
            if (points.empty() && !complete) {
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error,
                              "COPC decoder returned an empty incomplete chunk",
                              std::nullopt, pointsRead_};
                failureKind_ = CopcReadFailure::Hierarchy;
                ended_ = true;
                return usdpointcloud::PointStreamStatus::Error;
            }
            if (complete) {
                decoder_.reset();
            }
            for (const auto& point : points) {
                if (usdlas::MatchesReadOptions(point, options_)) {
                    pendingPoints_.push_back(point);
                }
            }
            continue;
        }

        if (entryIndex_ >= entries_.size()) {
            ended_ = true;
            return usdpointcloud::PointStreamStatus::End;
        }
        if (options_.isCancelled && options_.isCancelled()) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, "COPC read cancelled",
                          std::nullopt, pointsRead_};
            failureKind_ = CopcReadFailure::Hierarchy;
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }

        const auto& entry = entries_[entryIndex_++];
        if (!entry.IsPointData()) {
            continue;
        }
        std::vector<usdgeo::Diagnostic> diagnostics;
        decoder_ = OpenLazChunkDecoder(source_, header_, entry, diagnostics);
        if (!decoder_) {
            diagnostic = diagnostics.empty()
                             ? usdgeo::Diagnostic{
                                   usdgeo::DiagnosticCode::DecodeFailure,
                                   usdgeo::Severity::Error,
                                   "COPC point data decoder could not be opened",
                                   std::nullopt, pointsRead_}
                             : diagnostics.front();
            failureKind_ = CopcReadFailure::Hierarchy;
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
    }
}

const CopcHeader& CopcPointStream::Header() const noexcept {
    return header_;
}

std::uint64_t CopcPointStream::SourceBytesRead() const noexcept {
    return source_ ? source_->BytesRead() : 0;
}

CopcReadFailure CopcPointStream::FailureKind() const noexcept {
    return failureKind_;
}

std::unique_ptr<CopcPointStream> OpenCopcPointStream(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    return OpenCopcPointStream(
        std::make_shared<usdgeo::LocalRandomAccessSource>(filename), options,
        header, diagnostics, filename);
}

std::unique_ptr<CopcPointStream> OpenCopcPointStream(
    std::shared_ptr<usdgeo::RandomAccessSource> source,
    const usdpointcloud::PointReadOptions& options,
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics,
    std::string sourceName) {
    diagnostics.clear();
    header = {};
    if (!options.IsValid()) {
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "COPC stream options are invalid", diagnostics);
        return nullptr;
    }
    if (options.range.firstPoint != 0 || options.range.pointCount != 0) {
        AddStreamDiagnostic(
            usdgeo::DiagnosticCode::InvalidPointSourceRange,
            "COPC streams do not support LAS source point ranges", diagnostics);
        return nullptr;
    }

    CopcReader reader(source);
    if (!reader.ReadMetadata(header, diagnostics)) {
        return nullptr;
    }
    std::vector<CopcHierarchyEntry> entries;
    if (!reader.ReadHierarchy(header, entries, diagnostics)) {
        return nullptr;
    }

    const auto maximumSize = (std::numeric_limits<std::size_t>::max)();
    if (header.las.pointRecordLength >
        (maximumSize - sizeof(usdlas::LasPoint)) / 2) {
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "COPC point record size is invalid", diagnostics);
        return nullptr;
    }
    const auto bytesPerPoint =
        sizeof(usdlas::LasPoint) +
        static_cast<std::size_t>(header.las.pointRecordLength) * 2;
    const auto maximumPoints =
        (std::min)(options.chunkPointLimit,
                   options.memoryBudgetBytes / bytesPerPoint);
    if (maximumPoints == 0) {
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "COPC memory budget is too small for one point",
                            diagnostics);
        return nullptr;
    }
    return std::unique_ptr<CopcPointStream>(new CopcPointStream(
        std::move(source), std::move(sourceName), options, std::move(header),
        std::move(entries), maximumPoints));
}

bool CopcReader::ReadMetadata(
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    failureKind_ = CopcReadFailure::None;
    header = {};

    if (!source_) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC random-access source is missing");
        failureKind_ = CopcReadFailure::FileOpen;
        return false;
    }
    usdlas::LasReader lasReader(source_);
    if (!lasReader.ReadMetadata(header.las, diagnostics)) {
        failureKind_ = CopcReadFailure::LasMetadata;
        return false;
    }
    if (header.las.versionMajor != 1 || header.las.versionMinor != 4) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::UnsupportedVersion,
                      "COPC requires LAS version 1.4");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }
    if (header.las.pointFormat < 6 || header.las.pointFormat > 8) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::UnsupportedPointFormat,
                      "COPC requires LAS point format 6 through 8");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }

    const auto* infoRecord = FindCopcInfo(header.las.variableLengthRecords);
    if (infoRecord == nullptr || header.las.variableLengthRecords.empty() ||
        &header.las.variableLengthRecords.front() != infoRecord ||
        infoRecord->isExtended) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "COPC info VLR must be the first standard VLR");
        failureKind_ = CopcReadFailure::MissingInfo;
        return false;
    }
    if (infoRecord->data.size() != kCopcInfoSize) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::TruncatedRecord,
                      "COPC info VLR must contain 160 bytes");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }
    if (!HasZeroRange(infoRecord->data, 72)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC info VLR reserved fields must be zero");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }
    if (!HasCopcHierarchyVlr(header.las.variableLengthRecords)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "COPC hierarchy VLR is missing");
        failureKind_ = CopcReadFailure::MissingInfo;
        return false;
    }

    if (!source_->GetSize(header.fileSize, diagnostics)) {
        if (diagnostics.empty()) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "cannot determine COPC source size");
        }
        failureKind_ = CopcReadFailure::FileOpen;
        return false;
    }

    const auto& data = infoRecord->data;
    header.info.centerX = ReadLittle<double>(data, 0);
    header.info.centerY = ReadLittle<double>(data, 8);
    header.info.centerZ = ReadLittle<double>(data, 16);
    header.info.halfSize = ReadLittle<double>(data, 24);
    header.info.spacing = ReadLittle<double>(data, 32);
    header.info.rootHierarchyOffset = ReadLittle<std::uint64_t>(data, 40);
    header.info.rootHierarchySize = ReadLittle<std::uint64_t>(data, 48);
    header.info.gpsTimeMin = ReadLittle<double>(data, 56);
    header.info.gpsTimeMax = ReadLittle<double>(data, 64);

    if (!header.info.IsValid() ||
        header.info.rootHierarchySize % kHierarchyEntrySize != 0 ||
        !IsFileRangeValid(header.fileSize, header.info.rootHierarchyOffset,
                          header.info.rootHierarchySize)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC hierarchy range is invalid",
                      header.info.rootHierarchyOffset);
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }

    return true;
}

bool CopcReader::ReadHierarchy(
    const CopcHeader& header,
    std::vector<CopcHierarchyEntry>& entries,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    failureKind_ = CopcReadFailure::None;
    entries.clear();
    if (!header.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC header is not valid");
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }

    std::unordered_set<std::uint64_t> visitedPages;
    std::uint64_t totalPointCount = 0;
    std::function<bool(std::uint64_t, std::uint64_t)> readPage;
    readPage = [&](std::uint64_t offset, std::uint64_t size) {
        if (size == 0 || size % kHierarchyEntrySize != 0 ||
            !IsFileRangeValid(header.fileSize, offset, size)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                          "COPC hierarchy page range is invalid", offset);
            return false;
        }
        if (!visitedPages.insert(offset).second) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "COPC hierarchy contains a repeated page", offset);
            return false;
        }

        std::vector<std::uint8_t> page;
        if (!source_ ||
            !ReadFileRange(*source_, header.fileSize, offset, size, page,
                           diagnostics)) {
            return false;
        }
        for (std::size_t pageOffset = 0; pageOffset < page.size();
             pageOffset += kHierarchyEntrySize) {
            CopcHierarchyEntry entry;
            entry.level = ReadLittle<std::int32_t>(page, pageOffset);
            entry.x = ReadLittle<std::int32_t>(page, pageOffset + 4);
            entry.y = ReadLittle<std::int32_t>(page, pageOffset + 8);
            entry.z = ReadLittle<std::int32_t>(page, pageOffset + 12);
            entry.offset = ReadLittle<std::uint64_t>(page, pageOffset + 16);
            entry.byteSize = ReadLittle<std::int32_t>(page, pageOffset + 24);
            entry.pointCount = ReadLittle<std::int32_t>(page, pageOffset + 28);

            const auto entryFileOffset = offset + pageOffset;
            if (!entry.IsValid()) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                              "COPC hierarchy entry is invalid",
                              entryFileOffset);
                return false;
            }
            if (entry.IsHierarchyPage()) {
                if (entry.byteSize % static_cast<std::int32_t>(kHierarchyEntrySize) != 0 ||
                    !IsFileRangeValid(header.fileSize, entry.offset,
                                      static_cast<std::uint64_t>(entry.byteSize))) {
                    AddDiagnostic(diagnostics,
                                  usdgeo::DiagnosticCode::InvalidOffset,
                                  "COPC child hierarchy page range is invalid",
                                  entryFileOffset);
                    return false;
                }
                entries.push_back(entry);
                if (!readPage(entry.offset,
                              static_cast<std::uint64_t>(entry.byteSize))) {
                    return false;
                }
                continue;
            }
            if (entry.IsPointData() &&
                !IsFileRangeValid(header.fileSize, entry.offset,
                                  static_cast<std::uint64_t>(entry.byteSize))) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                              "COPC point data range is invalid", entryFileOffset);
                return false;
            }
            if (entry.IsPointData()) {
                const auto pointCount = static_cast<std::uint64_t>(entry.pointCount);
                if (pointCount > header.las.pointCount - totalPointCount) {
                    AddDiagnostic(
                        diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                        "COPC hierarchy point count exceeds the LAS header count",
                        entryFileOffset);
                    return false;
                }
                totalPointCount += pointCount;
            }
            entries.push_back(entry);
        }
        return true;
    };

    if (!readPage(header.info.rootHierarchyOffset,
                  header.info.rootHierarchySize)) {
        failureKind_ = CopcReadFailure::Hierarchy;
        entries.clear();
        return false;
    }
    if (totalPointCount != header.las.pointCount) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC hierarchy point count does not match the LAS header count");
        failureKind_ = CopcReadFailure::Hierarchy;
        entries.clear();
        return false;
    }
    return true;
}

bool CopcReader::BuildHierarchy(
    const CopcHeader& header,
    const std::vector<CopcHierarchyEntry>& entries,
    CopcHierarchy& hierarchy,
    std::vector<usdgeo::Diagnostic>& diagnostics) const {
    hierarchy = {};
    if (!header.IsValid() || entries.empty()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC hierarchy model requires a valid header and entries");
        return false;
    }

    hierarchy.bounds = {{header.info.centerX - header.info.halfSize,
                         header.info.centerY - header.info.halfSize,
                         header.info.centerZ - header.info.halfSize},
                        {header.info.centerX + header.info.halfSize,
                         header.info.centerY + header.info.halfSize,
                         header.info.centerZ + header.info.halfSize}};
    hierarchy.spacing = header.info.spacing;
    hierarchy.nodes.reserve(entries.size());

    std::map<std::string, std::size_t> nodeIndices;
    for (const auto& entry : entries) {
        CopcNode node;
        node.tile = {entry.level, entry.x, entry.y, entry.z};
        if (!BuildNodeBounds(header, entry, node.bounds, node.spacing)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                          "COPC hierarchy node coordinates are invalid");
            hierarchy = {};
            return false;
        }
        if (entry.IsPointData()) {
            node.hasPointData = true;
            node.pointCount = static_cast<std::uint64_t>(entry.pointCount);
            node.pointDataOffset = entry.offset;
            node.pointDataSize = static_cast<std::uint64_t>(entry.byteSize);
        } else if (entry.IsHierarchyPage()) {
            node.hasHierarchyPage = true;
            node.hierarchyPageOffset = entry.offset;
            node.hierarchyPageSize = static_cast<std::uint64_t>(entry.byteSize);
        } else {
            node.hasEmptyNode = true;
        }

        const auto key = node.tile.ToString();
        if (!nodeIndices.emplace(key, hierarchy.nodes.size()).second) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                          "COPC hierarchy contains a duplicate node");
            hierarchy = {};
            return false;
        }
        hierarchy.nodes.push_back(std::move(node));
    }

    for (const auto& node : hierarchy.nodes) {
        if (node.tile.level == 0) {
            continue;
        }
        const usdgeo::TileId parent{
            node.tile.level - 1, node.tile.x / 2, node.tile.y / 2,
            node.tile.z / 2};
        const auto parentIndex = nodeIndices.find(parent.ToString());
        if (parentIndex != nodeIndices.end()) {
            hierarchy.nodes[parentIndex->second].children.push_back(node.tile);
        }
    }

    if (!hierarchy.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC hierarchy model is invalid");
        hierarchy = {};
        return false;
    }
    return true;
}

bool CopcReader::BuildPointTiles(
    const CopcHierarchy& hierarchy,
    std::vector<CopcPointTile>& tiles,
    std::vector<usdgeo::Diagnostic>& diagnostics) const {
    tiles.clear();
    diagnostics.clear();
    if (!hierarchy.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC point tiles require a valid hierarchy");
        return false;
    }

    std::map<std::string, std::size_t> tileIndices;
    for (const auto& node : hierarchy.nodes) {
        if (!node.hasPointData) {
            continue;
        }

        CopcPointTile tile;
        tile.tile.id = node.tile;
        tile.tile.bounds = node.bounds;
        tile.tile.lod.bounds = node.bounds;
        tile.tile.lod.items.push_back({
            0, node.pointCount, node.bounds, {0, node.pointCount},
            node.spacing});
        tile.pointDataOffset = node.pointDataOffset;
        tile.pointDataSize = node.pointDataSize;
        const auto key = node.tile.ToString();
        if (!tileIndices.emplace(key, tiles.size()).second) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                          "COPC point tiles contain a duplicate node");
            tiles.clear();
            return false;
        }
        tiles.push_back(std::move(tile));
    }

    if (tiles.empty()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                      "COPC hierarchy contains no point-data tiles");
        return false;
    }

    for (const auto& node : hierarchy.nodes) {
        if (!node.hasPointData || node.tile.level == 0) {
            continue;
        }
        auto parent = node.tile;
        while (parent.level > 0) {
            parent = {parent.level - 1, parent.x / 2, parent.y / 2,
                      parent.z / 2};
            const auto parentIndex = tileIndices.find(parent.ToString());
            if (parentIndex == tileIndices.end()) {
                continue;
            }
            tiles[parentIndex->second].tile.children.push_back(node.tile);
            break;
        }
    }

    for (const auto& tile : tiles) {
        if (!tile.IsValid()) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                          "COPC point tile model is invalid");
            tiles.clear();
            return false;
        }
    }
    return true;
}

bool CopcReader::ReadPointData(
    const CopcHeader& header,
    const CopcHierarchyEntry& entry,
    std::vector<std::uint8_t>& bytes,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    failureKind_ = CopcReadFailure::None;
    bytes.clear();
    if (!header.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC header is not valid");
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }
    if (!entry.IsValid() || !entry.IsPointData()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC entry does not identify point data");
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }
    if (!source_ ||
        !ReadFileRange(*source_, header.fileSize, entry.offset,
                       static_cast<std::uint64_t>(entry.byteSize), bytes,
                       diagnostics)) {
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }
    return true;
}

bool CopcReader::ReadPoints(
    const CopcHeader& header,
    const CopcHierarchyEntry& entry,
    std::vector<usdlas::LasPoint>& points,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    points.clear();
    return ReadPoints(
        header, entry,
        [&](const usdlas::LasPoint& point, std::uint64_t) {
            points.push_back(point);
            return true;
        },
        diagnostics);
}

bool CopcReader::ReadPoints(const CopcHeader& header,
                            const CopcHierarchyEntry& entry,
                            const PointConsumer& consume,
                            std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    if (!consume) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "COPC point consumer is invalid");
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }

    auto decoder = OpenLazChunkDecoder(source_, header, entry, diagnostics);
    if (!decoder) {
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }

    const auto maximumPoints = (std::min)(
        static_cast<std::uint64_t>(entry.pointCount),
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()));
    std::uint64_t pointsRead = 0;
    bool complete = false;
    while (!complete) {
        std::vector<usdlas::LasPoint> points;
        if (!decoder->ReadChunk(static_cast<std::size_t>(maximumPoints), points,
                                complete, diagnostics)) {
            failureKind_ = CopcReadFailure::Hierarchy;
            return false;
        }
        if (points.empty() && !complete) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "COPC decoder returned an empty incomplete chunk");
            failureKind_ = CopcReadFailure::Hierarchy;
            return false;
        }
        for (const auto& point : points) {
            if (!consume(point, pointsRead)) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                              "COPC point consumer rejected a point");
                diagnostics.back().pointIndex = pointsRead;
                failureKind_ = CopcReadFailure::Hierarchy;
                return false;
            }
            ++pointsRead;
        }
    }
    return true;
}

CopcReadFailure CopcReader::FailureKind() const noexcept {
    return failureKind_;
}

} // namespace usdcopc
