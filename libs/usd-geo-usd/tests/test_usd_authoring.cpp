#include "usdgeo/PointCloudLayer.h"

#include <pxr/usd/usdGeom/points.h>

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
        {"nir", usdpointcloud::PointAttributeType::UInt16}};
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

    Check(usdgeo::PointCloudLayer::AuthorPointCloud(
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

    int epsgCode = 0;
    Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:epsgCode")).Get(&epsgCode));
    Check(epsgCode == 26910);
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
    Check(!usdgeo::PointCloudLayer::AuthorPointCloud(
        stage, "/InvalidPointCloud", reference, chunk.bounds, chunk,
        positions));
    Check(!stage->GetPrimAtPath(pxr::SdfPath("/InvalidPointCloud")).IsValid());
}

} // namespace

int main() {
    TestPointCloudRoundTrip();
    TestInvalidPositionDoesNotMutateStage();
    return 0;
}