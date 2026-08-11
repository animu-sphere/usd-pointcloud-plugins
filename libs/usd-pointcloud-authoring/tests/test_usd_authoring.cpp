#include "usdgeo/PointCloudLayer.h"

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdLod/rootAPI.h>
#include <pxr/usd/usd/payloads.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

std::set<std::filesystem::path> ListPointSpoolDirectories() {
    std::set<std::filesystem::path> directories;
    std::error_code error;
    const auto temporaryDirectory = std::filesystem::temp_directory_path(error);
    Check(!error);
    for (const auto& entry : std::filesystem::directory_iterator(
             temporaryDirectory, error)) {
        Check(!error);
        const auto name = entry.path().filename().string();
        const auto isDirectory = entry.is_directory(error);
        Check(!error);
        if (isDirectory &&
            name.rfind("usdgeo_point_spool_", 0) == 0) {
            directories.insert(entry.path());
        }
        error.clear();
    }
    Check(!error);
    return directories;
}

bool HasNewPointSpoolFile(
    const std::set<std::filesystem::path>& existingDirectories) {
    for (const auto& directory : ListPointSpoolDirectories()) {
        if (existingDirectories.count(directory) != 0) continue;
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(
                 directory, error)) {
            Check(!error);
            const auto name = entry.path().filename().string();
            const auto isRegularFile = entry.is_regular_file(error);
            Check(!error);
            if (isRegularFile &&
                name.rfind("tile_", 0) == 0 &&
                entry.path().extension() == ".bin") {
                return true;
            }
            error.clear();
        }
        Check(!error);
    }
    return false;
}

class TestPointStream final : public usdpointcloud::PointStream {
public:
    explicit TestPointStream(
        std::vector<std::pair<usdpointcloud::PointChunk,
                              usdpointcloud::PointData>> chunks)
        : chunks_(std::move(chunks)) {}

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        diagnostic = {};
        if (index_ == chunks_.size()) {
            return usdpointcloud::PointStreamStatus::End;
        }
        chunk = chunks_[index_].first;
        data = chunks_[index_].second;
        ++index_;
        return usdpointcloud::PointStreamStatus::Chunk;
    }

private:
    std::vector<std::pair<usdpointcloud::PointChunk,
                          usdpointcloud::PointData>> chunks_;
    std::size_t index_ = 0;
};

class GeneratedPointStream final : public usdpointcloud::PointStream {
public:
    GeneratedPointStream(
        std::size_t pointCount,
        std::set<std::filesystem::path> existingSpoolDirectories)
        : pointCount_(pointCount),
          existingSpoolDirectories_(std::move(existingSpoolDirectories)) {}

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        diagnostic = {};
        if (index_ == pointCount_) {
            return usdpointcloud::PointStreamStatus::End;
        }
        sawSpoolFile_ = sawSpoolFile_ ||
                        HasNewPointSpoolFile(existingSpoolDirectories_);
        const auto tileIndex = index_ % 32;
        const auto pointInTile = index_ / 32;
        data.positions = {{static_cast<double>(tileIndex * 128 + 1),
                   0.0, static_cast<double>(pointInTile)}};
        data.intensity = {static_cast<std::uint16_t>(index_)};
        usdgeo::SpatialBounds bounds;
        bounds.Expand(data.positions.front());
        chunk = usdpointcloud::MakePointChunk(data, bounds);
        ++index_;
        return usdpointcloud::PointStreamStatus::Chunk;
    }

    bool SawSpoolFile() const noexcept { return sawSpoolFile_; }

private:
    std::size_t pointCount_ = 0;
    std::size_t index_ = 0;
    std::set<std::filesystem::path> existingSpoolDirectories_;
    bool sawSpoolFile_ = false;
};

class FailingPointStream final : public usdpointcloud::PointStream {
public:
    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        if (failed_) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, "injected stream failure"};
            return usdpointcloud::PointStreamStatus::Error;
        }
        failed_ = true;
        data.positions = {{0.0, 0.0, 0.0}};
        usdgeo::SpatialBounds bounds;
        bounds.Expand(data.positions.front());
        chunk = usdpointcloud::MakePointChunk(data, bounds);
        diagnostic = {};
        return usdpointcloud::PointStreamStatus::Chunk;
    }

private:
    bool failed_ = false;
};

