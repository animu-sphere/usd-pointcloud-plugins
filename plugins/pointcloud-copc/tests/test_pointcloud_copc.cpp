#include "lazperf/io.hpp"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdLod/rootAPI.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void RegisterPlugin(const std::filesystem::path& plugInfo) {
    const auto plugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(
        plugInfo.string());
    Check(plugins.size() == 1);
    Check(plugins.front()->Load());
}

template <typename T>
void Write(std::vector<std::uint8_t>& bytes, std::size_t offset, T value) {
    std::array<std::uint8_t, sizeof(T)> encoded{};
    std::memcpy(encoded.data(), &value, sizeof(T));
    const std::uint16_t marker = 1;
    const bool nativeLittleEndian =
        *reinterpret_cast<const std::uint8_t*>(&marker) == 1;
    if (!nativeLittleEndian) {
        std::reverse(encoded.begin(), encoded.end());
    }
    std::copy(encoded.begin(), encoded.end(), bytes.begin() + offset);
}

std::vector<std::uint8_t> MakeMetadataFixture() {
    constexpr std::size_t headerSize = 375;
    constexpr std::size_t infoVlrOffset = headerSize;
    constexpr std::size_t hierarchyVlrOffset = infoVlrOffset + 54 + 160;
    constexpr std::size_t rootOffset = hierarchyVlrOffset + 54;
    constexpr std::size_t pointDataOffset = rootOffset + 32;
    constexpr std::size_t fileSize = pointDataOffset + 30;

    std::vector<std::uint8_t> bytes(fileSize, 0);
    std::memcpy(bytes.data(), "LASF", 4);
    Write(bytes, 24, std::uint8_t{1});
    Write(bytes, 25, std::uint8_t{4});
    Write(bytes, 94, static_cast<std::uint16_t>(headerSize));
    Write(bytes, 96, static_cast<std::uint32_t>(rootOffset));
    Write(bytes, 100, std::uint32_t{2});
    Write(bytes, 104, std::uint8_t{6});
    Write(bytes, 105, std::uint16_t{30});
    Write(bytes, 107, std::uint32_t{1});
    Write(bytes, 247, std::uint64_t{1});
    Write(bytes, 131, 0.01);
    Write(bytes, 139, 0.01);
    Write(bytes, 147, 0.01);
    Write(bytes, 155, 1000.0);
    Write(bytes, 163, 2000.0);
    Write(bytes, 171, 3000.0);
    Write(bytes, 179, 1001.0);
    Write(bytes, 187, 1000.0);
    Write(bytes, 195, 2001.0);
    Write(bytes, 203, 2000.0);
    Write(bytes, 211, 3001.0);
    Write(bytes, 219, 3000.0);

    Write(bytes, infoVlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + infoVlrOffset + 2, "copc", 4);
    Write(bytes, infoVlrOffset + 18, std::uint16_t{1});
    Write(bytes, infoVlrOffset + 20, std::uint16_t{160});
    std::memcpy(bytes.data() + infoVlrOffset + 22, "COPC info", 9);

    Write(bytes, hierarchyVlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 2, "copc", 4);
    Write(bytes, hierarchyVlrOffset + 18, std::uint16_t{1000});
    Write(bytes, hierarchyVlrOffset + 20, std::uint16_t{0});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 22, "hierarchy", 9);

    const auto infoOffset = infoVlrOffset + 54;
    Write(bytes, infoOffset, 1000.5);
    Write(bytes, infoOffset + 8, 2000.5);
    Write(bytes, infoOffset + 16, 3000.5);
    Write(bytes, infoOffset + 24, 2.0);
    Write(bytes, infoOffset + 32, 0.5);
    Write(bytes, infoOffset + 40, static_cast<std::uint64_t>(rootOffset));
    Write(bytes, infoOffset + 48, std::uint64_t{32});
    Write(bytes, infoOffset + 56, -10.0);
    Write(bytes, infoOffset + 64, 10.0);
    Write(bytes, rootOffset + 16, static_cast<std::uint64_t>(pointDataOffset));
    Write(bytes, rootOffset + 24, std::int32_t{30});
    Write(bytes, rootOffset + 28, std::int32_t{1});
    return bytes;
}

