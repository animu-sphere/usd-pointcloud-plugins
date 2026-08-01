#include "usdgeo/PointCloudLayer.h"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <cstdlib>
#include <filesystem>
#include <limits>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

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
        {"waveformDataExternal", usdpointcloud::PointAttributeType::UInt8}};
    const std::vector<usdgeo::Vec3d> positions = {
        {1000.0, 2000.0, 3000.0}, {1002.0, 2003.0, 3004.0}};
    usdgeo::PointCloudLayer::Data data;
    data.positions = positions;
    data.intensity = {42, 84};
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

    Check(usdgeo::AuthorPointCloudAsset(layer.get(), "/PointCloud", asset));
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    Check(pxr::UsdGeomGetStageUpAxis(stage) == pxr::TfToken("Y"));
    Check(stage->GetPrimAtPath(pxr::SdfPath("/PointCloud")).IsValid());

    usdgeo::PointCloudAuthorFailure failure;
    Check(!usdgeo::AuthorPointCloudAsset(
        nullptr, "/PointCloud", asset, failure));
    Check(failure == usdgeo::PointCloudAuthorFailure::InvalidLayer);
}

} // namespace

int main() {
    TestPointCloudRoundTrip();
    TestOptionalAttributesAreIndependent();
    TestInvalidPositionDoesNotMutateStage();
    TestLayerAuthoringEntryPoint();
    return 0;
}