void TestPointCloudRoundTrip() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    reference.wkt = "WKT";
    reference.localOrigin = {1000.0, 2000.0, 3000.0};

    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 2;
    chunk.bounds.Expand({1000.0, 2000.0, 3000.0});
    chunk.bounds.Expand({1002.0, 2003.0, 3004.0});
    chunk.attributes = {
        {"intensity", usdpointcloud::PointAttributeType::UInt16},
        {"red", usdpointcloud::PointAttributeType::UInt16},
        {"green", usdpointcloud::PointAttributeType::UInt16},
        {"blue", usdpointcloud::PointAttributeType::UInt16},
        {"classification", usdpointcloud::PointAttributeType::UInt8},
        {"classificationFlags", usdpointcloud::PointAttributeType::UInt8},
        {"scannerChannel", usdpointcloud::PointAttributeType::UInt8},
        {"scanDirectionFlag", usdpointcloud::PointAttributeType::UInt8},
        {"edgeOfFlightLine", usdpointcloud::PointAttributeType::UInt8},
        {"userData", usdpointcloud::PointAttributeType::UInt8},
        {"scanAngle", usdpointcloud::PointAttributeType::Int16},
        {"pointSourceId", usdpointcloud::PointAttributeType::UInt16},
        {"nir", usdpointcloud::PointAttributeType::UInt16},
        {"waveformDescriptorIndex", usdpointcloud::PointAttributeType::UInt8},
        {"waveformDataOffset", usdpointcloud::PointAttributeType::UInt64},
        {"waveformPacketSize", usdpointcloud::PointAttributeType::UInt32},
        {"returnPointWaveformLocation", usdpointcloud::PointAttributeType::Float32},
        {"waveformXt", usdpointcloud::PointAttributeType::Float32},
        {"waveformYt", usdpointcloud::PointAttributeType::Float32},
        {"waveformZt", usdpointcloud::PointAttributeType::Float32},
        {"waveformDataExternal", usdpointcloud::PointAttributeType::UInt8},
        {"normal", usdpointcloud::PointAttributeType::Float64Vec3}};
    const std::vector<usdgeo::Vec3d> positions = {
        {1000.0, 2000.0, 3000.0}, {1002.0, 2003.0, 3004.0}};
    usdgeo::PointCloudLayer::Data data;
    data.positions = positions;
    data.intensity = {42, 84};
    data.colorBitDepth = 16;
    data.red = {65535, 32768};
    data.green = {16384, 49152};
    data.blue = {0, 65535};
    data.classification = {2, 5};
    data.classificationFlags = {1, 3};
    data.scannerChannel = {0, 2};
    data.scanDirectionFlag = {0, 1};
    data.edgeOfFlightLine = {1, 0};
    data.userData = {7, 9};
    data.scanAngle = {-12, 34};
    data.pointSourceId = {100, 200};
    data.nir = {300, 400};
    data.waveformDescriptorIndex = {7, 8};
    data.waveformDataOffset = {1234, 5678};
    data.waveformPacketSize = {48, 64};
    data.returnPointWaveformLocation = {0.25f, 0.5f};
    data.waveformXt = {1.0f, 4.0f};
    data.waveformYt = {2.0f, 5.0f};
    data.waveformZt = {3.0f, 6.0f};
    data.waveformDataExternal = {1, 0};
    data.waveformDataFile = "sample.wdp";
    data.extraByteNames = {"normal"};
    data.extraByteComponentCounts = {3};
    data.extraBytes = {{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};

    Check(usdgeo::AuthorPointCloudAsset(
        stage, "/PointCloud", reference, chunk.bounds, chunk, data));

    const auto layerPath =
        std::filesystem::temp_directory_path() / "usd_geo_points.usda";
    Check(stage->GetRootLayer()->Export(layerPath.string()));
    const auto reopenedStage = pxr::UsdStage::Open(layerPath.string());
    Check(reopenedStage);
    std::filesystem::remove(layerPath);

    const auto points =
        pxr::UsdGeomPoints::Get(reopenedStage, pxr::SdfPath("/PointCloud"));
    Check(points.GetPrim().IsValid());
    pxr::VtVec3fArray authoredPositions;
    Check(points.GetPointsAttr().Get(&authoredPositions));
    Check(authoredPositions.size() == 2);
    Check(authoredPositions[1] == pxr::GfVec3f(2.0f, 3.0f, 4.0f));
    pxr::VtFloatArray authoredWidths;
    Check(points.GetWidthsAttr().Get(&authoredWidths));
    Check(authoredWidths.size() == 1 && authoredWidths[0] > 0.0f);
    Check(points.GetWidthsInterpolation() == pxr::UsdGeomTokens->constant);
    pxr::VtVec3fArray authoredColors;
    Check(points.GetDisplayColorPrimvar().Get(&authoredColors));
    Check(authoredColors.size() == 2 &&
          authoredColors[0] == pxr::GfVec3f(1.0f, 16384.0f / 65535.0f,
                             0.0f) &&
          authoredColors[1] == pxr::GfVec3f(32768.0f / 65535.0f,
                             49152.0f / 65535.0f, 1.0f));
    pxr::VtIntArray authoredIntensity;
    Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:intensity"))
              .Get(&authoredIntensity));
    Check(authoredIntensity.size() == 2 && authoredIntensity[1] == 84);

        pxr::VtArray<unsigned char> authoredClassificationFlags;
        Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:classificationFlags"))
              .Get(&authoredClassificationFlags));
        Check(authoredClassificationFlags.size() == 2 &&
            authoredClassificationFlags[1] == 3);
        pxr::VtIntArray authoredScanAngle;
        Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:scanAngle"))
              .Get(&authoredScanAngle));
        Check(authoredScanAngle.size() == 2 && authoredScanAngle[0] == -12);
        pxr::VtIntArray authoredNir;
        Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:nir"))
              .Get(&authoredNir));
        Check(authoredNir.size() == 2 && authoredNir[1] == 400);
          pxr::VtArray<std::uint64_t> authoredWaveformOffsets;
          Check(points.GetPrim()
                .GetAttribute(pxr::TfToken("geo:waveformDataOffset"))
                .Get(&authoredWaveformOffsets));
          Check(authoredWaveformOffsets.size() == 2 &&
              authoredWaveformOffsets[1] == 5678);
          pxr::VtArray<float> authoredWaveformParameters;
          Check(points.GetPrim()
                .GetAttribute(pxr::TfToken("geo:waveformXt"))
                .Get(&authoredWaveformParameters));
          Check(authoredWaveformParameters.size() == 2 &&
              authoredWaveformParameters[0] == 1.0f);
        std::string authoredWaveformDataFile;
        Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:waveformDataFile"))
              .Get(&authoredWaveformDataFile));
        Check(authoredWaveformDataFile == "sample.wdp");
        pxr::VtArray<pxr::GfVec3d> authoredNormals;
        Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:normal"))
                            .Get(&authoredNormals));
        Check(authoredNormals.size() == 2 &&
                    authoredNormals[1] == pxr::GfVec3d(4.0, 5.0, 6.0));

    int epsgCode = 0;
    Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:epsgCode")).Get(&epsgCode));
    Check(epsgCode == 26910);
}

