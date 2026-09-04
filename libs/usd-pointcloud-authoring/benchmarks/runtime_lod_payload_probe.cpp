#include "usdgeo/PointCloudLayer.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usd/stage.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct ProbeOptions {
    std::filesystem::path outputPath;
};

void PrintUsage() {
    std::cerr << "Usage: usdPointCloudAuthoring_runtime_lod_payload_probe "
                 "--output <root.usda>\n";
}

bool ParseOptions(int argc, char** argv, ProbeOptions& options) {
    if (argc != 3 || std::string(argv[1]) != "--output" ||
        std::string(argv[2]).empty()) {
        PrintUsage();
        return false;
    }
    options.outputPath = argv[2];
    return true;
}

usdgeo::PointCloudLayer::Data MakeData(std::size_t pointCount) {
    usdgeo::PointCloudLayer::Data data;
    for (std::size_t index = 0; index < pointCount; ++index) {
        data.positions.push_back({static_cast<double>(index), 0.0, 0.0});
    }
    return data;
}

usdpointcloud::PointCloudAsset MakeAsset(
    const usdgeo::GeoReference& reference,
    const usdgeo::SpatialBounds& bounds,
    std::size_t pointCount) {
    usdpointcloud::PointCloudAsset asset;
    asset.reference = reference;
    asset.bounds = bounds;
    asset.data = MakeData(pointCount);
    asset.chunk = usdpointcloud::MakePointChunk(asset.data, bounds);
    return asset;
}

std::string TileName() {
    return "Tile_L0_p0_p0_p0";
}

std::string LodPath(std::size_t index) {
    return "/PointCloud/Tiles/" + TileName() + "/LOD" +
           std::to_string(index);
}

std::size_t CountLoadedLods(const pxr::UsdStageRefPtr& stage) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < 3; ++index) {
        if (stage->GetPrimAtPath(pxr::SdfPath(LodPath(index))).IsLoaded()) {
            ++count;
        }
    }
    return count;
}

std::size_t LoadedPointCount(const pxr::UsdStageRefPtr& stage) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < 3; ++index) {
        const auto points = pxr::UsdGeomPoints::Get(
            stage, pxr::SdfPath(LodPath(index) + "/Points"));
        if (!points) {
            continue;
        }
        pxr::VtVec3fArray positions;
        if (points.GetPointsAttr().Get(&positions)) {
            count += positions.size();
        }
    }
    return count;
}

std::size_t AuthoredPayloadCount(const pxr::SdfLayerHandle& rootLayer) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < 3; ++index) {
        const auto primSpec = rootLayer->GetPrimAtPath(
            pxr::SdfPath(LodPath(index)));
        if (!primSpec) {
            continue;
        }
        count += primSpec->GetPayloadList().GetPrependedItems().size();
    }
    return count;
}

bool CreateFixture(const std::filesystem::path& outputPath) {
    std::error_code error;
    const auto absoluteOutputPath = std::filesystem::absolute(outputPath, error);
    if (error) {
        return false;
    }
    if (!absoluteOutputPath.parent_path().empty()) {
        std::filesystem::create_directories(
            absoluteOutputPath.parent_path(), error);
        if (error) {
            return false;
        }
    }

    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    reference.wkt = "WKT";
    reference.stageUpAxis = "Y";
    usdgeo::SpatialBounds bounds;
    bounds.Expand({0.0, 0.0, 0.0});
    bounds.Expand({2.0, 1.0, 1.0});

    usdpointcloud::PointLodHierarchy hierarchy;
    hierarchy.bounds = bounds;
    hierarchy.defaultIndex = 2;
    hierarchy.screenSizeThresholds = {0.25F, 0.10F};
    for (std::size_t index = 0; index < 3; ++index) {
        usdpointcloud::PointLodItem item;
        item.index = static_cast<std::uint32_t>(index);
        item.pointCount = 3 - index;
        item.bounds = bounds;
        item.sourceRange = {0, item.pointCount};
        item.spacing = static_cast<double>(index + 1);
        hierarchy.items.push_back(item);
    }

    usdgeo::PointCloudTileAsset tile;
    tile.tile.id = {0, 0, 0, 0};
    tile.tile.bounds = bounds;
    tile.tile.lod = hierarchy;
    for (std::size_t index = 0; index < 3; ++index) {
        tile.levels.push_back(MakeAsset(reference, bounds, 3 - index));
    }

    const auto payloadDirectory = absoluteOutputPath.parent_path() / "payloads";
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    if (!stage) {
        return false;
    }
    usdgeo::PointCloudPayloadOptions options;
    options.directory = payloadDirectory.string();
    options.rootLayerPath = absoluteOutputPath.string();
    if (!usdgeo::AuthorPointCloudTiledAssetWithPayloads(
            stage, "/PointCloud", {tile}, options)) {
        return false;
    }
    stage->GetRootLayer()->SetIdentifier(absoluteOutputPath.generic_string());
    if (!stage->GetRootLayer()->Export(absoluteOutputPath.string())) {
        return false;
    }
    return true;
}

