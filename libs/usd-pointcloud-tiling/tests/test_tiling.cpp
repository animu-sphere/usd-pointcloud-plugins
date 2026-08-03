#include "usdpointcloud/Tiling.h"
#include "usdpointcloud/Spool.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    schema.attributes.push_back({"density", usdpointcloud::PointAttributeType::Float64});
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdpointcloud::TileSpoolWriter writer;
    Check(writer.Open(path, tile, schema, 1, diagnostics));
    Check(writer.Append({{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.5}}, diagnostics));
    Check(writer.BufferedBytes() == 0);
    Check(writer.Append({{-1.0, -2.0, 0.0}, {-4.0, -5.0, 0.0}, {-2.5}}, diagnostics));
    Check(writer.Close(diagnostics));

    {
        usdpointcloud::TileSpoolReader reader;
        usdpointcloud::PointTileId readTile;
        usdpointcloud::SpoolSchema readSchema;
        Check(reader.Open(path, readTile, readSchema, diagnostics));
        Check(readTile.x == tile.x && readTile.y == tile.y && readSchema.IsValid());
        usdpointcloud::SpoolPoint point;
        Check(reader.ReadNext(point, diagnostics) && point.attributes.front() == 7.5);
        Check(reader.ReadNext(point, diagnostics) && point.attributes.front() == -2.5);
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
    TestFixedGridRouting();
    TestInvalidRouting();
    TestSpoolRoundTrip();
    TestIncompleteSpool();
    return 0;
}