void TestOptionalAttributesAreIndependent() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 1;
    chunk.bounds.Expand({0.0, 0.0, 0.0});

    usdgeo::PointCloudLayer::Data data;
    data.positions = {{0.0, 0.0, 0.0}};
    data.scanAngle = {-12};

    Check(usdgeo::AuthorPointCloudAsset(
        stage, "/PointCloud", reference, chunk.bounds, chunk, data));

    const auto prim = stage->GetPrimAtPath(pxr::SdfPath("/PointCloud"));
    Check(prim.IsValid());
    pxr::VtIntArray scanAngle;
    Check(prim.GetAttribute(pxr::TfToken("geo:scanAngle")).Get(&scanAngle));
    Check(scanAngle.size() == 1 && scanAngle[0] == -12);
    Check(!prim.GetAttribute(pxr::TfToken("geo:classificationFlags")));
}

void TestInvalidPositionDoesNotMutateStage() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 1;
    chunk.bounds.Expand({0.0, 0.0, 0.0});

    const std::vector<usdgeo::Vec3d> positions = {
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}};
    Check(!usdgeo::AuthorPointCloudAsset(
        stage, "/InvalidPointCloud", reference, chunk.bounds, chunk,
        positions));
    Check(!stage->GetPrimAtPath(pxr::SdfPath("/InvalidPointCloud")).IsValid());
}