bool RunProbe(const std::filesystem::path& outputPath) {
    const auto authoredStage = pxr::UsdStage::Open(outputPath.string());
    if (!authoredStage) {
        return false;
    }
    const auto authoredPayloads = AuthoredPayloadCount(
        authoredStage->GetRootLayer());
    const auto authoredLods = std::size_t{3};
    const auto defaultIndex = authoredStage->GetPrimAtPath(
        pxr::SdfPath("/PointCloud/Tiles/" + TileName()))
                                  .GetAttribute(pxr::TfToken("lod:default:index"));
    int defaultValue = -1;
    if (!defaultIndex || !defaultIndex.Get(&defaultValue)) {
        return false;
    }

    const auto lodHeuristic = authoredStage->GetPrimAtPath(pxr::SdfPath(
        "/PointCloud/LodHeuristics/" + TileName() + "/ScreenSize"));
    pxr::VtArray<float> thresholds;
    if (!lodHeuristic ||
        !lodHeuristic.GetAttribute(pxr::TfToken("thresholds"))
             .Get(&thresholds)) {
        return false;
    }

    const auto stage = pxr::UsdStage::Open(
        outputPath.string(), pxr::UsdStage::LoadNone);
    if (!stage) {
        return false;
    }
    const auto loadNoneLoadedLods = CountLoadedLods(stage);
    if (loadNoneLoadedLods != 0) {
        return false;
    }

    stage->Load(pxr::SdfPath(LodPath(1)));
    const auto selectiveLoadedLods = CountLoadedLods(stage);
    const auto selectiveLoadedPoints = LoadedPointCount(stage);
    if (selectiveLoadedLods != 1 || selectiveLoadedPoints != 2) {
        return false;
    }

    stage->Load();
    const auto allLoadedLods = CountLoadedLods(stage);
    const auto allLoadedPoints = LoadedPointCount(stage);
    if (allLoadedLods != 3 || allLoadedPoints != 6) {
        return false;
    }

    std::cout << "root\tauthored_lods\tauthored_payloads\tload_none_loaded_lods\t"
                 "selective_loaded_lods\tselective_loaded_points\t"
                 "load_all_loaded_lods\tload_all_points\tdefault_index\tthresholds\n"
              << outputPath.string() << '\t' << authoredLods << '\t'
              << authoredPayloads << '\t' << loadNoneLoadedLods << '\t'
              << selectiveLoadedLods << '\t'
              << selectiveLoadedPoints << '\t' << allLoadedLods << '\t'
              << allLoadedPoints << '\t' << defaultValue << '\t';
    for (std::size_t index = 0; index < thresholds.size(); ++index) {
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << thresholds[index];
    }
    std::cout << '\n';
    return true;
}

} // namespace

int main(int argc, char** argv) {
    ProbeOptions options;
    if (!ParseOptions(argc, argv, options)) {
        return 2;
    }
    if (!CreateFixture(options.outputPath) || !RunProbe(options.outputPath)) {
        std::cerr << "runtime LOD payload probe failed\n";
        return 1;
    }
    return 0;
}
