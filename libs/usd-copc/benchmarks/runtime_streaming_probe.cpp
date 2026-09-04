#include "usdgeo/RandomAccessSource.h"
#include "usdcopc/Copc.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ProbeOptions {
    std::string inputPath;
    std::int32_t level = 0;
};

void PrintUsage() {
    std::cerr << "Usage: usdCopc_runtime_streaming_probe <input.copc|input.copc.laz> "
                 "--level <non-negative level>\n";
}

bool ParseLevel(const std::string& value, std::int32_t& level) {
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stoll(value, &consumed);
        if (consumed != value.size() || parsed < 0 ||
            parsed > std::numeric_limits<std::int32_t>::max()) {
            return false;
        }
        level = static_cast<std::int32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseOptions(int argc, char** argv, ProbeOptions& options) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        PrintUsage();
        return false;
    }
    if (argc < 4) {
        PrintUsage();
        return false;
    }
    options.inputPath = argv[1];
    for (int index = 2; index < argc; ++index) {
        if (std::string(argv[index]) != "--level" || index + 1 >= argc ||
            !ParseLevel(argv[++index], options.level)) {
            PrintUsage();
            return false;
        }
    }
    return true;
}

void PrintDiagnostics(const std::vector<usdgeo::Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.message << "\n";
    }
}

bool TileOrder(const usdcopc::CopcPointTile& left,
               const usdcopc::CopcPointTile& right) {
    const auto& leftId = left.tile.id;
    const auto& rightId = right.tile.id;
    if (leftId.level != rightId.level) return leftId.level < rightId.level;
    if (leftId.x != rightId.x) return leftId.x < rightId.x;
    if (leftId.y != rightId.y) return leftId.y < rightId.y;
    return leftId.z < rightId.z;
}

bool MakeEntry(const usdcopc::CopcPointTile& tile,
               usdcopc::CopcHierarchyEntry& entry) {
    if (tile.tile.id.level < std::numeric_limits<std::int32_t>::min() ||
        tile.tile.id.level > std::numeric_limits<std::int32_t>::max() ||
        tile.tile.id.x < std::numeric_limits<std::int32_t>::min() ||
        tile.tile.id.x > std::numeric_limits<std::int32_t>::max() ||
        tile.tile.id.y < std::numeric_limits<std::int32_t>::min() ||
        tile.tile.id.y > std::numeric_limits<std::int32_t>::max() ||
        tile.tile.id.z < std::numeric_limits<std::int32_t>::min() ||
        tile.tile.id.z > std::numeric_limits<std::int32_t>::max() ||
        tile.tile.lod.items.empty() ||
        tile.tile.lod.items.front().pointCount >
            static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) ||
        tile.pointDataSize >
            static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    entry.level = static_cast<std::int32_t>(tile.tile.id.level);
    entry.x = static_cast<std::int32_t>(tile.tile.id.x);
    entry.y = static_cast<std::int32_t>(tile.tile.id.y);
    entry.z = static_cast<std::int32_t>(tile.tile.id.z);
    entry.pointCount = static_cast<std::int32_t>(
        tile.tile.lod.items.front().pointCount);
    entry.offset = tile.pointDataOffset;
    entry.byteSize = static_cast<std::int32_t>(tile.pointDataSize);
    return entry.IsValid() && entry.IsPointData();
}

std::string TileIdentity(const usdcopc::CopcPointTile& tile) {
    const auto& id = tile.tile.id;
    return "L" + std::to_string(id.level) + "/" + std::to_string(id.x) +
           "/" + std::to_string(id.y) + "/" + std::to_string(id.z);
}

std::string TileRange(const usdcopc::CopcPointTile& tile) {
    return std::to_string(tile.pointDataOffset) + ":" +
           std::to_string(tile.pointDataSize);
}

} // namespace

int main(int argc, char** argv) {
    ProbeOptions options;
    if (!ParseOptions(argc, argv, options)) {
        return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
    }

    auto localSource = usdgeo::OpenLocalRandomAccessSource(options.inputPath);
    if (!localSource) {
        std::cerr << "Unable to open COPC source\n";
        return 1;
    }
    auto source = std::shared_ptr<usdgeo::RandomAccessSource>(
        std::move(localSource));

    usdcopc::CopcReader reader(source);
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!reader.ReadMetadata(header, diagnostics)) {
        PrintDiagnostics(diagnostics);
        return 1;
    }

    std::vector<usdcopc::CopcHierarchyEntry> entries;
    if (!reader.ReadHierarchy(header, entries, diagnostics)) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    usdcopc::CopcHierarchy hierarchy;
    if (!reader.BuildHierarchy(header, entries, hierarchy, diagnostics)) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    std::vector<usdcopc::CopcPointTile> tiles;
    if (!reader.BuildPointTiles(hierarchy, tiles, diagnostics)) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    std::sort(tiles.begin(), tiles.end(), TileOrder);

    std::vector<const usdcopc::CopcPointTile*> selectedTiles;
    for (const auto& tile : tiles) {
        if (tile.tile.id.level == options.level) {
            selectedTiles.push_back(&tile);
        }
    }
    if (selectedTiles.empty()) {
        std::cerr << "Requested COPC level has no point-data tiles\n";
        return 2;
    }

    std::uint64_t selectedPointRangeBytes = 0;
    std::uint64_t decodedPoints = 0;
    std::vector<std::string> selectedTileIds;
    std::vector<std::string> selectedTileRanges;
    for (const auto* tile : selectedTiles) {
        usdcopc::CopcHierarchyEntry entry;
        if (!MakeEntry(*tile, entry)) {
            std::cerr << "COPC point tile cannot be represented as a hierarchy entry\n";
            return 1;
        }
        std::vector<usdlas::LasPoint> points;
        if (!reader.ReadPoints(header, entry, points, diagnostics)) {
            PrintDiagnostics(diagnostics);
            return 1;
        }
        selectedPointRangeBytes += tile->pointDataSize;
        decodedPoints += static_cast<std::uint64_t>(points.size());
        selectedTileIds.push_back(TileIdentity(*tile));
        selectedTileRanges.push_back(TileRange(*tile));
    }

    std::cout << "input\tlevel\thierarchy_nodes\tpoint_tiles\tselected_tiles\t"
                 "decoded_points\tselected_point_range_bytes\tsource_bytes_read\t"
                 "selected_tile_ids\tselected_tile_ranges\n"
              << options.inputPath << '\t' << options.level << '\t'
              << hierarchy.nodes.size() << '\t' << tiles.size() << '\t'
              << selectedTiles.size() << '\t' << decodedPoints << '\t'
              << selectedPointRangeBytes << '\t' << source->BytesRead() << '\t';
    for (std::size_t index = 0; index < selectedTileIds.size(); ++index) {
        if (index != 0) std::cout << ',';
        std::cout << selectedTileIds[index];
    }
    std::cout << '\t';
    for (std::size_t index = 0; index < selectedTileRanges.size(); ++index) {
        if (index != 0) std::cout << ',';
        std::cout << selectedTileRanges[index];
    }
    std::cout << '\n';
    return 0;
}