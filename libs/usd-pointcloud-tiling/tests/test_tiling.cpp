#include "usdpointcloud/Tiling.h"
#include "usdpointcloud/Spool.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <limits>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

class PlanningPointStream final : public usdpointcloud::PointStream {
public:
    explicit PlanningPointStream(std::vector<usdgeo::Vec3d> positions)
        : positions_(std::move(positions)) {}

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        diagnostic = {};
        if (read_) return usdpointcloud::PointStreamStatus::End;
        data.positions = positions_;
        usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
        for (const auto& position : data.positions) bounds.Expand(position);
        chunk = usdpointcloud::MakePointChunk(data, bounds);
        read_ = true;
        return usdpointcloud::PointStreamStatus::Chunk;
    }

private:
    std::vector<usdgeo::Vec3d> positions_;
    bool read_ = false;
};

void TestConfigValidation() {
    usdpointcloud::TileGridConfig config{10.0, 2};
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(config.IsValid());
    Check(usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(diagnostics.empty());

    config.tileSize = 0.0;
    Check(!usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(!diagnostics.empty());

    diagnostics.clear();
    config = {10.0, -1};
    Check(!usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(diagnostics.size() == 1);
}

void TestPointBudgetPlanning() {
    const usdpointcloud::PointBudgetPlan legacyPlan{3, 100, 2};
    Check(legacyPlan.tileCount == 3 && legacyPlan.maximumPointsPerTile == 100 &&
          legacyPlan.depth == 2);

    usdpointcloud::PointBudgetConfig config{100, 10, 3};
    std::vector<usdgeo::Vec3d> positions;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            positions.push_back({static_cast<double>(x), static_cast<double>(y), 0.0});
        }
    }
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdpointcloud::PointBudgetPlan plan;
    Check(usdpointcloud::ValidatePointBudgetConfig(config, diagnostics));
    Check(usdpointcloud::BuildPointBudgetPlan(positions, config, plan, diagnostics));
    Check(plan.depth == 2 && plan.tileCount == 16 &&
            plan.maximumPointsPerTile == 64 && plan.minimumPointsPerTile == 64 &&
            plan.pointCount == 1024 && plan.splitCount == 5 &&
            plan.averagePointsPerTile == 64.0);
    Check(diagnostics.empty());

    usdpointcloud::TilePlan tilePlan;
    Check(usdpointcloud::BuildTilePlan(plan, tilePlan, diagnostics));
    Check(tilePlan.IsValid() && tilePlan.plannerId == "adaptive-point-budget" &&
          tilePlan.plannerVersion == 1 && tilePlan.nodes.size() == 21);
    Check(tilePlan.nodes.front().id.level == 0 &&
          tilePlan.nodes.front().children.size() == 4);
    const usdpointcloud::PointBudgetTileRouter tilePlanRouter(tilePlan);
    Check(tilePlanRouter.GetTileId({31.0, 31.0, 0.0}).level == 2);

    diagnostics.clear();
    config.maxDepth = 1;
    Check(!usdpointcloud::BuildPointBudgetPlan(positions, config, plan, diagnostics));
    Check(!diagnostics.empty());

    diagnostics.clear();
    config = {100, 101, 3};
    Check(!usdpointcloud::ValidatePointBudgetConfig(config, diagnostics));
    Check(!diagnostics.empty());

    diagnostics.clear();
    config = {100, 70, 3};
    Check(!usdpointcloud::BuildPointBudgetPlan(positions, config, plan, diagnostics));
    Check(!diagnostics.empty());
}

void TestPointBudgetStreamPlanning() {
    usdpointcloud::PointBudgetConfig config{100, 10, 3};
    std::vector<usdgeo::Vec3d> positions;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            positions.push_back({static_cast<double>(x), static_cast<double>(y), 0.0});
        }
    }
    std::size_t factoryCalls = 0;
    const usdpointcloud::PointStreamFactory factory = [&]() {
        ++factoryCalls;
        return std::make_unique<PlanningPointStream>(positions);
    };
    usdpointcloud::PointBudgetPlan plan;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(usdpointcloud::BuildPointBudgetPlan(
        factory, config, plan, diagnostics));
    Check(factoryCalls == 2 && diagnostics.empty());
    Check(plan.pointCount == 1024 && plan.tileCount == 16 &&
          plan.minimumPointsPerTile == 64 &&
          plan.maximumPointsPerTile == 64 && plan.tiles.size() == 16);
    const usdpointcloud::PointBudgetTileRouter router(plan);
    Check(router.GetTileId({31.0, 31.0, 0.0}).level == 2);
}

