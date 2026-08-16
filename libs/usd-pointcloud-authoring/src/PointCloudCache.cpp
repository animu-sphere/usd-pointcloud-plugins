#include "usdgeo/PointCloudCache.h"
#include "usdpointcloud/Tiling.h"

#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/sdf/primSpec.h>

#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <vector>

namespace usdgeo {
namespace {

bool BuildResolverSourceIdentity(
    const pxr::ArResolver& resolver,
    const std::string& assetPath,
    const pxr::ArResolvedPath& resolvedPath,
    const pxr::ArAsset& asset,
    cache::SourceIdentity& identity,
    cache::ResolverIdentityStability& stability,
    std::string& errorMessage) {
    identity = {};
    stability = cache::ResolverIdentityStability::Unavailable;
    const auto assetInfo = resolver.GetAssetInfo(assetPath, resolvedPath);
    auto validationToken = assetInfo.version;
    if (validationToken.empty()) {
        const auto timestamp =
            resolver.GetModificationTimestamp(assetPath, resolvedPath);
        if (timestamp.IsValid()) {
            validationToken = "resolver-mtime:" +
                              std::to_string(timestamp.GetTime());
        }
    }
    const cache::ResolverAssetIdentity assetIdentity{
        resolvedPath.GetPathString(),
        static_cast<std::uintmax_t>(asset.GetSize()),
        validationToken};
    return cache::TryBuildResolverSourceIdentity(
        assetIdentity, identity, stability, errorMessage);
}

std::string FormatDouble(double value) {
    std::ostringstream result;
    result << std::setprecision(17) << value;
    return result.str();
}

bool BuildDescriptor(const usdgeo::cache::SourceIdentity& sourceIdentity,
                     const GeoReference& reference,
                     const usdpointcloud::PointReadRequest& request,
                     const std::string& parserVersion,
                     usdgeo::cache::Descriptor& descriptor,
                     std::string& errorMessage) {
    descriptor.source = sourceIdentity;

    descriptor.pluginVersion = "usd-pointcloud-plugins-0.3.0-display-v2";
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
    if (request.maxPointsPerTile) {
        descriptor.tileAndLod.emplace_back(
            "planner.id", usdpointcloud::kAdaptivePointBudgetPlannerId);
        descriptor.tileAndLod.emplace_back(
            "planner.version",
            std::to_string(usdpointcloud::kAdaptivePointBudgetPlannerVersion));
    }
    descriptor.downsampling = {{"algorithm", "fixed-stride"},
                               {"version", "1"}};
    if (!descriptor.IsValid()) {
        errorMessage = "cache descriptor is invalid";
        return false;
    }
    return true;
}

bool IsWithin(const std::filesystem::path& path,
              const std::filesystem::path& directory) {
    const auto relative = path.lexically_normal().lexically_relative(
        directory.lexically_normal());
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    for (const auto& component : relative) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool IsValidCachedPayloadPath(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& payloadDirectory) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(sourcePath, error) || error) {
        return false;
    }
    const auto resolvedSourcePath =
        std::filesystem::weakly_canonical(sourcePath, error);
    if (error) {
        return false;
    }
    const auto resolvedPayloadDirectory =
        std::filesystem::weakly_canonical(payloadDirectory, error);
    return !error &&
           IsWithin(resolvedSourcePath, resolvedPayloadDirectory);
}

bool ValidateCachedPayloads(
    const pxr::SdfLayerHandle& layer,
    const usdgeo::cache::Layout& layout) {
    bool valid = true;
    layer->Traverse(pxr::SdfPath::AbsoluteRootPath(),
                    [&](const pxr::SdfPath& path) {
        const auto prim = layer->GetPrimAtPath(path);
        if (!prim || !prim->HasPayloads()) {
            return;
        }
        const auto payloads = prim->GetPayloadList();
        const auto validate = [&](auto items) {
            for (const auto& item : items) {
                const pxr::SdfPayload payload = item;
                const auto assetPath = payload.GetAssetPath();
                if (assetPath.empty()) {
                    continue;
                }
                const std::filesystem::path sourcePath =
                    (layout.entryDirectory / assetPath).lexically_normal();
                if (std::filesystem::path(assetPath).is_absolute() ||
                    !IsWithin(sourcePath, layout.payloadDirectory) ||
                    !IsValidCachedPayloadPath(sourcePath,
                                              layout.payloadDirectory) ||
                    !pxr::SdfLayer::FindOrOpen(sourcePath.string())) {
                    valid = false;
                    return;
                }
            }
        };
        validate(payloads.GetExplicitItems());
        validate(payloads.GetAddedItems());
        validate(payloads.GetPrependedItems());
        validate(payloads.GetAppendedItems());
    });
    return valid;
}

bool MaterializePayloads(
    const pxr::SdfLayerHandle& layer,
    const usdgeo::cache::Layout& layout,
    const std::filesystem::path& targetDirectory) {
    const auto sourceDirectory = layout.payloadDirectory.lexically_normal();
    const auto normalizedTarget = targetDirectory.lexically_normal();

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> files;
    bool valid = true;
    layer->Traverse(pxr::SdfPath::AbsoluteRootPath(),
                    [&](const pxr::SdfPath& path) {
        const auto prim = layer->GetPrimAtPath(path);
        if (!prim || !prim->HasPayloads()) {
            return;
        }
        const auto payloads = prim->GetPayloadList();
        const auto collect = [&](auto items) {
            for (const auto& item : items) {
                const pxr::SdfPayload payload = item;
                const auto assetPath = payload.GetAssetPath();
                if (assetPath.empty()) {
                    continue;
                }
                const std::filesystem::path sourcePath =
                    (layout.entryDirectory / assetPath).lexically_normal();
                if (std::filesystem::path(assetPath).is_absolute() ||
                    !IsWithin(sourcePath, sourceDirectory)) {
                    valid = false;
                    return;
                }
                const auto relative =
                    sourcePath.lexically_relative(sourceDirectory);
                const auto targetPath =
                    (normalizedTarget / relative).lexically_normal();
                if (!IsValidCachedPayloadPath(sourcePath,
                                              sourceDirectory)) {
                    valid = false;
                    return;
                }
                files.emplace_back(sourcePath, targetPath);
            }
        };
        collect(payloads.GetExplicitItems());
        collect(payloads.GetAddedItems());
        collect(payloads.GetPrependedItems());
        collect(payloads.GetAppendedItems());
    });
    if (!valid) {
        return false;
    }

    std::set<std::filesystem::path> createdFiles;
    std::error_code error;
    for (const auto& [sourcePath, targetPath] : files) {
        static_cast<void>(sourcePath);
        const auto targetExists = std::filesystem::exists(targetPath, error);
        if (error || (targetExists &&
                      !std::filesystem::is_regular_file(targetPath, error)) ||
            error) {
            valid = false;
            break;
        }
        if (targetExists) {
            continue;
        }
        std::filesystem::create_directories(targetPath.parent_path(), error);
        if (error || !std::filesystem::copy_file(
                         sourcePath, targetPath,
                         std::filesystem::copy_options::none, error) ||
            error) {
            valid = false;
            break;
        }
        createdFiles.insert(targetPath);
    }
    if (!valid) {
        for (const auto& path : createdFiles) {
            std::filesystem::remove(path, error);
            error.clear();
        }
    }
    return valid;
}

void RebasePayloads(pxr::SdfLayer* layer,
                    const std::filesystem::path& sourceBaseDirectory,
                    const std::filesystem::path& targetBaseDirectory,
                    const std::filesystem::path& layerBaseDirectory) {
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
                const auto sourcePath =
                    (sourceBaseDirectory / payload.GetAssetPath()).lexically_normal();
                const auto targetPath =
                    (targetBaseDirectory /
                     sourcePath.lexically_relative(
                         (sourceBaseDirectory / "payloads").lexically_normal()))
                        .lexically_normal();
                const auto relativeTarget = targetPath.lexically_relative(
                    layerBaseDirectory.lexically_normal());
                auto rebased = payload;
                rebased.SetAssetPath(
                    (relativeTarget.empty() ? targetPath : relativeTarget)
                        .generic_string());
                items.Replace(payload, rebased);
            }
        };
        rebase(payloads.GetExplicitItems());
        rebase(payloads.GetAddedItems());
        rebase(payloads.GetPrependedItems());
        rebase(payloads.GetAppendedItems());
    });
}

