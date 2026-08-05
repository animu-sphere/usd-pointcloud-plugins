#include "usdcopc/Copc.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
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

void WriteHierarchyEntry(std::vector<std::uint8_t>& bytes,
                         std::size_t offset,
                         std::int32_t level,
                         std::int32_t x,
                         std::int32_t y,
                         std::int32_t z,
                         std::int32_t pointCount,
                         std::uint64_t dataOffset,
                         std::uint32_t byteSize) {
    Write(bytes, offset, level);
    Write(bytes, offset + 4, x);
    Write(bytes, offset + 8, y);
    Write(bytes, offset + 12, z);
    Write(bytes, offset + 16, pointCount);
    Write(bytes, offset + 20, dataOffset);
    Write(bytes, offset + 28, byteSize);
}

std::vector<std::uint8_t> MakeFixture() {
    constexpr std::size_t headerSize = 375;
    constexpr std::size_t vlrOffset = headerSize;
    constexpr std::size_t rootOffset = vlrOffset + 54 + 160;
    constexpr std::size_t childOffset = rootOffset + 64;
    constexpr std::size_t pointDataOffset = childOffset + 32;
    constexpr std::size_t fileSize = pointDataOffset + 60;

    std::vector<std::uint8_t> bytes(fileSize, 0);
    std::memcpy(bytes.data(), "LASF", 4);
    Write(bytes, 24, std::uint8_t{1});
    Write(bytes, 25, std::uint8_t{4});
    Write(bytes, 94, std::uint16_t{375});
    Write(bytes, 96, static_cast<std::uint32_t>(pointDataOffset));
    Write(bytes, 100, std::uint32_t{1});
    Write(bytes, 104, std::uint8_t{6});
    Write(bytes, 105, std::uint16_t{30});
    Write(bytes, 107, std::uint32_t{2});
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
    Write(bytes, 247, std::uint64_t{2});

    Write(bytes, vlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + vlrOffset + 2, "copc", 4);
    Write(bytes, vlrOffset + 18, std::uint16_t{1});
    Write(bytes, vlrOffset + 20, std::uint16_t{160});
    std::memcpy(bytes.data() + vlrOffset + 22, "COPC info", 9);

    const auto infoOffset = vlrOffset + 54;
    Write(bytes, infoOffset, 1000.5);
    Write(bytes, infoOffset + 8, 2000.5);
    Write(bytes, infoOffset + 16, 3000.5);
    Write(bytes, infoOffset + 24, 2.0);
    Write(bytes, infoOffset + 32, 0.5);
    Write(bytes, infoOffset + 40, static_cast<std::uint64_t>(rootOffset));
    Write(bytes, infoOffset + 48, std::uint64_t{64});
    Write(bytes, infoOffset + 56, -10.0);
    Write(bytes, infoOffset + 64, 10.0);

    WriteHierarchyEntry(bytes, rootOffset, 0, 0, 0, 0, 1,
                        pointDataOffset, 30);
    WriteHierarchyEntry(bytes, rootOffset + 32, 1, 1, 0, 0, -1,
                        childOffset, 32);
    WriteHierarchyEntry(bytes, childOffset, 2, 2, 0, 0, 1,
                        pointDataOffset + 30, 30);
    return bytes;
}

std::filesystem::path WriteFixture(const std::vector<std::uint8_t>& bytes,
                                   const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    Check(static_cast<bool>(file));
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    Check(static_cast<bool>(file));
    return path;
}

void TestMetadataAndHierarchy() {
    const auto path = WriteFixture(MakeFixture(), "usd_copc_test.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(reader.ReadMetadata(header, diagnostics));
    Check(diagnostics.empty());
    Check(header.IsValid());
    Check(header.info.rootHierarchySize == 64 &&
          header.info.rootHierarchyOffset > 0);

    std::vector<usdcopc::CopcHierarchyEntry> entries;
    Check(reader.ReadHierarchy(header, entries, diagnostics));
    Check(diagnostics.empty() && entries.size() == 3);
    Check(entries[0].IsPointData() && entries[0].pointCount == 1);
    Check(entries[1].IsHierarchyPage() && entries[1].byteSize == 32);
    Check(entries[2].IsPointData() && entries[2].level == 2);

    std::filesystem::remove(path);
}

void TestInvalidChildPage() {
    auto bytes = MakeFixture();
    constexpr std::size_t rootOffset = 589;
    Write(bytes, rootOffset + 32 + 28, std::uint32_t{31});
    const auto path = WriteFixture(bytes, "usd_copc_invalid_test.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(reader.ReadMetadata(header, diagnostics));
    std::vector<usdcopc::CopcHierarchyEntry> entries;
    Check(!reader.ReadHierarchy(header, entries, diagnostics));
    Check(!diagnostics.empty());
    std::filesystem::remove(path);
}

} // namespace

int main() {
    TestMetadataAndHierarchy();
    TestInvalidChildPage();
    return 0;
}