std::vector<std::vector<std::uint8_t>> MakeEquivalentRecords() {
    std::vector<std::vector<std::uint8_t>> records(
        3, std::vector<std::uint8_t>(30, 0));
    for (std::size_t index = 0; index < records.size(); ++index) {
        auto& record = records[index];
        Write(record, 0, static_cast<std::int32_t>(index * 100));
        Write(record, 4, static_cast<std::int32_t>(index * 100));
        Write(record, 8, static_cast<std::int32_t>(index * 100));
        Write(record, 12, static_cast<std::uint16_t>(10 + index));
        Write(record, 14, std::uint8_t{0x21});
        Write(record, 15, static_cast<std::uint8_t>(2 + index));
        Write(record, 16, static_cast<std::uint8_t>(7 + index));
        Write(record, 17, static_cast<std::uint8_t>(20 + index));
        Write(record, 18, static_cast<std::int16_t>(-4 + index));
        Write(record, 20, static_cast<std::uint16_t>(42 + index));
        Write(record, 22, 12.5 + index);
    }
    return records;
}

void SetEquivalentHeader(std::vector<std::uint8_t>& bytes,
                         std::uint32_t pointDataOffset,
                         std::uint32_t vlrCount) {
    std::memcpy(bytes.data(), "LASF", 4);
    Write(bytes, 24, std::uint8_t{1});
    Write(bytes, 25, std::uint8_t{4});
    Write(bytes, 94, std::uint16_t{375});
    Write(bytes, 96, pointDataOffset);
    Write(bytes, 100, vlrCount);
    Write(bytes, 104, std::uint8_t{6});
    Write(bytes, 105, std::uint16_t{30});
    Write(bytes, 107, std::uint32_t{3});
    Write(bytes, 131, 0.01);
    Write(bytes, 139, 0.01);
    Write(bytes, 147, 0.01);
    Write(bytes, 155, 1000.0);
    Write(bytes, 163, 2000.0);
    Write(bytes, 171, 3000.0);
    Write(bytes, 179, 1002.0);
    Write(bytes, 187, 1000.0);
    Write(bytes, 195, 2002.0);
    Write(bytes, 203, 2000.0);
    Write(bytes, 211, 3002.0);
    Write(bytes, 219, 3000.0);
    Write(bytes, 247, std::uint64_t{3});
}

