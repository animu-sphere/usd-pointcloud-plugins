#include "usdgeo/cache/Cache.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::cerr << "check failed\n";
        std::exit(1);
    }
}

usdgeo::cache::Descriptor MakeDescriptor() {
    usdgeo::cache::Descriptor descriptor;
    descriptor.source = {"C:/data/sample.las", 1024, 12345, "sha256:abc"};
    descriptor.pluginVersion = "0.2.1";
    descriptor.parserVersion = "las-1";
    descriptor.openUsdVersion = "26.08";
    descriptor.coordinateTransform = {{"origin", "100,200,0"}};
    descriptor.attributes = {{"selection", "xyz,rgb"}};
    descriptor.tileAndLod = {{"tileSize", "64"}, {"lod", "balanced"}};
    descriptor.downsampling = {{"algorithm", "fixed-stride"},
                               {"version", "1"}};
    return descriptor;
}

void TestStableDescriptorKey() {
    const auto first = MakeDescriptor();
    auto equivalent = first;
    equivalent.attributes = {{"selection", " xyz,rgb "}};
    Check(first.IsValid());
    Check(usdgeo::cache::StableCacheKey(first) ==
          usdgeo::cache::StableCacheKey(equivalent));

    equivalent.source.modifiedTime++;
    Check(usdgeo::cache::StableCacheKey(first) !=
          usdgeo::cache::StableCacheKey(equivalent));
}

void TestLayoutAndInvalidation() {
    const auto uniqueSuffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("usdgeo-cache-test-" + std::to_string(uniqueSuffix));
    usdgeo::cache::Layout layout;
    Check(usdgeo::cache::TryBuildLayout(root, MakeDescriptor(), layout));
    Check(layout.IsValid());

    const usdgeo::TileId tile{2, -3, 4, 0};
    Check(usdgeo::cache::TilePayloadPath(layout, tile, 1).filename() ==
          "Tile_L2_n3_p4_p0_LOD1.usdc");
    Check(!usdgeo::cache::IsCacheHit(layout));

    std::filesystem::create_directories(layout.payloadDirectory);
    std::ofstream(layout.rootLayer) << "cache";
    Check(usdgeo::cache::IsCacheHit(layout));
    Check(usdgeo::cache::Invalidate(layout));
    Check(!std::filesystem::exists(layout.entryDirectory));
}

} // namespace

int main() {
    TestStableDescriptorKey();
    TestLayoutAndInvalidation();
    return 0;
}