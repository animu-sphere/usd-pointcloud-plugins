#include "usdgeo/cache/Cache.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

        equivalent = first;
        equivalent.source.validationToken = "sha256:changed";
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

void TestResolverSourceIdentity() {
    const usdgeo::cache::ResolverAssetIdentity stableAsset{
        "resolver://survey/sample.copc", 4096, "opaque-revision-a"};
    Check(usdgeo::cache::ClassifyResolverIdentity(stableAsset) ==
          usdgeo::cache::ResolverIdentityStability::Stable);
    Check(std::string(usdgeo::cache::ResolverIdentityStabilityName(
              usdgeo::cache::ResolverIdentityStability::Stable)) == "stable");

    usdgeo::cache::SourceIdentity stableIdentity;
    usdgeo::cache::ResolverIdentityStability stability;
    std::string errorMessage;
    Check(usdgeo::cache::TryBuildResolverSourceIdentity(
        stableAsset, stableIdentity, stability, errorMessage));
    Check(stability == usdgeo::cache::ResolverIdentityStability::Stable);
    Check(errorMessage.empty());
    Check(stableIdentity.identifier == stableAsset.resolvedIdentifier);
    Check(stableIdentity.sizeBytes == stableAsset.sizeBytes);
    Check(stableIdentity.validationToken == stableAsset.validationToken);
    Check(stableIdentity.canonicalPath.empty());
    Check(stableIdentity.contentIdentity.empty());

    const usdgeo::cache::ResolverAssetIdentity unstableAsset{
        stableAsset.resolvedIdentifier, stableAsset.sizeBytes, {}};
    Check(usdgeo::cache::ClassifyResolverIdentity(unstableAsset) ==
          usdgeo::cache::ResolverIdentityStability::Unstable);
    Check(std::string(usdgeo::cache::ResolverIdentityStabilityName(
              usdgeo::cache::ResolverIdentityStability::Unstable)) ==
          "unstable");
    Check(!usdgeo::cache::TryBuildResolverSourceIdentity(
        unstableAsset, stableIdentity, stability, errorMessage));
    Check(stability == usdgeo::cache::ResolverIdentityStability::Unstable);
    Check(errorMessage == "resolver source identity is unstable");
    Check(!stableIdentity.IsValid());

    const usdgeo::cache::ResolverAssetIdentity unavailableAsset{};
    Check(usdgeo::cache::ClassifyResolverIdentity(unavailableAsset) ==
          usdgeo::cache::ResolverIdentityStability::Unavailable);
    Check(std::string(usdgeo::cache::ResolverIdentityStabilityName(
              usdgeo::cache::ResolverIdentityStability::Unavailable)) ==
          "unavailable");
    Check(!usdgeo::cache::TryBuildResolverSourceIdentity(
        unavailableAsset, stableIdentity, stability, errorMessage));
    Check(stability == usdgeo::cache::ResolverIdentityStability::Unavailable);
    Check(errorMessage == "resolver source identity is unavailable");
    Check(!stableIdentity.IsValid());
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

    void TestLookupStatistics() {
        Check(std::string(usdgeo::cache::LookupStatusName(
              usdgeo::cache::LookupStatus::InvalidLayout)) ==
            "invalid-layout");
        Check(std::string(usdgeo::cache::LookupStatusName(
              usdgeo::cache::LookupStatus::Missing)) == "missing");
        Check(std::string(usdgeo::cache::LookupStatusName(
              usdgeo::cache::LookupStatus::Incomplete)) == "incomplete");
        Check(std::string(usdgeo::cache::LookupStatusName(
              usdgeo::cache::LookupStatus::Hit)) == "hit");

        usdgeo::cache::ResetLookupStatistics();
        usdgeo::cache::Layout invalidLayout;
        Check(usdgeo::cache::Inspect(invalidLayout).status ==
            usdgeo::cache::LookupStatus::InvalidLayout);

        const auto root = std::filesystem::temp_directory_path() /
                    ("usdgeo-cache-statistics-" +
                     std::to_string(std::chrono::steady_clock::now()
                                .time_since_epoch()
                                .count()));
        usdgeo::cache::Layout layout;
        Check(usdgeo::cache::TryBuildLayout(root, MakeDescriptor(), layout));
        Check(usdgeo::cache::Inspect(layout).status ==
            usdgeo::cache::LookupStatus::Missing);
        std::filesystem::create_directories(layout.entryDirectory);
        std::ofstream(layout.rootLayer) << "cache";
        Check(usdgeo::cache::Inspect(layout).status ==
            usdgeo::cache::LookupStatus::Incomplete);
        std::ofstream(layout.manifest) << "committed";
        Check(usdgeo::cache::Inspect(layout).status ==
            usdgeo::cache::LookupStatus::Hit);

        const auto statistics = usdgeo::cache::GetLookupStatistics();
        Check(statistics.lookups == 4);
        Check(statistics.invalidLayouts == 1);
        Check(statistics.misses == 1);
        Check(statistics.incomplete == 1);
        Check(statistics.hits == 1);
        Check(statistics.HitRatio() == 0.25);

        usdgeo::cache::ResetLookupStatistics();
        const auto resetStatistics = usdgeo::cache::GetLookupStatistics();
        Check(resetStatistics.lookups == 0 && resetStatistics.HitRatio() == 0.0);
        std::filesystem::remove_all(root);
    }

void TestConcurrentLookupStatistics() {
    usdgeo::cache::ResetLookupStatistics();
    const auto root = std::filesystem::temp_directory_path() /
                      ("usdgeo-cache-concurrent-statistics-" +
                       std::to_string(std::chrono::steady_clock::now()
                                          .time_since_epoch()
                                          .count()));
    usdgeo::cache::Layout layout;
    Check(usdgeo::cache::TryBuildLayout(root, MakeDescriptor(), layout));
    std::filesystem::create_directories(layout.entryDirectory);
    std::ofstream(layout.rootLayer) << "cache";
    std::ofstream(layout.manifest) << "committed";

    constexpr int threadCount = 8;
    constexpr int lookupsPerThread = 64;
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    for (int threadIndex = 0; threadIndex != threadCount; ++threadIndex) {
        workers.emplace_back([&layout, lookupsPerThread] {
            for (int lookupIndex = 0; lookupIndex != lookupsPerThread;
                 ++lookupIndex) {
                usdgeo::cache::Inspect(layout);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    const auto statistics = usdgeo::cache::GetLookupStatistics();
    const auto expectedLookups =
        static_cast<std::uint64_t>(threadCount * lookupsPerThread);
    Check(statistics.lookups == expectedLookups);
    Check(statistics.hits == expectedLookups);
    Check(statistics.misses == 0 && statistics.incomplete == 0 &&
          statistics.invalidLayouts == 0);
    Check(statistics.HitRatio() == 1.0);
    std::filesystem::remove_all(root);
    usdgeo::cache::ResetLookupStatistics();
}

} // namespace

int main() {
    TestLocalSourceIdentity();
    TestStableDescriptorKey();
    TestResolverSourceIdentity();
    TestLayoutAndInvalidation();
    TestLookupStatistics();
    TestConcurrentLookupStatistics();
    return 0;
}