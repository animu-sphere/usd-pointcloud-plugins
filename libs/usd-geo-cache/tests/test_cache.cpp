#include "usdgeo/cache/Cache.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void Check(bool condition, const char* message = "check failed") {
    if (!condition) {
        std::cerr << message << "\n";
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

void TestLocalSourceIdentity() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("usdgeo-source-identity-" +
                       std::to_string(std::chrono::steady_clock::now()
                                          .time_since_epoch()
                                          .count()));
    const auto source = root / "source.las";
    std::filesystem::create_directories(root);
    std::ofstream(source) << "source identity";

    usdgeo::cache::SourceIdentity identity;
    std::string errorMessage;
    Check(usdgeo::cache::TryBuildLocalSourceIdentity(
        source, identity, errorMessage));
    Check(errorMessage.empty());
    Check(identity.IsValid());
    Check(identity.identifier ==
          std::filesystem::weakly_canonical(source).generic_string());
    Check(identity.validationToken.rfind("fnv1a64:", 0) == 0);

    std::filesystem::remove_all(root);
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

    const usdgeo::cache::SourceIdentity resolverIdentity{
        "resolver://survey/sample.las", 0, 0, "etag:abc"};
    Check(resolverIdentity.IsValid());
    const usdgeo::cache::SourceIdentity unstableIdentity{
        "resolver://survey/sample.las", 0, 0, ""};
    Check(!unstableIdentity.IsValid());

    usdgeo::cache::SourceIdentity legacyIdentity;
    legacyIdentity.canonicalPath = "C:/data/sample.las";
    legacyIdentity.sizeBytes = 1024;
    legacyIdentity.modifiedTime = 12345;
    legacyIdentity.contentIdentity = "sha256:abc";
    Check(legacyIdentity.IsValid());
    usdgeo::cache::Descriptor legacyDescriptor = first;
    legacyDescriptor.source = legacyIdentity;
    Check(usdgeo::cache::StableCacheKey(legacyDescriptor) ==
          usdgeo::cache::StableCacheKey(first));
}

void TestLayoutAndInvalidation() {
    const auto uniqueSuffix =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("usdgeo-cache-test-" + std::to_string(uniqueSuffix));
    usdgeo::cache::Layout layout;
    Check(usdgeo::cache::Inspect(layout).status ==
              usdgeo::cache::LookupStatus::InvalidLayout,
          "invalid lookup status");
    Check(usdgeo::cache::TryBuildLayout(root, MakeDescriptor(), layout));
    Check(layout.IsValid());

    const usdgeo::TileId tile{2, -3, 4, 0};
    Check(usdgeo::cache::TilePayloadPath(layout, tile, 1).filename() ==
          "Tile_L2_n3_p4_p0_LOD1.usdc");
    Check(usdgeo::cache::Inspect(layout).status ==
              usdgeo::cache::LookupStatus::Missing,
          "missing lookup status");
    Check(!usdgeo::cache::IsCacheHit(layout));

    std::filesystem::create_directories(layout.payloadDirectory);
    std::ofstream(layout.rootLayer) << "cache";
    Check(usdgeo::cache::Inspect(layout).status ==
              usdgeo::cache::LookupStatus::Incomplete,
          "incomplete lookup status");
    Check(!usdgeo::cache::IsCacheHit(layout));
    std::filesystem::create_directory(layout.manifest);
    Check(usdgeo::cache::Inspect(layout).status ==
              usdgeo::cache::LookupStatus::Incomplete,
          "non-regular marker status");
    std::filesystem::remove_all(layout.manifest);
    std::ofstream(layout.manifest) << "committed";
    Check(usdgeo::cache::Inspect(layout).status ==
              usdgeo::cache::LookupStatus::Hit,
          "hit lookup status");
    Check(usdgeo::cache::IsCacheHit(layout));
    const auto unrelatedDirectory = root / "unrelated";
    std::filesystem::create_directories(unrelatedDirectory);
    std::ofstream(unrelatedDirectory / "source.las") << "source";
    Check(usdgeo::cache::Invalidate(root, MakeDescriptor()));
    Check(!std::filesystem::exists(layout.entryDirectory));
    Check(std::filesystem::exists(unrelatedDirectory / "source.las"));
}

} // namespace

int main() {
    TestLocalSourceIdentity();
    TestStableDescriptorKey();
    TestLayoutAndInvalidation();
    return 0;
}