std::filesystem::path WriteFixture(
    const std::vector<std::uint8_t>& bytes,
    const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Check(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    Check(output.good());
    return path;
}

std::filesystem::path WriteEquivalentLas(
    const std::vector<std::vector<std::uint8_t>>& records) {
    std::vector<std::uint8_t> bytes(375 + records.size() * 30, 0);
    SetEquivalentHeader(bytes, 375, 0);
    for (std::size_t index = 0; index < records.size(); ++index) {
        std::copy(records[index].begin(), records[index].end(),
                  bytes.begin() + 375 + index * 30);
    }
    return WriteFixture(bytes, "usd_copc_lod_equivalent.las");
}

template <typename T>
T ReadValue(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::vector<std::uint8_t> WriteEquivalentLaz(
    const std::vector<std::vector<std::uint8_t>>& records,
    const std::filesystem::path& path) {
    lazperf::writer::named_file::config config;
    config.scale = {0.01, 0.01, 0.01};
    config.offset = {1000.0, 2000.0, 3000.0};
    config.chunk_size = static_cast<unsigned int>(records.size());
    config.pdrf = 6;
    config.minor_version = 4;
    lazperf::writer::named_file writer(path.string(), config);
    for (const auto& record : records) {
        writer.writePoint(reinterpret_cast<const char*>(record.data()));
    }
    writer.close();

    std::ifstream input(path, std::ios::binary);
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    Check(size > 0);
    input.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    Check(static_cast<bool>(input));

    const auto pointDataOffset = ReadValue<std::uint32_t>(bytes, 96);
    const auto chunkTableOffset = ReadValue<std::int64_t>(
        bytes, pointDataOffset);
    Check(chunkTableOffset > static_cast<std::int64_t>(pointDataOffset + 8));
    const auto headerSize = ReadValue<std::uint16_t>(bytes, 94);
    Write(bytes, static_cast<std::size_t>(headerSize) + 54,
          std::uint16_t{2});
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    Check(output.good());
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    Check(output.good());
    return std::vector<std::uint8_t>(
        bytes.begin() + pointDataOffset + 8,
        bytes.begin() + static_cast<std::size_t>(chunkTableOffset));
}

void WriteHierarchyEntry(std::vector<std::uint8_t>& bytes,
                         std::size_t offset,
                         std::int32_t level,
                         std::int32_t x,
                         std::int32_t y,
                         std::int32_t z,
                         std::int32_t pointCount,
                         std::uint64_t dataOffset,
                         std::int32_t byteSize) {
    Write(bytes, offset, level);
    Write(bytes, offset + 4, x);
    Write(bytes, offset + 8, y);
    Write(bytes, offset + 12, z);
    Write(bytes, offset + 16, dataOffset);
    Write(bytes, offset + 24, byteSize);
    Write(bytes, offset + 28, pointCount);
}

std::filesystem::path WriteEquivalentCopc(
    const std::vector<std::uint8_t>& compressedPoints) {
    constexpr std::size_t headerSize = 375;
    constexpr std::size_t infoVlrOffset = headerSize;
    constexpr std::size_t hierarchyVlrOffset = infoVlrOffset + 54 + 160;
    constexpr std::size_t rootOffset = hierarchyVlrOffset + 54;
    constexpr std::size_t pointDataOffset = rootOffset + 32;
    std::vector<std::uint8_t> bytes(pointDataOffset + compressedPoints.size(),
                                    0);
    SetEquivalentHeader(bytes, static_cast<std::uint32_t>(pointDataOffset), 2);

    Write(bytes, infoVlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + infoVlrOffset + 2, "copc", 4);
    Write(bytes, infoVlrOffset + 18, std::uint16_t{1});
    Write(bytes, infoVlrOffset + 20, std::uint16_t{160});
    std::memcpy(bytes.data() + infoVlrOffset + 22, "COPC info", 9);
    Write(bytes, infoVlrOffset + 54, 1001.0);
    Write(bytes, infoVlrOffset + 62, 2001.0);
    Write(bytes, infoVlrOffset + 70, 3001.0);
    Write(bytes, infoVlrOffset + 78, 10.0);
    Write(bytes, infoVlrOffset + 86, 0.5);
    Write(bytes, infoVlrOffset + 94, static_cast<std::uint64_t>(rootOffset));
    Write(bytes, infoVlrOffset + 102, std::uint64_t{32});
    Write(bytes, infoVlrOffset + 110, 12.5);
    Write(bytes, infoVlrOffset + 118, 14.5);

    Write(bytes, hierarchyVlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 2, "copc", 4);
    Write(bytes, hierarchyVlrOffset + 18, std::uint16_t{1000});
    Write(bytes, hierarchyVlrOffset + 20, std::uint16_t{32});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 22, "hierarchy", 9);
    WriteHierarchyEntry(bytes, rootOffset, 0, 0, 0, 0, 3,
                        static_cast<std::uint64_t>(pointDataOffset),
                        static_cast<std::int32_t>(compressedPoints.size()));
    std::copy(compressedPoints.begin(), compressedPoints.end(),
              bytes.begin() + pointDataOffset);
    return WriteFixture(bytes, "usd_copc_lod_equivalent.copc");
}

struct LodSnapshot {
    std::vector<pxr::VtVec3fArray> positions;
    std::vector<pxr::VtIntArray> intensity;
    std::vector<pxr::VtArray<unsigned char>> classification;
    std::vector<pxr::VtArray<double>> gpsTime;
    std::vector<pxr::GfVec3d> boundsMin;
    std::vector<pxr::GfVec3d> boundsMax;
    std::vector<std::uint64_t> pointCounts;
    pxr::VtArray<float> thresholds;
    int defaultIndex = -1;
};

LodSnapshot ReadLodSnapshot(const pxr::UsdStageRefPtr& stage) {
    const auto root = stage->GetPrimAtPath(pxr::SdfPath("/PointCloud"));
    Check(root.HasAPI<pxr::UsdLodRootAPI>());
    LodSnapshot snapshot;
    Check(root.GetAttribute(pxr::TfToken("lod:default:index"))
              .Get(&snapshot.defaultIndex));
    const auto heuristic = stage->GetPrimAtPath(pxr::SdfPath(
        "/LodHeuristics/PointCloudScreenSize"));
    Check(heuristic.IsValid());
    Check(heuristic.GetAttribute(pxr::TfToken("thresholds"))
              .Get(&snapshot.thresholds));
    for (int index = 0; index < 4; ++index) {
        const auto path = pxr::SdfPath(
            "/PointCloud/LOD" + std::to_string(index));
        const auto points = pxr::UsdGeomPoints::Get(stage, path);
        if (!points.GetPrim().IsValid()) break;
        pxr::VtVec3fArray positions;
        pxr::VtIntArray intensity;
        pxr::VtArray<unsigned char> classification;
        pxr::VtArray<double> gpsTime;
        pxr::GfVec3d boundsMin;
        pxr::GfVec3d boundsMax;
        std::uint64_t pointCount = 0;
        Check(points.GetPointsAttr().Get(&positions));
        Check(points.GetPrim()
                  .GetAttribute(pxr::TfToken("geo:intensity"))
                  .Get(&intensity));
        Check(points.GetPrim()
                  .GetAttribute(pxr::TfToken("geo:classification"))
                  .Get(&classification));
        Check(points.GetPrim()
                  .GetAttribute(pxr::TfToken("geo:gpsTime"))
                  .Get(&gpsTime));
        Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:boundsMin"))
              .Get(&boundsMin));
        Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:boundsMax"))
              .Get(&boundsMax));
        Check(points.GetPrim()
              .GetAttribute(pxr::TfToken("geo:pointCount"))
              .Get(&pointCount));
        snapshot.positions.push_back(std::move(positions));
        snapshot.intensity.push_back(std::move(intensity));
        snapshot.classification.push_back(std::move(classification));
        snapshot.gpsTime.push_back(std::move(gpsTime));
        snapshot.boundsMin.push_back(boundsMin);
        snapshot.boundsMax.push_back(boundsMax);
        snapshot.pointCounts.push_back(pointCount);
    }
    return snapshot;
}

