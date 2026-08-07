#include "usdcopc/Copc.h"
#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"

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
                         std::int32_t byteSize) {
    Write(bytes, offset, level);
    Write(bytes, offset + 4, x);
    Write(bytes, offset + 8, y);
    Write(bytes, offset + 12, z);
    Write(bytes, offset + 16, dataOffset);
    Write(bytes, offset + 24, byteSize);
    Write(bytes, offset + 28, pointCount);
}

std::vector<std::uint8_t> MakeFixture(std::uint8_t pointFormat = 6) {
    constexpr std::size_t headerSize = 375;
    constexpr std::size_t vlrOffset = headerSize;
    constexpr std::size_t hierarchyVlrOffset = vlrOffset + 54 + 160;
    constexpr std::size_t rootOffset = hierarchyVlrOffset + 54;
    constexpr std::size_t childOffset = rootOffset + 64;
    constexpr std::size_t pointDataOffset = rootOffset + 96;
    constexpr std::size_t fileSize = pointDataOffset + 60;

    std::vector<std::uint8_t> bytes(fileSize, 0);
    std::memcpy(bytes.data(), "LASF", 4);
    Write(bytes, 24, std::uint8_t{1});
    Write(bytes, 25, std::uint8_t{4});
    Write(bytes, 94, std::uint16_t{375});
    Write(bytes, 96, static_cast<std::uint32_t>(pointDataOffset));
        Write(bytes, 100, std::uint32_t{2});
        Write(bytes, 104, pointFormat);
        Write(bytes, 105,
            static_cast<std::uint16_t>(pointFormat == 9 ? 59 : 30));
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

    Write(bytes, hierarchyVlrOffset, std::uint16_t{0});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 2, "copc", 4);
    Write(bytes, hierarchyVlrOffset + 18, std::uint16_t{1000});
    Write(bytes, hierarchyVlrOffset + 20, std::uint16_t{96});
    std::memcpy(bytes.data() + hierarchyVlrOffset + 22, "hierarchy", 9);

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