void TestLayerAuthoringEntryPoint() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("usd_geo_asset.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    reference.stageUpAxis = "Y";

    usdpointcloud::PointCloudAsset asset;
    asset.reference = reference;
    asset.data.positions = {{1000.0, 2000.0, 3000.0}};
    asset.bounds.Expand({0.0, 0.0, 0.0});
    asset.chunk = usdpointcloud::MakePointChunk(asset.data, asset.bounds);

    Check(usdgeo::AuthorPointCloudAsset(
        layer.operator->(), "/PointCloud", asset));
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    Check(pxr::UsdGeomGetStageUpAxis(stage) == pxr::TfToken("Y"));
    Check(stage->GetPrimAtPath(pxr::SdfPath("/PointCloud")).IsValid());

    usdgeo::PointCloudAuthorFailure failure;
    Check(!usdgeo::AuthorPointCloudAsset(
        nullptr, "/PointCloud", asset, failure));
    Check(failure == usdgeo::PointCloudAuthorFailure::InvalidLayer);
}

void TestMetadataAuthoringAllowsEmptyPointCloud() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("metadata.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    usdgeo::SpatialBounds bounds;
    bounds.Expand({0.0, 0.0, 0.0});

    usdpointcloud::PointChunk chunk;
    chunk.bounds = bounds;
    chunk.attributes = {{"xyz", usdpointcloud::PointAttributeType::Float64}};

    Check(usdgeo::AuthorPointCloudMetadata(
        layer.operator->(), "/PointCloud", reference, bounds, chunk,
        {3, {0.01, 0.01, 0.01}, {1000.0, 2000.0, 3000.0}}));
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath("/PointCloud"));
    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.empty());
    std::uint64_t pointCount = 1;
    Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:pointCount"))
              .Get(&pointCount));
    Check(pointCount == 0);
}

void TestLodAuthoring() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    std::vector<usdpointcloud::PointCloudAsset> levels(2);
    for (auto& level : levels) {
        level.reference = reference;
        level.data.positions = {{0.0, 0.0, 0.0}};
        level.bounds.Expand({0.0, 0.0, 0.0});
        level.chunk = usdpointcloud::MakePointChunk(level.data, level.bounds);
    }
    usdpointcloud::PointLodHierarchy hierarchy;
    hierarchy.bounds = levels[0].bounds;
    hierarchy.items = {{0, 1, hierarchy.bounds, {0, 1}},
                        {1, 1, hierarchy.bounds, {0, 1}}};
    hierarchy.screenSizeThresholds = {0.1f};

    Check(usdgeo::AuthorPointCloudLodAsset(
        stage, "/PointCloud", levels, hierarchy));
    const auto prim = stage->GetPrimAtPath(pxr::SdfPath("/PointCloud"));
    Check(prim.HasAPI<pxr::UsdLodRootAPI>());
    Check(prim.GetAttribute(pxr::TfToken("lod:default:index")).IsValid());
    Check(stage->GetPrimAtPath(pxr::SdfPath("/PointCloud/LOD0")).IsValid());
    Check(stage->GetPrimAtPath(pxr::SdfPath("/PointCloud/LOD1")).IsValid());
    Check(stage->GetPrimAtPath(
              pxr::SdfPath("/PointCloud/LodHeuristics/ScreenSize"))
              .IsValid());
}

void TestLodAuthoringRejectsMismatchedMetadata() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointCloudAsset level;
    level.reference = reference;
    level.data.positions = {{0.0, 0.0, 0.0}};
    level.bounds.Expand({0.0, 0.0, 0.0});
    level.chunk = usdpointcloud::MakePointChunk(level.data, level.bounds);

    usdpointcloud::PointLodHierarchy hierarchy;
    hierarchy.bounds = level.bounds;
    hierarchy.items = {{0, 2, hierarchy.bounds, {0, 2}}};

    Check(!usdgeo::AuthorPointCloudLodAsset(
        stage, "/PointCloud", {level}, hierarchy));
    Check(!stage->GetPrimAtPath(pxr::SdfPath("/PointCloud")).IsValid());
}

void TestLodAuthoringRejectsInvalidLevelWithoutMutation() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointCloudAsset validLevel;
    validLevel.reference = reference;
    validLevel.data.positions = {{0.0, 0.0, 0.0}};
    validLevel.bounds.Expand({0.0, 0.0, 0.0});
    validLevel.chunk = usdpointcloud::MakePointChunk(
        validLevel.data, validLevel.bounds);
    auto invalidLevel = validLevel;
    invalidLevel.reference.epsgCode.reset();

    usdpointcloud::PointLodHierarchy hierarchy;
    hierarchy.bounds = validLevel.bounds;
    hierarchy.items = {{0, 1, hierarchy.bounds, {0, 1}},
                        {1, 1, hierarchy.bounds, {0, 1}}};

    Check(!usdgeo::AuthorPointCloudLodAsset(
        stage, "/PointCloud", {validLevel, invalidLevel}, hierarchy));
    Check(!stage->GetPrimAtPath(pxr::SdfPath("/PointCloud")).IsValid());
}

