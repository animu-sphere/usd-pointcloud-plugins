#include "usdgeo/PointCloudLayer.h"

#include <pxr/usd/usdGeom/points.h>

#include <cstdlib>
#include <filesystem>

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
    chunk.attributes = {{"classification",
                         usdpointcloud::PointAttributeType::UInt8}};
    const std::vector<usdgeo::Vec3d> positions = {
        {1000.0, 2000.0, 3000.0}, {1002.0, 2003.0, 3004.0}};

    Check(usdgeo::PointCloudLayer::AuthorPointCloud(
        stage, "/PointCloud", reference, chunk.bounds, chunk, positions));

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

    int epsgCode = 0;
    Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:epsgCode")).Get(&epsgCode));
    Check(epsgCode == 26910);
}

} // namespace

int main() {
    TestPointCloudRoundTrip();
    return 0;
}