template <typename T>
T Read(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

std::vector<std::vector<std::uint8_t>> MakeEquivalentRecords() {
    std::vector<std::vector<std::uint8_t>> records(3,
                                                    std::vector<std::uint8_t>(30));
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

std::filesystem::path WriteEquivalentLas(
    const std::vector<std::vector<std::uint8_t>>& records) {
    std::vector<std::uint8_t> bytes(375 + records.size() * 30, 0);
    SetEquivalentHeader(bytes, 375, 0);
    for (std::size_t index = 0; index < records.size(); ++index) {
        std::copy(records[index].begin(), records[index].end(),
                  bytes.begin() + 375 + index * 30);
    }
    return WriteFixture(bytes, "usd_copc_equivalent.las");
}

std::vector<std::uint8_t> WriteEquivalentLaz(
    const std::vector<std::vector<std::uint8_t>>& records,
    const std::filesystem::path& path,
    int pointFormat,
    int minorVersion) {
    lazperf::writer::named_file::config config;
    config.scale = {0.01, 0.01, 0.01};
    config.offset = {1000.0, 2000.0, 3000.0};
    config.chunk_size = static_cast<unsigned int>(records.size());
    config.pdrf = pointFormat;
    config.minor_version = minorVersion;
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

    const auto pointDataOffset = Read<std::uint32_t>(bytes, 96);
    const auto chunkTableOffset = Read<std::int64_t>(bytes, pointDataOffset);
    Check(chunkTableOffset > static_cast<std::int64_t>(pointDataOffset + 8));
    if (pointFormat == 6) {
        const auto headerSize = Read<std::uint16_t>(bytes, 94);
        Write(bytes, static_cast<std::size_t>(headerSize) + 54,
              std::uint16_t{2});
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        Check(static_cast<bool>(output));
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        Check(static_cast<bool>(output));
    }
    return std::vector<std::uint8_t>(
        bytes.begin() + pointDataOffset + 8,
        bytes.begin() + static_cast<std::size_t>(chunkTableOffset));
}

std::filesystem::path WriteEquivalentCopc(
    const std::vector<std::uint8_t>& compressedPoints) {
    constexpr std::size_t headerSize = 375;
    constexpr std::size_t infoVlrOffset = headerSize;
    constexpr std::size_t hierarchyVlrOffset = infoVlrOffset + 54 + 160;
    constexpr std::size_t rootOffset = hierarchyVlrOffset + 54;
    constexpr std::size_t pointDataOffset = rootOffset + 32;
    std::vector<std::uint8_t> bytes(pointDataOffset + compressedPoints.size(), 0);
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
    return WriteFixture(bytes, "usd_copc_equivalent.copc");
}

struct StreamResult {
    usdpointcloud::PointChunk chunk;
    usdpointcloud::PointData data;
};

StreamResult ReadEquivalentStream(usdpointcloud::PointStream& stream) {
    StreamResult result;
    usdgeo::Diagnostic diagnostic;
    Check(stream.ReadNext(result.chunk, result.data, diagnostic) ==
          usdpointcloud::PointStreamStatus::Chunk);
        Check(diagnostic.message.empty());
    Check(result.chunk.IsValid() && result.data.IsValid());
    usdpointcloud::PointChunk endChunk;
    usdpointcloud::PointData endData;
    Check(stream.ReadNext(endChunk, endData, diagnostic) ==
          usdpointcloud::PointStreamStatus::End);
    Check(diagnostic.message.empty());
    return result;
}

void CheckEquivalentData(const usdpointcloud::PointData& expected,
                         const usdpointcloud::PointData& actual) {
    Check(expected.positions.size() == actual.positions.size());
    for (std::size_t index = 0; index < expected.positions.size(); ++index) {
        Check(expected.positions[index].x == actual.positions[index].x);
        Check(expected.positions[index].y == actual.positions[index].y);
        Check(expected.positions[index].z == actual.positions[index].z);
    }
    Check(expected.intensity == actual.intensity);
    Check(expected.returnNumber == actual.returnNumber);
    Check(expected.numberOfReturns == actual.numberOfReturns);
    Check(expected.classification == actual.classification);
    Check(expected.classificationFlags == actual.classificationFlags);
    Check(expected.scannerChannel == actual.scannerChannel);
    Check(expected.scanDirectionFlag == actual.scanDirectionFlag);
    Check(expected.edgeOfFlightLine == actual.edgeOfFlightLine);
    Check(expected.userData == actual.userData);
    Check(expected.scanAngle == actual.scanAngle);
    Check(expected.pointSourceId == actual.pointSourceId);
}

void TestLasLazCopcPointStreamEquivalence() {
    const auto records = MakeEquivalentRecords();
    const auto lasPath = WriteEquivalentLas(records);
    const auto lazPath = std::filesystem::temp_directory_path() /
                         "usd_copc_equivalent.laz";
    const auto compressedPoints = WriteEquivalentLaz(records, lazPath, 6, 4);
    const auto copcPath = WriteEquivalentCopc(compressedPoints);

    usdpointcloud::PointReadOptions options;
    options.chunkPointLimit = 16;
    options.memoryBudgetBytes = 1024 * 1024;
    usdlas::LasHeader lasHeader;
    std::vector<usdgeo::Diagnostic> lasDiagnostics;
    auto lasStream = usdlas::OpenLasPointStream(
        lasPath.string(), options, lasHeader, lasDiagnostics);
    Check(lasStream && lasDiagnostics.empty());
    const auto lasResult = ReadEquivalentStream(*lasStream);

    usdlas::LasHeader lazHeader;
    std::vector<usdgeo::Diagnostic> lazDiagnostics;
    auto lazStream = usdlaz::OpenLazPointStream(
        lazPath.string(), options, lazHeader, lazDiagnostics);
    Check(lazStream && lazDiagnostics.empty());
    const auto lazResult = ReadEquivalentStream(*lazStream);

    usdcopc::CopcHeader copcHeader;
    std::vector<usdgeo::Diagnostic> copcDiagnostics;
    auto copcStream = usdcopc::OpenCopcPointStream(
        copcPath.string(), options, copcHeader, copcDiagnostics);
    Check(copcStream && copcDiagnostics.empty());
    const auto copcResult = ReadEquivalentStream(*copcStream);

    Check(lasHeader.pointCount == lazHeader.pointCount &&
          lasHeader.pointCount == copcHeader.las.pointCount);
        Check(lasHeader.pointFormat == 6 && lazHeader.pointFormat == 6 &&
            copcHeader.las.pointFormat == 6);
    Check(lasHeader.xScale == lazHeader.xScale &&
          lasHeader.xScale == copcHeader.las.xScale);
    Check(lasHeader.yScale == lazHeader.yScale &&
          lasHeader.yScale == copcHeader.las.yScale);
    Check(lasHeader.zScale == lazHeader.zScale &&
          lasHeader.zScale == copcHeader.las.zScale);
        Check(lasHeader.xOffset == lazHeader.xOffset &&
            lasHeader.xOffset == copcHeader.las.xOffset);
        Check(lasHeader.yOffset == lazHeader.yOffset &&
            lasHeader.yOffset == copcHeader.las.yOffset);
        Check(lasHeader.zOffset == lazHeader.zOffset &&
            lasHeader.zOffset == copcHeader.las.zOffset);
        Check(lasHeader.bounds.minimum.x == lazHeader.bounds.minimum.x &&
            lasHeader.bounds.minimum.x == copcHeader.las.bounds.minimum.x &&
            lasHeader.bounds.minimum.y == lazHeader.bounds.minimum.y &&
            lasHeader.bounds.minimum.y == copcHeader.las.bounds.minimum.y &&
            lasHeader.bounds.minimum.z == lazHeader.bounds.minimum.z &&
            lasHeader.bounds.minimum.z == copcHeader.las.bounds.minimum.z &&
            lasHeader.bounds.maximum.x == lazHeader.bounds.maximum.x &&
            lasHeader.bounds.maximum.x == copcHeader.las.bounds.maximum.x &&
            lasHeader.bounds.maximum.y == lazHeader.bounds.maximum.y &&
            lasHeader.bounds.maximum.y == copcHeader.las.bounds.maximum.y &&
            lasHeader.bounds.maximum.z == lazHeader.bounds.maximum.z &&
            lasHeader.bounds.maximum.z == copcHeader.las.bounds.maximum.z);
    CheckEquivalentData(lasResult.data, lazResult.data);
    CheckEquivalentData(lasResult.data, copcResult.data);
        Check(lasResult.data.gpsTime == lazResult.data.gpsTime &&
            lasResult.data.gpsTime == copcResult.data.gpsTime);
    Check(lasResult.chunk.pointCount == lazResult.chunk.pointCount &&
          lasResult.chunk.pointCount == copcResult.chunk.pointCount);
        Check(lasResult.chunk.bounds.minimum.x == lazResult.chunk.bounds.minimum.x &&
            lasResult.chunk.bounds.minimum.x == copcResult.chunk.bounds.minimum.x &&
            lasResult.chunk.bounds.minimum.y == lazResult.chunk.bounds.minimum.y &&
            lasResult.chunk.bounds.minimum.y == copcResult.chunk.bounds.minimum.y &&
            lasResult.chunk.bounds.minimum.z == lazResult.chunk.bounds.minimum.z &&
            lasResult.chunk.bounds.minimum.z == copcResult.chunk.bounds.minimum.z &&
            lasResult.chunk.bounds.maximum.x == lazResult.chunk.bounds.maximum.x &&
            lasResult.chunk.bounds.maximum.x == copcResult.chunk.bounds.maximum.x &&
            lasResult.chunk.bounds.maximum.y == lazResult.chunk.bounds.maximum.y &&
            lasResult.chunk.bounds.maximum.y == copcResult.chunk.bounds.maximum.y &&
            lasResult.chunk.bounds.maximum.z == lazResult.chunk.bounds.maximum.z &&
            lasResult.chunk.bounds.maximum.z == copcResult.chunk.bounds.maximum.z);

    usdpointcloud::PointReadOptions invalidOptions = options;
    invalidOptions.chunkPointLimit = 0;
    usdlas::LasHeader invalidLasHeader;
    usdlas::LasHeader invalidLazHeader;
    usdcopc::CopcHeader invalidCopcHeader;
    std::vector<usdgeo::Diagnostic> invalidLasDiagnostics;
    std::vector<usdgeo::Diagnostic> invalidLazDiagnostics;
    std::vector<usdgeo::Diagnostic> invalidCopcDiagnostics;
    Check(!usdlas::OpenLasPointStream(lasPath.string(), invalidOptions,
                                      invalidLasHeader,
                                      invalidLasDiagnostics));
    Check(!usdlaz::OpenLazPointStream(lazPath.string(), invalidOptions,
                                      invalidLazHeader,
                                      invalidLazDiagnostics));
    Check(!usdcopc::OpenCopcPointStream(copcPath.string(), invalidOptions,
                                        invalidCopcHeader,
                                        invalidCopcDiagnostics));
    Check(invalidLasDiagnostics.size() == 1 &&
          invalidLazDiagnostics.size() == 1 &&
          invalidCopcDiagnostics.size() == 1);
    Check(invalidLasDiagnostics.front().code ==
              invalidLazDiagnostics.front().code &&
          invalidLasDiagnostics.front().code ==
              invalidCopcDiagnostics.front().code);
    Check(invalidLasDiagnostics.front().severity ==
              invalidLazDiagnostics.front().severity &&
          invalidLasDiagnostics.front().severity ==
              invalidCopcDiagnostics.front().severity);

    copcStream.reset();
    lazStream.reset();
    lasStream.reset();

    std::filesystem::remove(lasPath);
    std::filesystem::remove(lazPath);
    std::filesystem::remove(copcPath);
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

        usdcopc::CopcHierarchy hierarchy;
        Check(reader.BuildHierarchy(header, entries, hierarchy, diagnostics));
        Check(diagnostics.empty() && hierarchy.IsValid() &&
            hierarchy.nodes.size() == 3);
        Check(hierarchy.nodes[0].tile.ToString() == "L0/0/0/0" &&
            hierarchy.nodes[0].bounds.minimum.x == 998.5 &&
            hierarchy.nodes[0].bounds.maximum.x == 1002.5 &&
            hierarchy.nodes[0].children.size() == 1);
        Check(hierarchy.nodes[2].tile.ToString() == "L2/2/0/0" &&
            hierarchy.nodes[2].spacing == 0.125 &&
            hierarchy.nodes[2].bounds.minimum.x == 1000.5 &&
            hierarchy.nodes[2].bounds.maximum.x == 1001.5);

          std::vector<usdcopc::CopcPointTile> tiles;
          Check(reader.BuildPointTiles(hierarchy, tiles, diagnostics));
          Check(diagnostics.empty() && tiles.size() == 2);
          Check(tiles[0].IsValid() && tiles[1].IsValid());
          Check(tiles[0].tile.id.ToString() == "L0/0/0/0" &&
              tiles[0].tile.lod.items[0].sourceRange.firstPoint == 0 &&
              tiles[0].tile.lod.items[0].sourceRange.pointCount == 1 &&
              tiles[0].tile.lod.items[0].spacing == 0.5 &&
              tiles[0].pointDataOffset == entries[0].offset &&
              tiles[0].pointDataSize ==
                  static_cast<std::uint64_t>(entries[0].byteSize) &&
              tiles[0].tile.children.size() == 1 &&
              tiles[0].tile.children[0].ToString() == "L2/2/0/0");
          Check(tiles[1].tile.id.ToString() == "L2/2/0/0" &&
              tiles[1].tile.lod.items[0].sourceRange.firstPoint == 0 &&
              tiles[1].tile.lod.items[0].sourceRange.pointCount == 1 &&
              tiles[1].tile.lod.items[0].spacing == 0.125 &&
              tiles[1].pointDataOffset == entries[2].offset &&
              tiles[1].pointDataSize ==
                  static_cast<std::uint64_t>(entries[2].byteSize));

          auto reorderedEntries = entries;
          std::swap(reorderedEntries[0], reorderedEntries[1]);
          usdcopc::CopcHierarchy reorderedHierarchy;
          Check(reader.BuildHierarchy(header, reorderedEntries,
                            reorderedHierarchy, diagnostics));
          Check(reorderedHierarchy.IsValid() &&
              reorderedHierarchy.nodes.size() == 3);

          auto entriesWithEmptyNode = entries;
          entriesWithEmptyNode.push_back({1, 0, 0, 0, 0, 0, 0});
          usdcopc::CopcHierarchy hierarchyWithEmptyNode;
          Check(reader.BuildHierarchy(header, entriesWithEmptyNode,
                            hierarchyWithEmptyNode, diagnostics));
          Check(hierarchyWithEmptyNode.IsValid() &&
              hierarchyWithEmptyNode.nodes.size() == 4 &&
              hierarchyWithEmptyNode.nodes.back().hasEmptyNode);

    std::vector<std::uint8_t> pointData;
    Check(reader.ReadPointData(header, entries[2], pointData, diagnostics));
    Check(diagnostics.empty() && pointData.size() == 30);

    std::vector<usdlas::LasPoint> points;
    Check(!reader.ReadPoints(header, entries[2], points, diagnostics));
    Check(points.empty() && !diagnostics.empty());

    std::filesystem::remove(path);
}

void TestCompressedPointFormatMetadata() {
    auto bytes = MakeFixture();
    bytes[104] = static_cast<std::uint8_t>(bytes[104] | 0x80);
    const auto path = WriteFixture(bytes, "usd_copc_compressed_metadata.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(reader.ReadMetadata(header, diagnostics));
    Check(diagnostics.empty() && header.las.pointFormat == 6);
    std::filesystem::remove(path);
}

void TestInvalidChildPage() {
    auto bytes = MakeFixture();
    constexpr std::size_t rootOffset = 643;
    Write(bytes, rootOffset + 32 + 24, std::int32_t{31});
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

void TestRejectsPointCountMismatch() {
    auto bytes = MakeFixture();
    Write(bytes, 247, std::uint64_t{3});
    const auto path = WriteFixture(bytes, "usd_copc_point_count_mismatch.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(reader.ReadMetadata(header, diagnostics));
    std::vector<usdcopc::CopcHierarchyEntry> entries;
    Check(!reader.ReadHierarchy(header, entries, diagnostics));
    Check(!diagnostics.empty());
    std::filesystem::remove(path);
}

void TestRejectsUnsupportedPointFormat() {
    const auto path = WriteFixture(
        MakeFixture(9), "usd_copc_unsupported_format.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.ReadMetadata(header, diagnostics));
    Check(!diagnostics.empty() &&
          diagnostics.front().code ==
              usdgeo::DiagnosticCode::UnsupportedPointFormat);
    std::filesystem::remove(path);
}

void TestRejectsMissingHierarchyVlr() {
    auto bytes = MakeFixture();
    Write(bytes, 100, std::uint32_t{1});
    const auto path = WriteFixture(
        bytes, "usd_copc_missing_hierarchy.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.ReadMetadata(header, diagnostics));
    Check(!diagnostics.empty());
    std::filesystem::remove(path);
}

void TestRejectsNonZeroReservedInfo() {
    auto bytes = MakeFixture();
    bytes[501] = 1;
    const auto path = WriteFixture(
        bytes, "usd_copc_invalid_reserved.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.ReadMetadata(header, diagnostics));
    Check(!diagnostics.empty());
    std::filesystem::remove(path);
}

void TestRejectsInvalidNodeCoordinates() {
    auto bytes = MakeFixture();
    constexpr std::size_t childCoordinateOffset = 711;
    Write(bytes, childCoordinateOffset, std::int32_t{-1});
    const auto path = WriteFixture(bytes, "usd_copc_invalid_node_test.copc");
    usdcopc::CopcReader reader(path.string());
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(reader.ReadMetadata(header, diagnostics));
    std::vector<usdcopc::CopcHierarchyEntry> entries;
    Check(reader.ReadHierarchy(header, entries, diagnostics));
    usdcopc::CopcHierarchy hierarchy;
    Check(!reader.BuildHierarchy(header, entries, hierarchy, diagnostics));
    Check(!diagnostics.empty());
    std::filesystem::remove(path);
}

void TestPointStreamRejectsSourceRange() {
    const auto path = WriteFixture(MakeFixture(), "usd_copc_stream_range.copc");
    usdpointcloud::PointReadOptions options;
    options.range = {1, 1};
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    const auto stream = usdcopc::OpenCopcPointStream(
        path.string(), options, header, diagnostics);
    Check(!stream);
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code ==
              usdgeo::DiagnosticCode::InvalidPointSourceRange);
    std::filesystem::remove(path);
}

void TestPointStreamChecksCancellation() {
    const auto path = WriteFixture(MakeFixture(), "usd_copc_stream_cancel.copc");
    usdpointcloud::PointReadOptions options;
    options.isCancelled = [] { return true; };
    usdcopc::CopcHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    auto stream = usdcopc::OpenCopcPointStream(
        path.string(), options, header, diagnostics);
    Check(stream && diagnostics.empty() && header.IsValid());

    usdpointcloud::PointChunk chunk;
    usdpointcloud::PointData data;
    usdgeo::Diagnostic diagnostic;
    Check(stream->ReadNext(chunk, data, diagnostic) ==
          usdpointcloud::PointStreamStatus::Error);
    Check(diagnostic.code == usdgeo::DiagnosticCode::DecodeFailure);
    Check(diagnostic.message == "COPC read cancelled");
    std::filesystem::remove(path);
}

} // namespace

int main() {
    TestMetadataAndHierarchy();
    TestCompressedPointFormatMetadata();
    TestInvalidChildPage();
    TestRejectsPointCountMismatch();
    TestRejectsUnsupportedPointFormat();
    TestRejectsMissingHierarchyVlr();
    TestRejectsNonZeroReservedInfo();
    TestRejectsInvalidNodeCoordinates();
    TestPointStreamRejectsSourceRange();
    TestPointStreamChecksCancellation();
    TestLasLazCopcPointStreamEquivalence();
    return 0;
}