void TestPointBudgetPlanningMatchesVectorForUnevenDistribution() {
    const std::vector<usdgeo::Vec3d> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {1.0, 1.0, 0.0}, {10.0, 0.0, 0.0}, {11.0, 0.0, 0.0},
        {10.0, 1.0, 0.0}, {11.0, 1.0, 0.0}, {0.0, 10.0, 0.0},
        {1.0, 10.0, 0.0}, {0.0, 11.0, 0.0}, {1.0, 11.0, 0.0},
        {10.0, 10.0, 0.0}, {11.0, 10.0, 0.0}, {10.0, 11.0, 0.0},
        {11.0, 11.0, 0.0}};
    const usdpointcloud::PointBudgetConfig config{4, 2, 3};
    usdpointcloud::PointBudgetPlan vectorPlan;
    std::vector<usdgeo::Diagnostic> vectorDiagnostics;
    Check(usdpointcloud::BuildPointBudgetPlan(
        positions, config, vectorPlan, vectorDiagnostics));

    const usdpointcloud::PointStreamFactory factory = [&]() {
        return std::make_unique<PlanningPointStream>(positions);
    };
    usdpointcloud::PointBudgetPlan streamPlan;
    std::vector<usdgeo::Diagnostic> streamDiagnostics;
    Check(usdpointcloud::BuildPointBudgetPlan(
        factory, config, streamPlan, streamDiagnostics));
    Check(vectorPlan.tiles.size() == streamPlan.tiles.size());
    for (std::size_t index = 0; index < vectorPlan.tiles.size(); ++index) {
        Check(vectorPlan.tiles[index].id.level == streamPlan.tiles[index].id.level &&
              vectorPlan.tiles[index].id.x == streamPlan.tiles[index].id.x &&
              vectorPlan.tiles[index].id.y == streamPlan.tiles[index].id.y &&
              vectorPlan.tiles[index].pointCount ==
                  streamPlan.tiles[index].pointCount);
    }
}

void TestPointBudgetDepthLimit() {
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdpointcloud::ValidatePointBudgetConfig(
        {1, 1, usdpointcloud::kMaxPointBudgetDepth + 1}, diagnostics));
    Check(!diagnostics.empty());
}

void TestTilePlanValidation() {
    const usdgeo::SpatialBounds bounds{{0.0, 0.0, 0.0},
                                       {1.0, 1.0, 0.0}};
    usdpointcloud::TilePlan plan;
    plan.plannerId = "copc-native-hierarchy";
    plan.plannerVersion = 1;
    plan.nodes = {{{0, 0, 0, 0}, bounds, 2, {-1, 0, 0, 0},
                   {{1, 0, 0, 0}}, {{0, 10}}, false},
                  {{1, 0, 0, 0}, bounds, 2, {0, 0, 0, 0}, {},
                   {{0, 10}}, true}};
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(usdpointcloud::ValidateTilePlan(plan, diagnostics));
    Check(diagnostics.empty());

    plan.nodes[1].parent = {-1, 0, 0, 0};
    Check(!usdpointcloud::ValidateTilePlan(plan, diagnostics));
    Check(!diagnostics.empty());

    plan.nodes[1].parent = {0, 0, 0, 0};
    plan.nodes[0].parent = {1, 0, 0, 0};
    plan.nodes[1].children.push_back(plan.nodes[0].id);
    diagnostics.clear();
    Check(!usdpointcloud::ValidateTilePlan(plan, diagnostics));
    Check(!diagnostics.empty());
}

void TestTilePlanCacheIdentity() {
    const usdgeo::SpatialBounds bounds{{0.0, 0.0, 0.0},
                                       {1.0, 1.0, 0.0}};
    usdpointcloud::TilePlan first;
    first.plannerId = "copc-native-hierarchy";
    first.plannerVersion = 1;
    first.nodes = {
        {{0, 0, 0, 0}, bounds, 2, {-1, 0, 0, 0}, {{1, 0, 0, 0}}, {}, false},
        {{1, 0, 0, 0}, bounds, 2, {0, 0, 0, 0}, {}, {{0, 10}}, true}};
    Check(usdpointcloud::StableTilePlanKey(first).size() == 16);

    auto reordered = first;
    std::swap(reordered.nodes[0], reordered.nodes[1]);
    Check(usdpointcloud::StableTilePlanKey(first) ==
          usdpointcloud::StableTilePlanKey(reordered));

    reordered.nodes[1].pointCount++;
    Check(usdpointcloud::StableTilePlanKey(first) !=
          usdpointcloud::StableTilePlanKey(reordered));

    reordered = first;
    reordered.plannerVersion++;
    Check(usdpointcloud::StableTilePlanKey(first) !=
          usdpointcloud::StableTilePlanKey(reordered));

    usdpointcloud::TilePlan invalid = first;
    invalid.plannerId.clear();
    Check(usdpointcloud::StableTilePlanKey(invalid).empty());
}

