#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>

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

void TestMetadataIntegration() {
    const auto plugInfo = std::filesystem::path(USDGEOCOPC_SOURCE_DIR) /
                          "plugin" / "resources" / "pointcloud-copc" /
                          "plugInfo.json";
    const auto plugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(
        plugInfo.string());
    Check(plugins.size() == 1);
    Check(plugins.front()->Load());
    const auto format = pxr::SdfFileFormat::FindByExtension("sample.copc");
    Check(format);

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

} // namespace

int main() {
    TestMetadataIntegration();
    return 0;
}
