#include "usdgeo/PointCloudCache.h"

#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/sdf/primSpec.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace usdgeo {
namespace {

std::string FormatDouble(double value) {
    std::ostringstream result;
    result << std::setprecision(17) << value;
    return result.str();
}

bool HashFile(const std::filesystem::path& path,
              std::string& identity,
              std::string& errorMessage) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        errorMessage = "unable to open input for cache identity";
        return false;
    }

    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(
                buffer[static_cast<std::size_t>(index)]);
            hash *= prime;
        }
    }
    if (!input.eof()) {
        errorMessage = "unable to read input for cache identity";
        return false;
    }

    std::ostringstream formatted;
    formatted << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
              << hash;
    identity = formatted.str();
    return true;
}

bool BuildDescriptor(const std::filesystem::path& sourcePath,
                     const GeoReference& reference,
                     const usdpointcloud::PointReadRequest& request,
                     const std::string& parserVersion,
                     usdgeo::cache::Descriptor& descriptor,
                     std::string& errorMessage) {
    std::error_code error;
    const auto canonicalPath =
        std::filesystem::weakly_canonical(sourcePath, error);
    if (error) {
        errorMessage = "unable to canonicalize input for cache identity: " +
                       error.message();
        return false;
    }
    const auto size = std::filesystem::file_size(sourcePath, error);
    if (error) {
        errorMessage = "unable to inspect input for cache identity: " +
                       error.message();
        return false;
    }
    const auto modified = std::filesystem::last_write_time(sourcePath, error);
    if (error) {
        errorMessage = "unable to inspect input timestamp for cache identity: " +
                       error.message();
        return false;
    }
    std::string contentIdentity;
    if (!HashFile(sourcePath, contentIdentity, errorMessage)) {
        return false;
    }

    descriptor.source = {canonicalPath.generic_string(), size,
                         modified.time_since_epoch().count(), contentIdentity};
    descriptor.pluginVersion = "usd-pointcloud-plugins-0.2.2";
    descriptor.parserVersion = parserVersion;
    descriptor.openUsdVersion = "26.08";
    descriptor.coordinateTransform = {
        {"epsg", reference.epsgCode ? std::to_string(*reference.epsgCode) : "0"},
        {"linearUnit", reference.linearUnit},
        {"sourceUpAxis", reference.sourceUpAxis},
        {"stageUpAxis", reference.stageUpAxis},
        {"origin.x", FormatDouble(reference.localOrigin.x)},
        {"origin.y", FormatDouble(reference.localOrigin.y)},
        {"origin.z", FormatDouble(reference.localOrigin.z)}};
    for (const auto& [key, value] : request.canonicalArguments) {
        if (key == "attributes") {
            descriptor.attributes.emplace_back(key, value);
        } else if (key != "payloadDirectory" && key != "chunkPointLimit" &&
                   key != "memoryBudgetBytes") {
            descriptor.tileAndLod.emplace_back(key, value);
        }
    }
    descriptor.downsampling = {{"algorithm", "fixed-stride"},
                               {"version", "1"}};
    if (!descriptor.IsValid()) {
        errorMessage = "cache descriptor is invalid";
        return false;
    }
    return true;
}

void RebasePayloads(pxr::SdfLayer* layer,
                    const std::filesystem::path& entryDirectory) {
    layer->Traverse(pxr::SdfPath::AbsoluteRootPath(),
                    [&](const pxr::SdfPath& path) {
        const auto prim = layer->GetPrimAtPath(path);
        if (!prim || !prim->HasPayloads()) {
            return;
        }
        const auto payloads = prim->GetPayloadList();
        const auto rebase = [&](auto items) {
            for (const auto& item : items) {
                const pxr::SdfPayload payload = item;
                if (payload.GetAssetPath().empty() ||
                    std::filesystem::path(payload.GetAssetPath()).is_absolute()) {
                    continue;
                }
                const auto absolutePath =
                    (entryDirectory / payload.GetAssetPath()).lexically_normal();
                auto rebased = payload;
                rebased.SetAssetPath(absolutePath.generic_string());
                items.Replace(payload, rebased);
            }
        };
        rebase(payloads.GetExplicitItems());
        rebase(payloads.GetAddedItems());
        rebase(payloads.GetPrependedItems());
        rebase(payloads.GetAppendedItems());
        rebase(payloads.GetDeletedItems());
    });
}

} // namespace

std::filesystem::path PointCloudCacheRootFromEnvironment() {
    const auto* value = std::getenv("USDGEO_CACHE_ROOT");
    if (!value || *value == '\0') {
        return {};
    }
    std::error_code error;
    const auto root = std::filesystem::absolute(value, error);
    return error ? std::filesystem::path{} : root.lexically_normal();
}

bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const std::filesystem::path& sourcePath,
    const GeoReference& reference,
    const usdpointcloud::PointReadRequest& request,
    const std::string& parserVersion,
    bool& hit,
    std::string& errorMessage) {
    hit = false;
    if (!layer) {
        errorMessage = "cache lookup requires a writable layer";
        return false;
    }
    const auto cacheRoot = PointCloudCacheRootFromEnvironment();
    if (cacheRoot.empty()) {
        return true;
    }

    usdgeo::cache::Descriptor descriptor;
    if (!BuildDescriptor(sourcePath, reference, request, parserVersion,
                         descriptor, errorMessage)) {
        return false;
    }
    usdgeo::cache::Layout layout;
    if (!usdgeo::cache::TryBuildLayout(cacheRoot, descriptor, layout)) {
        errorMessage = "unable to build cache layout";
        return false;
    }
    if (!usdgeo::cache::IsCacheHit(layout)) {
        return true;
    }

    const auto cachedLayer = pxr::SdfLayer::FindOrOpen(layout.rootLayer.string());
    if (!cachedLayer) {
        errorMessage = "unable to open cached root layer";
        return false;
    }
    layer->TransferContent(cachedLayer);
    RebasePayloads(layer, layout.entryDirectory);
    hit = true;
    return true;
}

} // namespace usdgeo