void TestTileManifestSerialization() {
    const usdgeo::SpatialBounds bounds{{-2.0, 1.0, 3.0},
                                       {4.0, 5.0, 6.0}};
    usdpointcloud::PointTileManifest manifest;
    manifest.entries = {
        {{2, -3, 4, 0}, 1, bounds, 12, "payload/L2_-3_4_0_LOD1.usdc"},
        {{0, 0, 0, 0}, 0, bounds, 8, "payload/L0_0_0_0_LOD0.usdc"}};
    std::vector<usdgeo::Diagnostic> diagnostics;
    std::string first;
    Check(usdpointcloud::SerializePointTileManifest(
        manifest, first, diagnostics));
    Check(diagnostics.empty());
    Check(first.find("format=usd-pointcloud-tile-manifest-v1\n") == 0);
    Check(first.find("tile.0.id=L0/0/0/0\n") != std::string::npos);
    Check(first.find("tile.1.id=L2/-3/4/0\n") != std::string::npos);

    std::swap(manifest.entries[0], manifest.entries[1]);
    std::string second;
    Check(usdpointcloud::SerializePointTileManifest(
        manifest, second, diagnostics));
    Check(first == second);
}

void TestTileManifestValidation() {
    const usdgeo::SpatialBounds bounds{{0.0, 0.0, 0.0},
                                       {1.0, 1.0, 1.0}};
    usdpointcloud::PointTileManifest manifest;
    manifest.entries = {
        {{0, 0, 0, 0}, 0, bounds, 1, "payload/a.usdc"},
        {{0, 0, 0, 0}, 0, bounds, 1, "payload/b.usdc"}};
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdpointcloud::ValidatePointTileManifest(manifest, diagnostics));
    Check(!diagnostics.empty());

    diagnostics.clear();
    manifest.entries[1].id.level = -1;
    Check(!usdpointcloud::ValidatePointTileManifest(manifest, diagnostics));

    diagnostics.clear();
    manifest.entries = {
        {{0, 0, 0, 0}, 0, bounds, 1, "../outside.usdc"}};
    Check(!usdpointcloud::ValidatePointTileManifest(manifest, diagnostics));
}

void TestFixedGridRouting() {
    const usdpointcloud::FixedGridTileRouter router({10.0, 0});
    Check(router.IsValid());
    Check(router.GetTileId({0.0, 0.0, 100.0}).x == 0);
    Check(router.GetTileId({9.999, 19.999, 0.0}).y == 1);
    Check(router.GetTileId({10.0, -10.0, 0.0}).x == 1);
    Check(router.GetTileId({-0.001, -10.0, 0.0}).x == -1);
    Check(router.GetTileId({-10.0, -20.0, 0.0}).y == -2);
}

void TestInvalidRouting() {
    const usdpointcloud::FixedGridTileRouter invalid({0.0, 0});
    Check(!invalid.IsValid());
    Check(!invalid.GetTileId({0.0, 0.0, 0.0}).IsValid());

    const usdpointcloud::FixedGridTileRouter valid({1.0, 0});
    Check(!valid.GetTileId({std::numeric_limits<double>::infinity(), 0.0, 0.0})
               .IsValid());
    const double int64UpperExclusive = std::ldexp(1.0, 63);
    Check(!valid.GetTileId({int64UpperExclusive, 0.0, 0.0}).IsValid());
    Check(valid.GetTileId({-int64UpperExclusive, 0.0, 0.0}).IsValid());
}

