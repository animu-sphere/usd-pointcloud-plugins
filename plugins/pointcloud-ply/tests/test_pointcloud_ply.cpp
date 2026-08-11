#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/points.h>

#include <cstdlib>
#include <filesystem>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestCorpusAsset(const std::filesystem::path& relativePath,
                     std::size_t expectedPointCount) {
    const auto source = std::filesystem::path(USDGEOPLY_SOURCE_DIR) /
                        "tests" / "corpus" / relativePath;
    const pxr::SdfLayer::FileFormatArguments arguments = {{"epsg", "4978"}};
    const auto layer = pxr::SdfLayer::FindOrOpen(source.string(), arguments);
    Check(layer);
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath("/PointCloud"));
    Check(points.GetPrim().IsValid());

    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.size() == expectedPointCount);
}

void TestFileFormatIntegration() {
    const auto plugInfo = std::filesystem::path(USDGEOPLY_SOURCE_DIR) /
                          "plugin" / "resources" / "pointcloud-ply" /
                          "plugInfo.json";
    const auto plugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(
        plugInfo.string());
    Check(plugins.size() == 1);
    Check(plugins.front()->Load());
    const auto format = pxr::SdfFileFormat::FindByExtension("sample.ply");
    Check(format);
    Check(dynamic_cast<const pxr::PcpDynamicFileFormatInterface*>(
        format.operator->()));

    const auto source = std::filesystem::path(USDGEOPLY_SOURCE_DIR) /
                        "tests" / "fixtures" / "conformance.ply";
    const pxr::SdfLayer::FileFormatArguments arguments = {{"epsg", "4978"}};
    const auto layer = pxr::SdfLayer::FindOrOpen(source.string(), arguments);
    Check(layer);
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath("/PointCloud"));
    Check(points.GetPrim().IsValid());

    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.size() == 2);
    Check(positions[0] == pxr::GfVec3f(1.0f, 2.0f, 3.0f));
    Check(points.GetPrim().GetAttribute(pxr::TfToken("geo:red")).IsValid());

    TestCorpusAsset(
        std::filesystem::path("stanford-bunny") /
            "stanford-bunny-thinned-4096.ply",
        4096);
    TestCorpusAsset(
        std::filesystem::path("open3d-fragment") /
            "open3d-fragment-thinned-8192.ply",
        8192);
}

} // namespace

int main() {
    TestFileFormatIntegration();
    return 0;
}