void TestTiledLodAuthoring() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    const auto makeTile = [&](const usdpointcloud::PointTileId& id,
                              double x) {
        usdgeo::PointCloudTileAsset result;
        result.tile.id = id;
        result.tile.bounds.Expand({x, 0.0, 0.0});
        result.tile.lod.bounds = result.tile.bounds;
        result.tile.lod.screenSizeThresholds = {0.1F};
        result.tile.lod.items = {
            {0, 1, result.tile.bounds, {0, 1}},
            {1, 1, result.tile.bounds, {0, 1}}};
        for (int index = 0; index < 2; ++index) {
            usdpointcloud::PointCloudAsset level;
            level.reference = reference;
            level.bounds = result.tile.bounds;
            level.data.positions = {{x, 0.0, 0.0}};
            level.chunk = usdpointcloud::MakePointChunk(
                level.data, level.bounds);
            result.levels.push_back(std::move(level));
        }
        return result;
    };

    Check(usdgeo::AuthorPointCloudTiledAsset(
        stage, "/PointCloud",
        {makeTile({0, 0, 0, 0}, 0.0), makeTile({0, 1, 0, 0}, 10.0)}));
    Check(stage->GetPrimAtPath(pxr::SdfPath(
              "/PointCloud/Tiles/Tile_L0_p0_p0_p0"))
              .HasAPI<pxr::UsdLodRootAPI>());
    Check(stage->GetPrimAtPath(pxr::SdfPath(
              "/PointCloud/Tiles/Tile_L0_p1_p0_p0/LOD1"))
              .IsValid());
    Check(stage->GetPrimAtPath(pxr::SdfPath(
              "/PointCloud/LodHeuristics/ScreenSize"))
              .IsValid());
}

void TestTiledLodAuthoringSupportsNegativeTileCoordinates() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    usdgeo::PointCloudTileAsset tile;
    tile.tile.id = {0, std::numeric_limits<std::int64_t>::min(), -2, 3};
    tile.tile.bounds.Expand({0.0, 0.0, 0.0});
    tile.tile.lod.bounds = tile.tile.bounds;
    tile.tile.lod.items = {{0, 1, tile.tile.bounds, {0, 1}}};
    usdpointcloud::PointCloudAsset level;
    level.reference = reference;
    level.bounds = tile.tile.bounds;
    level.data.positions = {{0.0, 0.0, 0.0}};
    level.chunk = usdpointcloud::MakePointChunk(level.data, level.bounds);
    tile.levels.push_back(std::move(level));

    Check(usdgeo::AuthorPointCloudTiledAsset(stage, "/PointCloud", {tile}));
    Check(stage->GetPrimAtPath(pxr::SdfPath(
              "/PointCloud/Tiles/Tile_L0_n9223372036854775808_n2_p3"))
              .IsValid());
}

void TestTiledLodAuthoringRejectsMismatchedThresholds() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    usdgeo::PointCloudTileAsset tile;
    tile.tile.id = {0, 0, 0, 0};
    tile.tile.bounds.Expand({0.0, 0.0, 0.0});
    tile.tile.lod.bounds = tile.tile.bounds;
    tile.tile.lod.items = {{0, 1, tile.tile.bounds, {0, 1}}};
    usdpointcloud::PointCloudAsset level;
    level.reference = reference;
    level.bounds = tile.tile.bounds;
    level.data.positions = {{0.0, 0.0, 0.0}};
    level.chunk = usdpointcloud::MakePointChunk(level.data, level.bounds);
    tile.levels.push_back(level);
    auto secondTile = tile;
    secondTile.tile.id.x = 1;
    secondTile.tile.lod.screenSizeThresholds = {0.1F};

    Check(!usdgeo::AuthorPointCloudTiledAsset(
        stage, "/PointCloud", {tile, secondTile}));
    Check(!stage->GetPrimAtPath(pxr::SdfPath("/PointCloud")).IsValid());
}