void TestSpoolRoundTrip() {
    const auto path = std::filesystem::temp_directory_path() / "usdgeo-tile-spool.bin";
    const usdpointcloud::PointTileId tile{2, -3, 4, 0};
    usdpointcloud::SpoolSchema schema;
    schema.attributes.push_back({"count", usdpointcloud::PointAttributeType::UInt64});
    schema.attributes.push_back({"density", usdpointcloud::PointAttributeType::Float32});
    schema.attributes.push_back({"normal", usdpointcloud::PointAttributeType::Float64Vec2});
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdpointcloud::SpoolIoStats ioStats;
    usdpointcloud::TileSpoolWriter writer;
    Check(writer.Open(path, tile, schema, 1, diagnostics, &ioStats));
    Check(writer.Append({{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0},
                         {std::uint64_t{0xFFFFFFFFFFFFFFFF}, 7.5F,
                          std::array<double, 2>{1.0, 2.0}}}, diagnostics));
    Check(writer.BufferedBytes() == 0);
    Check(writer.Append({{-1.0, -2.0, 0.0}, {-4.0, -5.0, 0.0},
                         {std::uint64_t{42}, -2.5F,
                          std::array<double, 2>{3.0, 4.0}}}, diagnostics));
    double markerCoordinate = 0.0;
    const char footerBytes[] = "USDGEND1";
    std::memcpy(&markerCoordinate, footerBytes, sizeof(markerCoordinate));
    Check(writer.Append({{markerCoordinate, 8.0, 9.0}, {10.0, 11.0, 12.0},
                         {std::uint64_t{7}, 1.0F,
                          std::array<double, 2>{5.0, 6.0}}}, diagnostics));
    Check(writer.Close(diagnostics));

    {
        usdpointcloud::TileSpoolReader reader;
        usdpointcloud::PointTileId readTile;
        usdpointcloud::SpoolSchema readSchema;
        Check(reader.Open(path, readTile, readSchema, diagnostics, &ioStats));
        Check(readTile.x == tile.x && readTile.y == tile.y && readSchema.IsValid());
        usdpointcloud::SpoolPoint point;
          Check(reader.ReadNext(point, diagnostics) &&
              std::get<std::uint64_t>(point.attributes[0]) ==
                std::uint64_t{0xFFFFFFFFFFFFFFFF} &&
                            std::get<float>(point.attributes[1]) == 7.5F &&
                            std::get<std::array<double, 2>>(point.attributes[2]) ==
                                    std::array<double, 2>{1.0, 2.0});
          Check(reader.ReadNext(point, diagnostics) &&
              std::get<std::uint64_t>(point.attributes[0]) == 42 &&
              std::get<float>(point.attributes[1]) == -2.5F);
          Check(reader.ReadNext(point, diagnostics) &&
              std::get<std::uint64_t>(point.attributes[0]) == 7);
        Check(!reader.ReadNext(point, diagnostics) && reader.IsComplete());
        Check(diagnostics.empty());
    }
    Check(ioStats.bytesWritten > 0);
    Check(ioStats.bytesWritten == ioStats.bytesRead);
    std::filesystem::remove(path);
}

void TestIncompleteSpool() {
    const auto path = std::filesystem::temp_directory_path() / "usdgeo-incomplete-spool.bin";
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write("USDGSP01", 8);
    }
    {
        usdpointcloud::TileSpoolReader reader;
        usdpointcloud::PointTileId tile;
        usdpointcloud::SpoolSchema schema;
        std::vector<usdgeo::Diagnostic> diagnostics;
        Check(!reader.Open(path, tile, schema, diagnostics));
        Check(!diagnostics.empty());
        Check(diagnostics.front().code ==
              usdgeo::DiagnosticCode::SpoolIncomplete);
        Check(usdgeo::StoppedByWorkingStorageFailure(diagnostics));
    }
    std::filesystem::remove(path);
}

usdpointcloud::SpoolSchema TestSpoolSchema() {
    usdpointcloud::SpoolSchema schema;
    schema.attributes.push_back(
        {"intensity", usdpointcloud::PointAttributeType::UInt16});
    return schema;
}

usdpointcloud::SpoolPoint TestSpoolPoint(double x) {
    return {{x, 0.0, 0.0}, {x, 0.0, 0.0}, {std::uint16_t{7}}};
}

void TestMissingSpoolIsIncomplete() {
    const auto path =
        std::filesystem::temp_directory_path() / "usdgeo-missing-spool.bin";
    std::filesystem::remove(path);
    usdpointcloud::TileSpoolReader reader;
    usdpointcloud::PointTileId tile;
    usdpointcloud::SpoolSchema schema;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.Open(path, tile, schema, diagnostics));
    Check(diagnostics.size() == 1);
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::SpoolIncomplete);
    usdpointcloud::SpoolPoint point;
    Check(!reader.ReadNext(point, diagnostics));
    Check(!reader.IsComplete());
}