void CheckEquivalentLodSnapshots(const LodSnapshot& expected,
                                 const LodSnapshot& actual) {
    Check(expected.defaultIndex == actual.defaultIndex);
    Check(expected.thresholds == actual.thresholds);
    Check(expected.positions == actual.positions);
    Check(expected.intensity == actual.intensity);
    Check(expected.classification == actual.classification);
    Check(expected.gpsTime == actual.gpsTime);
    Check(expected.boundsMin == actual.boundsMin);
    Check(expected.boundsMax == actual.boundsMax);
    Check(expected.pointCounts == actual.pointCounts);
}

void TestMetadataIntegration() {
    const auto format = pxr::SdfFileFormat::FindByExtension("sample.copc");
    Check(format);
    Check(dynamic_cast<const pxr::PcpDynamicFileFormatInterface*>(
              format.operator->()));

    const auto path = std::filesystem::temp_directory_path() /
                      "usd_pointcloud_plugins_metadata.copc";
    {
        const auto bytes = MakeMetadataFixture();
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        Check(output.good());
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        Check(output.good());
    }

    const auto layer = pxr::SdfLayer::CreateAnonymous("metadata.usda");
    Check(layer);
    Check(format->Read(layer.operator->(), path.string(), true));
    Check(layer->GetPrimAtPath(pxr::SdfPath("/PointCloud")));
    Check(layer->GetAttributeAtPath(
        pxr::SdfPath("/PointCloud.geo:pointFormat")));
    std::filesystem::remove(path);
}