std::filesystem::path LayerBaseDirectory(
    const pxr::SdfLayer* layer,
    const std::filesystem::path& fallbackDirectory) {
    if (layer && !layer->IsAnonymous()) {
        const auto realPath = layer->GetRealPath();
        if (!realPath.empty()) {
            return std::filesystem::path(realPath).parent_path();
        }
    }
    return fallbackDirectory;
}

} // namespace

bool TryBuildResolverSourceIdentity(
    const pxr::ArResolver& resolver,
    const std::string& assetPath,
    const pxr::ArResolvedPath& resolvedPath,
    const pxr::ArAsset& asset,
    cache::SourceIdentity& identity,
    cache::ResolverIdentityStability& stability,
    std::string& errorMessage) {
    return BuildResolverSourceIdentity(resolver, assetPath, resolvedPath, asset,
                                       identity, stability, errorMessage);
}

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
    if (PointCloudCacheRootFromEnvironment().empty()) {
        return true;
    }

    usdgeo::cache::SourceIdentity sourceIdentity;
    if (!usdgeo::cache::TryBuildLocalSourceIdentity(
            sourcePath, sourceIdentity, errorMessage)) {
        return false;
    }
    return TryLoadPointCloudCache(
        layer, sourceIdentity, sourcePath.parent_path(), reference, request,
        parserVersion, hit, errorMessage);
}

