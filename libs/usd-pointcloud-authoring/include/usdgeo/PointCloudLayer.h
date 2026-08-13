#pragma once

#include "usdgeo/GeoReference.h"
#include "usdpointcloud/PointCloud.h"
#include "usdpointcloud/Lod.h"
#include "usdpointcloud/Spool.h"
#include "usdpointcloud/Tiling.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace usdgeo {

class PointCloudLayer {
public:
    using Data = usdpointcloud::PointData;

    static pxr::UsdStageRefPtr CreateStage();

    static bool AuthorPointCloud(const pxr::UsdStageRefPtr& stage,
                                 const std::string& primPath,
                                 const GeoReference& reference,
                                 const SpatialBounds& bounds,
                                 const usdpointcloud::PointChunk& chunk,
                                 const std::vector<Vec3d>& positions);

    static bool AuthorPointCloud(const pxr::UsdStageRefPtr& stage,
                                 const std::string& primPath,
                                 const GeoReference& reference,
                                 const SpatialBounds& bounds,
                                 const usdpointcloud::PointChunk& chunk,
                                 const Data& data);
};

enum class PointCloudAuthorFailure {
    None,
    InvalidLayer,
    StageCreation,
    StageMetrics,
    PointCloud,
};

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const usdpointcloud::PointCloudAsset& asset);

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const usdpointcloud::PointCloudAsset& asset,
    PointCloudAuthorFailure& failure);

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data);

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data,
    PointCloudAuthorFailure& failure);

bool AuthorPointCloudAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const std::vector<Vec3d>& positions);

bool AuthorPointCloudAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data);

bool AuthorPointCloudMetadata(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const struct PointCloudSourceMetadata& sourceMetadata);

bool AuthorPointCloudLodAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<usdpointcloud::PointCloudAsset>& levels,
    const usdpointcloud::PointLodHierarchy& hierarchy);

bool AuthorPointCloudLodAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const std::vector<usdpointcloud::PointCloudAsset>& levels,
    const usdpointcloud::PointLodHierarchy& hierarchy);

struct PointCloudTileAsset {
    usdpointcloud::PointTile tile;
    std::vector<usdpointcloud::PointCloudAsset> levels;
};

struct PointCloudSourceMetadata {
    std::uint8_t pointFormat = 0;
    Vec3d scale;
    Vec3d offset;
};

struct PointCloudPayloadOptions {
    std::string directory;
    std::string rootLayerPath;
    std::size_t tileMemoryLimitBytes = 64 * 1024 * 1024;
    std::function<bool()> isCancelled;
    std::function<void(std::size_t)> onBufferedBytes;
    std::vector<usdpointcloud::PointTileManifestEntry>* tileManifestEntries =
        nullptr;
};

bool AuthorPointCloudTiledAssetFromStream(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    usdpointcloud::PointStream& stream,
    const GeoReference& reference,
    const usdpointcloud::TileRouter& router,
    const PointCloudPayloadOptions& options,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool AuthorPointCloudTiledAssetFromStream(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    usdpointcloud::PointStream& stream,
    const GeoReference& reference,
    const usdpointcloud::TileGridConfig& tileConfig,
    const PointCloudPayloadOptions& options,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool AuthorPointCloudTiledAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles);

bool AuthorPointCloudTiledAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles);

bool AuthorPointCloudTiledAssetWithPayloads(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles,
    const PointCloudPayloadOptions& options);

bool AuthorPointCloudTiledAssetWithPayloads(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles,
    const PointCloudPayloadOptions& options,
    std::vector<std::filesystem::path>& generatedPayloads);

} // namespace usdgeo