void TestAuthoredLodEquivalence() {
    const auto records = MakeEquivalentRecords();
    const auto lasPath = WriteEquivalentLas(records);
    const auto lazPath = std::filesystem::temp_directory_path() /
                         "usd_copc_lod_equivalent.laz";
    const auto compressedPoints = WriteEquivalentLaz(records, lazPath);
    const auto copcPath = WriteEquivalentCopc(compressedPoints);
    const pxr::SdfLayer::FileFormatArguments arguments = {
        {"lod", "balanced"}};

    const auto lasLayer = pxr::SdfLayer::FindOrOpen(
        lasPath.string(), arguments);
    const auto lazLayer = pxr::SdfLayer::FindOrOpen(
        lazPath.string(), arguments);
    const auto copcLayer = pxr::SdfLayer::FindOrOpen(
        copcPath.string(), arguments);
    Check(lasLayer && lazLayer && copcLayer);

    const auto lasStage = pxr::UsdStage::Open(lasLayer);
    const auto lazStage = pxr::UsdStage::Open(lazLayer);
    const auto copcStage = pxr::UsdStage::Open(copcLayer);
    Check(lasStage && lazStage && copcStage);

    const auto lasSnapshot = ReadLodSnapshot(lasStage);
    const auto lazSnapshot = ReadLodSnapshot(lazStage);
    const auto copcSnapshot = ReadLodSnapshot(copcStage);
    Check(lasSnapshot.positions.size() == 3);
    CheckEquivalentLodSnapshots(lasSnapshot, lazSnapshot);
    CheckEquivalentLodSnapshots(lasSnapshot, copcSnapshot);

    const auto dynamicPath = std::filesystem::temp_directory_path() /
                             "usd_pointcloud_plugins_dynamic_copc_lod.usda";
    {
        std::ofstream output(dynamicPath);
        Check(output.good());
        output << "#usda 1.0\n"
               << "def \"Survey\" (\n"
               << "    prepend payload = @" << copcPath.generic_string()
               << "@</PointCloud>\n"
               << "    pc_copc_lod = \"balanced\"\n"
               << ")\n"
               << "{}\n";
        Check(output.good());
    }
    const auto dynamicStage = pxr::UsdStage::Open(dynamicPath.string());
    Check(dynamicStage);
    const auto survey = dynamicStage->GetPrimAtPath(
        pxr::SdfPath("/Survey"));
    Check(survey.IsValid());
    Check(survey.HasAPI<pxr::UsdLodRootAPI>());
    std::filesystem::remove(dynamicPath);

    std::error_code error;
    std::filesystem::remove(lasPath, error);
    std::filesystem::remove(lazPath, error);
    std::filesystem::remove(copcPath, error);
}

} // namespace

int main() {
    RegisterPlugin(std::filesystem::path(USDGEOLAS_SOURCE_DIR) /
                   "plugin" / "resources" / "pointcloud-las" /
                   "plugInfo.json");
    RegisterPlugin(std::filesystem::path(USDGEOLAZ_SOURCE_DIR) /
                   "plugin" / "resources" / "pointcloud-laz" /
                   "plugInfo.json");
    RegisterPlugin(std::filesystem::path(USDGEOCOPC_SOURCE_DIR) /
                   "plugin" / "resources" / "pointcloud-copc" /
                   "plugInfo.json");
    TestMetadataIntegration();
    TestAuthoredLodEquivalence();
    return 0;
}