void TestTiledLodPayloadAuthoring() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdgeo::PointCloudTileAsset tile;
    tile.tile.id = {0, 0, 0, 0};
    tile.tile.bounds.Expand({0.0, 0.0, 0.0});
    tile.tile.lod.bounds = tile.tile.bounds;
    tile.tile.lod.items = {{0, 1, tile.tile.bounds, {0, 1}}};

    usdpointcloud::PointCloudAsset level;
    level.reference = reference;
    level.bounds = tile.tile.bounds;
    level.data.positions = {{0.0, 0.0, 0.0}};
    level.chunk = usdpointcloud::MakePointChunk(level.data, level.bounds);
    tile.levels.push_back(std::move(level));

    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_payloads";
    const auto rootLayerPath = payloadDirectory / "PointCloud.usda";
    std::filesystem::remove_all(payloadDirectory);
    Check(usdgeo::AuthorPointCloudTiledAssetWithPayloads(
        stage, "/PointCloud", {tile},
        {payloadDirectory.string(), rootLayerPath.string()}));
    Check(stage->GetRootLayer()->GetIdentifier() == rootLayerPath.generic_string());

    const auto lodPrim = stage->GetPrimAtPath(pxr::SdfPath(
        "/PointCloud/Tiles/Tile_L0_p0_p0_p0/LOD0"));
    Check(lodPrim.IsValid());
    Check(std::filesystem::exists(
        payloadDirectory / "Tile_L0_p0_p0_p0_LOD0.usdc"));
    Check(stage->GetRootLayer()->Export(rootLayerPath.string()));
    const auto reopenedStage = pxr::UsdStage::Open(rootLayerPath.string());
    Check(reopenedStage);
    const auto points = pxr::UsdGeomPoints::Get(
        reopenedStage,
        pxr::SdfPath("/PointCloud/Tiles/Tile_L0_p0_p0_p0/LOD0/Points"));
    Check(points.GetPrim().IsValid());
    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.size() == 1 && positions[0] == pxr::GfVec3f(0.0f));
    std::filesystem::remove_all(payloadDirectory);

    const auto failedStage = usdgeo::PointCloudLayer::CreateStage();
    const auto failedIdentifier = failedStage->GetRootLayer()->GetIdentifier();
    std::filesystem::create_directories(payloadDirectory);
    {
        std::ofstream conflict(
            payloadDirectory / "Tile_L0_p0_p0_p0_LOD0.usdc",
            std::ios::binary);
        conflict << "pre-existing payload";
    }
    Check(!usdgeo::AuthorPointCloudTiledAssetWithPayloads(
        failedStage, "/PointCloud", {tile},
        {payloadDirectory.string(), rootLayerPath.string()}));
    Check(failedStage->GetRootLayer()->GetIdentifier() == failedIdentifier);
    std::filesystem::remove_all(payloadDirectory);

    const auto invalidPayloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_invalid_payloads";
    const auto invalidRootLayerPath = invalidPayloadDirectory / "PointCloud.usda";
    std::filesystem::remove_all(invalidPayloadDirectory);
    auto invalidTile = tile;
    invalidTile.tile.lod.items.clear();
    const auto invalidStage = usdgeo::PointCloudLayer::CreateStage();
    Check(!usdgeo::AuthorPointCloudTiledAssetWithPayloads(
        invalidStage, "/PointCloud", {invalidTile},
        {invalidPayloadDirectory.string(), invalidRootLayerPath.string()}));
    Check(!std::filesystem::exists(invalidPayloadDirectory));
}

void TestStreamTiledPayloadAuthoring() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("streamed.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    reference.localOrigin = {0.0, 0.0, 0.0};

    const auto makeChunk = [](const std::vector<usdgeo::Vec3d>& positions,
                              const std::vector<std::uint16_t>& intensity,
                              const std::vector<double>& temperatures) {
        usdpointcloud::PointData data;
        data.positions = positions;
        data.intensity = intensity;
        data.extraByteNames = {"temperature (C)"};
        data.extraByteComponentCounts = {1};
        data.extraBytes = {temperatures};
        usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
        for (const auto& position : positions) bounds.Expand(position);
        return std::make_pair(usdpointcloud::MakePointChunk(data, bounds),
                              std::move(data));
    };
    TestPointStream stream({
        makeChunk({{0.25, 0.0, 0.0}, {1.25, 0.0, 0.0}}, {10, 20},
                  {12.5, 18.0}),
        makeChunk({{-0.25, 0.0, 0.0}}, {30}, {7.0}),
    });

    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_stream_payloads";
    const auto rootLayerPath = payloadDirectory / "PointCloud.usda";
    std::filesystem::remove_all(payloadDirectory);
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {1.0, 0},
        {payloadDirectory.string(), rootLayerPath.string(), 1}, diagnostics));
    Check(diagnostics.empty());
    Check(layer->Export(rootLayerPath.string()));

    const auto stage = pxr::UsdStage::Open(rootLayerPath.string());
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath(
            "/PointCloud/Tiles/Tile_L0_p1_p0_p0/LOD0/Points"));
    Check(points.GetPrim().IsValid());
    pxr::VtIntArray authoredIntensity;
    Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:intensity"))
              .Get(&authoredIntensity));
    Check(authoredIntensity.size() == 1 && authoredIntensity[0] == 20);
    pxr::VtArray<double> authoredTemperature;
    Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:temperature__C_"))
              .Get(&authoredTemperature));
    Check(authoredTemperature.size() == 1 && authoredTemperature[0] == 18.0);
    Check(std::filesystem::exists(
        payloadDirectory / "Tile_L0_n1_p0_p0_LOD0.usdc"));
    Check(std::filesystem::exists(
        payloadDirectory / "Tile_L0_p0_p0_p0_LOD0.usdc"));
    std::filesystem::remove_all(payloadDirectory);
}

