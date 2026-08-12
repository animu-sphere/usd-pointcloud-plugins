#include "usdpointcloud/Tiling.h"
#include "usdpointcloud/Spool.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <limits>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

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
          plan.maximumPointsPerTile == 64);
    Check(diagnostics.empty());

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
    usdpointcloud::TileSpoolWriter writer;
    Check(writer.Open(path, tile, schema, 1, diagnostics));
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
        Check(reader.Open(path, readTile, readSchema, diagnostics));
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
    }
    std::filesystem::remove(path);
}

} // namespace

int main() {
    TestConfigValidation();
    TestPointBudgetPlanning();
    TestFixedGridRouting();
    TestInvalidRouting();
    TestSpoolRoundTrip();
    TestIncompleteSpool();
    return 0;
}
