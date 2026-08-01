#include "lazperf/io.hpp"
#include "lazperf/las.hpp"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/errorMark.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/points.h>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <string>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

std::filesystem::path MakeFixture() {
    const auto path = std::filesystem::temp_directory_path() /
                      "usd_geo_plugins_conformance.laz";
    lazperf::writer::named_file::config config;
    config.scale = {0.01, 0.01, 0.01};
    config.chunk_size = 2;
    config.minor_version = 2;
    lazperf::writer::named_file writer(path.string(), config);
    for (int index = 0; index < 3; ++index) {
        lazperf::las::point10 source;
        source.x = index * 100;
        source.y = index * 200;
        source.z = index * 300;
        source.intensity = static_cast<unsigned short>(index + 1);
        char record[20];
        source.pack(record);
        writer.writePoint(record);
    }
    writer.close();
    return path;
}

void TestFileFormatIntegration() {
    const auto plugInfo = std::filesystem::path(GEOLAZ_SOURCE_DIR) /
                          "plugin" / "resources" / "geo-laz" /
                          "plugInfo.json";
    const auto plugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(
        plugInfo.string());
    Check(plugins.size() == 1);
    Check(plugins.front()->Load());
    const auto format = pxr::SdfFileFormat::FindByExtension("sample.laz");
    Check(format);

    const auto path = MakeFixture();
    const auto layer = pxr::SdfLayer::FindOrOpen(path.string());
    Check(layer);
    Check(layer->GetPrimAtPath(pxr::SdfPath("/PointCloud")));
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath("/PointCloud"));
    Check(points.GetPrim().IsValid());

    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.size() == 3);
    Check(positions[1] == pxr::GfVec3f(1.0f, 3.0f, -2.0f));

    const auto intensityAttribute = layer->GetAttributeAtPath(
        pxr::SdfPath("/PointCloud.geo:intensity"));
    Check(intensityAttribute != nullptr);
    const auto intensityValue = intensityAttribute->GetDefaultValue();
    Check(intensityValue.IsHolding<pxr::VtIntArray>());
    const auto& intensity = intensityValue.UncheckedGet<pxr::VtIntArray>();
    Check(intensity.size() == 3 && intensity[0] == 1 && intensity[2] == 3);
    Check(layer->GetAttributeAtPath(
              pxr::SdfPath("/PointCloud.geo:classificationFlags")) == nullptr);
    Check(layer->GetAttributeAtPath(
              pxr::SdfPath("/PointCloud.geo:scannerChannel")) == nullptr);

    std::error_code error;
    std::filesystem::remove(path, error);
}

void TestCheckedInAsset(const std::filesystem::path& path,
                        std::size_t expectedPointCount) {
    const auto layer = pxr::SdfLayer::FindOrOpen(path.string());
    Check(layer);
    const auto stage = pxr::UsdStage::Open(layer);
    Check(stage);
    const auto points = pxr::UsdGeomPoints::Get(
        stage, pxr::SdfPath("/PointCloud"));
    Check(points.GetPrim().IsValid());

    pxr::VtVec3fArray positions;
    Check(points.GetPointsAttr().Get(&positions));
    Check(positions.size() == expectedPointCount);

    const auto intensityAttribute = layer->GetAttributeAtPath(
        pxr::SdfPath("/PointCloud.geo:intensity"));
    Check(intensityAttribute != nullptr);
    const auto intensity = intensityAttribute->GetDefaultValue();
    Check(intensity.IsHolding<pxr::VtIntArray>());
    Check(intensity.UncheckedGet<pxr::VtIntArray>().size() ==
          expectedPointCount);
}

void TestCheckedInAssets() {
    const auto root = std::filesystem::path(GEOLAZ_SOURCE_DIR) / "tests";
    TestCheckedInAsset(root / "fixtures" / "conformance.laz", 3);
    TestCheckedInAsset(
        root / "corpus" / "virtual-shizuoka-2019" /
            "virtual-shizuoka-08NF2330-thinned-4096.laz",
        4096);
    TestCheckedInAsset(
        root / "corpus" / "usgs-3dep-2020" /
            "usgs-3dep-2020-thinned-4096.laz",
        4096);
}

void TestMissingFileDiagnostic() {
    pxr::TfErrorMark mark;
    const auto uniqueSuffix =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("usd_geo_plugins_missing_" +
                       std::to_string(uniqueSuffix) + ".laz");
    const auto format = pxr::SdfFileFormat::FindByExtension("sample.laz");
    Check(format);
    const auto layer = pxr::SdfLayer::CreateAnonymous("missing.laz");
    Check(layer);
    Check(!format->Read(layer.operator->(), path.string(), false));
    Check(!mark.IsClean());
    Check(mark.GetBegin()->GetCommentary().find("[LAZ002]") !=
          std::string::npos);
    mark.Clear();
}

} // namespace

int main() {
    TestFileFormatIntegration();
    TestMissingFileDiagnostic();
    TestCheckedInAssets();
    return 0;
}