void TestGeneratedStreamTiledPayloadAuthoring() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("generated_stream.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    const auto spoolDirectoriesBefore = ListPointSpoolDirectories();
    GeneratedPointStream stream(131072, spoolDirectoriesBefore);
    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_generated_payloads";
    const auto rootLayerPath = payloadDirectory / "PointCloud.usda";
    std::filesystem::remove_all(payloadDirectory);
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {128.0, 0},
        {payloadDirectory.string(), rootLayerPath.string(), 1024}, diagnostics));
    Check(diagnostics.empty());
    Check(std::filesystem::exists(
        payloadDirectory / "Tile_L0_p0_p0_p0_LOD0.usdc"));
    Check(std::filesystem::exists(
        payloadDirectory / "Tile_L0_p31_p0_p0_LOD0.usdc"));
    Check(layer->GetPrimAtPath(pxr::SdfPath(
              "/PointCloud/Tiles/Tile_L0_p31_p0_p0")) != nullptr);
    Check(stream.SawSpoolFile());
    Check(ListPointSpoolDirectories() == spoolDirectoriesBefore);
    std::filesystem::remove_all(payloadDirectory);
}

void TestStreamCancellationCleansSpools() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("cancelled_stream.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointData data;
    data.positions = {{0.25, 0.0, 0.0}, {0.75, 0.0, 0.0}};
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    for (const auto& position : data.positions) bounds.Expand(position);
    TestPointStream stream({{
        usdpointcloud::MakePointChunk(data, bounds), data}});

    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_cancelled_payloads";
    std::filesystem::remove_all(payloadDirectory);
    const auto spoolDirectoriesBefore = ListPointSpoolDirectories();
    int cancellationChecks = 0;
    std::vector<usdgeo::Diagnostic> diagnostics;
    const usdgeo::PointCloudPayloadOptions options{
        payloadDirectory.string(),
        (payloadDirectory / "PointCloud.usda").string(),
        1,
        [&cancellationChecks]() { return ++cancellationChecks >= 3; }};

    Check(!usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {1.0, 0},
        options, diagnostics));
    Check(!diagnostics.empty());
    Check(ListPointSpoolDirectories() == spoolDirectoriesBefore);
    Check(!std::filesystem::exists(payloadDirectory));
}

void TestStreamCancellationDuringSpoolReadCleansSpools() {
    const auto layer = pxr::SdfLayer::CreateAnonymous(
        "cancelled_spool_read.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    usdpointcloud::PointData data;
    data.positions = {{0.25, 0.0, 0.0}};
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    bounds.Expand(data.positions.front());
    TestPointStream stream({{
        usdpointcloud::MakePointChunk(data, bounds), data}});

    const auto payloadDirectory =
        std::filesystem::temp_directory_path() /
        "usd_geo_cancelled_spool_read_payloads";
    std::filesystem::remove_all(payloadDirectory);
    const auto spoolDirectoriesBefore = ListPointSpoolDirectories();
    int cancellationChecks = 0;
    std::vector<usdgeo::Diagnostic> diagnostics;
    const usdgeo::PointCloudPayloadOptions options{
        payloadDirectory.string(),
        (payloadDirectory / "PointCloud.usda").string(),
        1,
        [&cancellationChecks]() { return ++cancellationChecks >= 6; }};

    Check(!usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {1.0, 0},
        options, diagnostics));
    Check(!diagnostics.empty());
    Check(ListPointSpoolDirectories() == spoolDirectoriesBefore);
    Check(!std::filesystem::exists(payloadDirectory));
}