bool TryLoadPointCloudCache(
    pxr::SdfLayer* layer,
    const cache::SourceIdentity& sourceIdentity,
    const std::filesystem::path& payloadBaseDirectory,
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
    if (!BuildDescriptor(sourceIdentity, reference, request, parserVersion,
                         descriptor, errorMessage)) {
        return false;
    }
    usdgeo::cache::Layout layout;
    if (!usdgeo::cache::TryBuildLayout(cacheRoot, descriptor, layout)) {
        errorMessage = "unable to build cache layout";
        return false;
    }
    const auto lookup = usdgeo::cache::Inspect(layout);
    if (lookup.status == usdgeo::cache::LookupStatus::Incomplete) {
        usdgeo::cache::Invalidate(cacheRoot, descriptor);
    }
    if (!lookup.IsHit()) {
        return true;
    }

    const auto cachedLayer = pxr::SdfLayer::FindOrOpen(layout.rootLayer.string());
    if (!cachedLayer) {
        usdgeo::cache::Invalidate(cacheRoot, descriptor);
        return true;
    }
    if (!ValidateCachedPayloads(cachedLayer, layout)) {
        usdgeo::cache::Invalidate(cacheRoot, descriptor);
        return true;
    }

    std::filesystem::path targetPayloadDirectory;
    if (!request.payloadDirectory.empty()) {
        targetPayloadDirectory = request.payloadDirectory;
        if (targetPayloadDirectory.is_relative()) {
            if (payloadBaseDirectory.empty()) {
                return true;
            }
            targetPayloadDirectory = payloadBaseDirectory /
                                     targetPayloadDirectory;
        }
        std::error_code error;
        targetPayloadDirectory =
            std::filesystem::absolute(targetPayloadDirectory, error);
        if (error || !MaterializePayloads(
                         cachedLayer, layout, targetPayloadDirectory)) {
            return true;
        }
    }
    layer->TransferContent(cachedLayer);
    const auto layerBaseDirectory =
        LayerBaseDirectory(layer, payloadBaseDirectory);
    RebasePayloads(
        layer, layout.entryDirectory,
        targetPayloadDirectory.empty() ? layout.payloadDirectory
                                       : targetPayloadDirectory,
        layerBaseDirectory);
    hit = true;
    return true;
}

} // namespace usdgeo