void TestUnfinalizedSpoolIsIncomplete() {
    // Points are flushed but the writer never closes, as when a generator is
    // killed: the reader must refuse the spool rather than report its points
    // as the whole tile.
    const auto path = std::filesystem::temp_directory_path() /
                      "usdgeo-unfinalized-spool.bin";
    const usdpointcloud::PointTileId tile{0, 1, 2, 0};
    std::vector<usdgeo::Diagnostic> diagnostics;
    {
        usdpointcloud::TileSpoolWriter writer;
        Check(writer.Open(path, tile, TestSpoolSchema(), 1, diagnostics));
        Check(writer.Append(TestSpoolPoint(1.0), diagnostics));
        Check(writer.Append(TestSpoolPoint(2.0), diagnostics));
        Check(writer.Flush(diagnostics));
        Check(diagnostics.empty());
    }
    usdpointcloud::TileSpoolReader reader;
    usdpointcloud::PointTileId readTile;
    usdpointcloud::SpoolSchema readSchema;
    if (reader.Open(path, readTile, readSchema, diagnostics)) {
        usdpointcloud::SpoolPoint point;
        while (reader.ReadNext(point, diagnostics)) {
        }
    }
    Check(!reader.IsComplete());
    Check(!diagnostics.empty());
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::SpoolIncomplete);
    reader.Close();
    std::filesystem::remove(path);
}

void TestUnwritableSpoolReportsWriteFailure() {
    const auto directory = std::filesystem::temp_directory_path() /
                           "usdgeo-missing-spool-directory";
    std::filesystem::remove_all(directory);
    usdpointcloud::TileSpoolWriter writer;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!writer.Open(directory / "tile_0.bin", {0, 0, 0, 0},
                       TestSpoolSchema(), 1, diagnostics));
    Check(diagnostics.size() == 1);
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::SpoolWriteFailed);
    Check(usdgeo::StoppedByWorkingStorageFailure(diagnostics));
    Check(!std::filesystem::exists(directory));
}

void TestExhaustedOpenFileBudgetIsReported() {
#if defined(__unix__) || defined(__APPLE__)
    // Lower the soft descriptor limit below every descriptor already open, so
    // the next open fails exactly as it would in a generator holding one
    // spool per tile, then restore it.
    const auto directory = std::filesystem::temp_directory_path();
    const auto writePath = directory / "usdgeo-budget-spool-write.bin";
    const auto readPath = directory / "usdgeo-budget-spool-read.bin";
    std::vector<usdgeo::Diagnostic> diagnostics;
    {
        usdpointcloud::TileSpoolWriter writer;
        Check(writer.Open(readPath, {0, 0, 0, 0}, TestSpoolSchema(), 1,
                          diagnostics));
        Check(writer.Append(TestSpoolPoint(1.0), diagnostics));
        Check(writer.Close(diagnostics));
    }
    rlimit original{};
    Check(getrlimit(RLIMIT_NOFILE, &original) == 0);
    rlimit exhausted = original;
    exhausted.rlim_cur = 0;
    Check(setrlimit(RLIMIT_NOFILE, &exhausted) == 0);
    usdpointcloud::TileSpoolWriter writer;
    std::vector<usdgeo::Diagnostic> writeDiagnostics;
    const auto writeOpened = writer.Open(writePath, {0, 0, 0, 0},
                                         TestSpoolSchema(), 1,
                                         writeDiagnostics);
    usdpointcloud::TileSpoolReader reader;
    usdpointcloud::PointTileId tile;
    usdpointcloud::SpoolSchema schema;
    std::vector<usdgeo::Diagnostic> readDiagnostics;
    const auto readOpened =
        reader.Open(readPath, tile, schema, readDiagnostics);
    Check(setrlimit(RLIMIT_NOFILE, &original) == 0);

    Check(!writeOpened);
    Check(writeDiagnostics.size() == 1);
    Check(writeDiagnostics.front().code ==
          usdgeo::DiagnosticCode::OpenFileBudgetExhausted);
    Check(!readOpened);
    Check(readDiagnostics.size() == 1);
    Check(readDiagnostics.front().code ==
          usdgeo::DiagnosticCode::OpenFileBudgetExhausted);
    Check(usdgeo::StoppedByWorkingStorageFailure(readDiagnostics));
    std::filesystem::remove(writePath);
    std::filesystem::remove(readPath);
#endif
}

} // namespace

int main() {
    TestConfigValidation();
    TestPointBudgetPlanning();
    TestFixedGridRouting();
    TestInvalidRouting();
    TestSpoolRoundTrip();
    TestIncompleteSpool();
    TestMissingSpoolIsIncomplete();
    TestUnfinalizedSpoolIsIncomplete();
    TestUnwritableSpoolReportsWriteFailure();
    TestExhaustedOpenFileBudgetIsReported();
    TestPointBudgetStreamPlanning();
    TestPointBudgetPlanningMatchesVectorForUnevenDistribution();
    TestPointBudgetDepthLimit();
    TestTilePlanValidation();
    TestTilePlanCacheIdentity();
    TestTileManifestSerialization();
    TestTileManifestValidation();
    return 0;
}