void TestStreamFailureDoesNotMutateLayer() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("failed_stream.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    FailingPointStream stream;
    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_failed_payloads";
    std::filesystem::remove_all(payloadDirectory);
    std::vector<usdgeo::Diagnostic> diagnostics;

    Check(!usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {1.0, 0},
        {payloadDirectory.string(),
         (payloadDirectory / "PointCloud.usda").string(), 1}, diagnostics));
    Check(!diagnostics.empty());
    Check(layer->GetPrimAtPath(pxr::SdfPath("/PointCloud")) == nullptr);
    Check(!std::filesystem::exists(payloadDirectory));
}

void TestStreamPayloadFailureRollsBackGeneratedPayloads() {
    const auto layer = pxr::SdfLayer::CreateAnonymous("payload_failure.usda");
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;

    const auto makeChunk = [](double x) {
        usdpointcloud::PointData data;
        data.positions = {{x, 0.0, 0.0}};
        usdgeo::SpatialBounds bounds;
        bounds.Expand(data.positions.front());
        return std::make_pair(usdpointcloud::MakePointChunk(data, bounds),
                              std::move(data));
    };
    TestPointStream stream({makeChunk(0.0), makeChunk(1.0)});
    const auto payloadDirectory =
        std::filesystem::temp_directory_path() / "usd_geo_payload_failure";
    const auto firstPayload =
        payloadDirectory / "Tile_L0_p0_p0_p0_LOD0.usdc";
    const auto conflictingPayload =
        payloadDirectory / "Tile_L0_p1_p0_p0_LOD0.usdc";
    std::filesystem::remove_all(payloadDirectory);
    std::filesystem::create_directories(payloadDirectory);
    {
        std::ofstream conflict(conflictingPayload, std::ios::binary);
        conflict << "pre-existing payload";
    }

    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference, {1.0, 0},
        {payloadDirectory.string(),
         (payloadDirectory / "PointCloud.usda").string(), 1}, diagnostics));
    Check(!diagnostics.empty());
    Check(layer->GetPrimAtPath(pxr::SdfPath("/PointCloud")) == nullptr);
    Check(!std::filesystem::exists(firstPayload));
    Check(std::filesystem::exists(conflictingPayload));
    std::filesystem::remove_all(payloadDirectory);
}

void TestInvalidExtraByteNameDoesNotAuthor() {
    const auto stage = usdgeo::PointCloudLayer::CreateStage();
    usdgeo::GeoReference reference;
    reference.localOrigin = {0.0, 0.0, 0.0};

    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 1;
    chunk.bounds.Expand({0.0, 0.0, 0.0});
    chunk.attributes = {
        {"temperature (C)", usdpointcloud::PointAttributeType::Float64}};
    usdpointcloud::PointData data;
    data.positions = {{0.0, 0.0, 0.0}};
    data.extraByteNames = {"temperature (C)"};
    data.extraBytes = {{21.5}};

    Check(!usdgeo::AuthorPointCloudAsset(
        stage, "/PointCloud", reference, chunk.bounds, chunk, data));
}

} // namespace

int main() {
    TestPointCloudRoundTrip();
    TestOptionalAttributesAreIndependent();
    TestInvalidPositionDoesNotMutateStage();
    TestLayerAuthoringEntryPoint();
    TestMetadataAuthoringAllowsEmptyPointCloud();
    TestLodAuthoring();
    TestLodAuthoringRejectsMismatchedMetadata();
    TestLodAuthoringRejectsInvalidLevelWithoutMutation();
    TestTiledLodAuthoring();
    TestTiledLodAuthoringSupportsNegativeTileCoordinates();
    TestInvalidExtraByteNameDoesNotAuthor();
    TestTiledLodAuthoringRejectsMismatchedThresholds();
    TestTiledLodPayloadAuthoring();
    TestStreamTiledPayloadAuthoring();
    TestGeneratedStreamTiledPayloadAuthoring();
    TestStreamCancellationCleansSpools();
    TestStreamCancellationDuringSpoolReadCleansSpools();
    TestStreamFailureDoesNotMutateLayer();
    TestStreamPayloadFailureRollsBackGeneratedPayloads();
    return 0;
}