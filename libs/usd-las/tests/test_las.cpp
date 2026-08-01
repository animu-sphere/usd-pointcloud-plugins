#include "usdlas/Las.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
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
    const bool nativeLittleEndian = *reinterpret_cast<const std::uint8_t*>(&marker) == 1;
    if (!nativeLittleEndian) {
        std::reverse(encoded.begin(), encoded.end());
    }
    std::copy(encoded.begin(), encoded.end(), bytes.begin() + offset);
}

std::vector<std::uint8_t> MakeHeader(std::uint8_t versionMinor,
                                     std::uint8_t format) {
    const std::size_t size = versionMinor == 4 ? 375
                              : versionMinor == 3 ? 235
                                                  : 227;
    std::vector<std::uint8_t> bytes(size, 0);
    std::memcpy(bytes.data(), "LASF", 4);
    Write(bytes, 24, std::uint8_t{1});
    Write(bytes, 25, versionMinor);
    const auto headerSize = versionMinor == 4   ? std::uint32_t{375}
                            : versionMinor == 3 ? std::uint32_t{235}
                                                : std::uint32_t{227};
    Write(bytes, 94, static_cast<std::uint16_t>(headerSize));
    Write(bytes, 96, headerSize);
    Write(bytes, 104, format);
    const auto recordLength = format == 0 ? 20
                              : format == 1 ? 28
                              : format == 2 ? 26
                              : format == 3 ? 34
                              : format == 4 ? 57
                              : format == 5 ? 63
                              : format == 6 ? 30
                              : format == 7 ? 36
                              : format == 8 ? 38
                                             : format == 9 ? 59 : 67;
    Write(bytes, 105, static_cast<std::uint16_t>(recordLength));
    Write(bytes, 107, std::uint32_t{1});
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
    if (versionMinor == 4) {
        Write(bytes, 247, std::uint64_t{1});
    }
    return bytes;
}

void TestHeaderAndPoint() {
    auto bytes = MakeHeader(2, 3);
    usdlas::LasHeader header;
    std::string error;
    Check(usdlas::InspectHeader(bytes, header, error));
    Check(header.pointCount == 1 && header.xOffset == 1000.0);

    std::vector<std::uint8_t> record(34, 0);
    Write(record, 0, std::int32_t{123});
    Write(record, 4, std::int32_t{-50});
    Write(record, 8, std::int32_t{300});
    Write(record, 12, std::uint16_t{42});
    Write(record, 14, std::uint8_t{0x21});
    Write(record, 15, std::uint8_t{2});
    Write(record, 20, 12.5);
    Write(record, 28, std::uint16_t{100});
    Write(record, 30, std::uint16_t{200});
    Write(record, 32, std::uint16_t{300});
    usdlas::LasPoint point;
    Check(usdlas::DecodePoint(header, record, point, error));
    Check(point.sourcePosition.x == 1001.23 && point.sourcePosition.y == 1999.5 &&
          point.sourcePosition.z == 3003.0);
    Check(point.intensity == 42 && point.returnNumber == 1 &&
          point.numberOfReturns == 2 && point.classification == 2);
    Check(point.hasColor && point.red == 100 && point.hasGpsTime &&
          point.gpsTime == 12.5);
}

void TestPointCloudAssetHelpers() {
    const auto bytes = MakeHeader(2, 0);
    usdlas::LasHeader header;
    std::string error;
    Check(usdlas::InspectHeader(bytes, header, error));

    usdlas::LasPoint point;
    point.sourcePosition = {1001.0, 2000.0, 3000.0};
    point.intensity = 42;
    usdpointcloud::PointData data;
    Check(usdlas::AppendPointData(header, {point}, "sample.las", data, error));
    Check(data.positions.size() == 1 && data.intensity[0] == 42);

    usdpointcloud::PointCloudAsset asset;
    Check(usdlas::BuildPointCloudAsset(
        header, data, "LAS CRS unavailable", asset, error));
    Check(asset.IsValid());
    Check(asset.reference.stageUpAxis == "Y");
    Check(asset.chunk.pointCount == 1);
}

void TestValidation() {
    usdlas::LasHeader header;
    std::string error;
    auto unsupported = MakeHeader(2, 5);
    Check(!usdlas::InspectHeader(unsupported, header, error));
    auto modernOnOldVersion = MakeHeader(2, 6);
    Check(!usdlas::InspectHeader(modernOnOldVersion, header, error));
    auto las13 = MakeHeader(3, 0);
    Check(usdlas::InspectHeader(las13, header, error));
    auto las14 = MakeHeader(4, 6);
    Check(usdlas::InspectHeader(las14, header, error));
    Check(header.pointCount == 1);
    std::vector<std::uint8_t> shortRecord(10);
    usdlas::LasPoint point;
    Check(!usdlas::DecodePoint(header, shortRecord, point, error));
    header.pointRecordLength = 1;
    Check(!usdlas::DecodePoint(header, std::vector<std::uint8_t>(1), point,
                               error));

    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdlas::InspectHeader({}, header, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::InvalidSignature &&
          diagnostics.front().severity == usdgeo::Severity::Error);

    auto invalidPointHeader = MakeHeader(2, 0);
    Write(invalidPointHeader, 131, std::numeric_limits<double>::max());
    Check(usdlas::InspectHeader(invalidPointHeader, header, diagnostics));
    std::vector<std::uint8_t> invalidRecord(34, 0);
    Write(invalidRecord, 0, std::numeric_limits<std::int32_t>::max());
    Check(!usdlas::DecodePoint(header, invalidRecord, point, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::NonFiniteCoordinate);
}

void TestRangeReader() {
    const auto filename =
        std::filesystem::temp_directory_path() / "usd-geo-plugins-test.las";
    auto bytes = MakeHeader(2, 0);
    Write(bytes, 107, std::uint32_t{3});
    for (int index = 0; index < 3; ++index) {
        std::vector<std::uint8_t> record(20, 0);
        Write(record, 0, static_cast<std::int32_t>(index + 1));
        Write(record, 4, static_cast<std::int32_t>(index + 2));
        Write(record, 8, static_cast<std::int32_t>(index + 3));
        bytes.insert(bytes.end(), record.begin(), record.end());
    }
    {
        std::ofstream output(filename, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    usdlas::LasReader reader(filename.string());
    usdlas::LasReadOptions options;
    options.chunkPointLimit = 2;
    options.memoryBudgetBytes = 2 * (sizeof(usdlas::LasPoint) + 40);
    options.range = {1, 1};
    usdlas::LasHeader header;
    std::string error;
    std::size_t points = 0;
    Check(reader.Read(
        options,
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>& data) {
            points += data.size();
            Check(data.front().sourcePosition.x == 1000.02);
            return true;
        },
        header, error));
    Check(points == 1);

    auto invalidBytes = MakeHeader(2, 0);
    Write(invalidBytes, 131, std::numeric_limits<double>::max());
    std::vector<std::uint8_t> invalidRecord(20, 0);
    Write(invalidRecord, 0, std::int32_t{2});
    invalidBytes.insert(invalidBytes.end(), invalidRecord.begin(),
                        invalidRecord.end());
    {
        std::ofstream output(filename, std::ios::binary);
        output.write(reinterpret_cast<const char*>(invalidBytes.data()),
                     static_cast<std::streamsize>(invalidBytes.size()));
    }
    usdgeo::Diagnostic diagnostics;
    std::vector<usdgeo::Diagnostic> typedDiagnostics;
    Check(!reader.Read(
        {},
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>&) { return true; },
        header, typedDiagnostics));
    Check(typedDiagnostics.size() == 1);
    diagnostics = typedDiagnostics.front();
    Check(diagnostics.code == usdgeo::DiagnosticCode::NonFiniteCoordinate &&
          diagnostics.pointIndex == 0 && diagnostics.byteOffset == 227);
    Check(reader.FailureKind() == usdlas::LasReadFailure::PointDecode);
    Check(std::remove(filename.string().c_str()) == 0);
}

void TestReaderFailureKinds() {
    const auto filename =
        std::filesystem::temp_directory_path() / "usd-geo-plugins-evlr-test.las";
    auto bytes = MakeHeader(4, 6);
    Write(bytes, 235, std::uint64_t{375});
    Write(bytes, 243, std::uint32_t{1});
    bytes.resize(375);
    {
        std::ofstream output(filename, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    usdlas::LasReader reader(filename.string());
    usdlas::LasHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.Read(
        {},
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>&) { return true; },
        header, diagnostics));
    Check(reader.FailureKind() == usdlas::LasReadFailure::Evlr);
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::TruncatedHeader);
    Check(std::remove(filename.string().c_str()) == 0);
}

    void TestWaveformPointFormats() {
        const auto check = [](std::uint8_t versionMinor,
                      std::uint8_t format,
                      std::size_t waveformOffset) {
          auto bytes = MakeHeader(versionMinor, format);
          usdlas::LasHeader header;
          std::string error;
          Check(usdlas::InspectHeader(bytes, header, error));
          std::vector<std::uint8_t> record(header.pointRecordLength, 0);
                    Write(record, waveformOffset, std::uint8_t{0x87});
                    Write(record, waveformOffset + 1, std::uint64_t{1234});
                    if (format == 10) {
                        Write(record, 36, std::uint16_t{900});
                    }
          Write(record, waveformOffset + 9, std::uint32_t{48});
          Write(record, waveformOffset + 13, 0.25f);
          Write(record, waveformOffset + 17, 1.0f);
          Write(record, waveformOffset + 21, 2.0f);
          Write(record, waveformOffset + 25, 3.0f);
          usdlas::LasPoint point;
          Check(usdlas::DecodePoint(header, record, point, error));
          Check(point.hasWaveform && point.waveform.descriptorIndex == 7 &&
              point.waveform.external && point.waveform.dataOffset == 1234 &&
              point.waveform.packetSize == 48 &&
              point.waveform.returnPointLocation == 0.25f &&
              point.waveform.xt == 1.0f && point.waveform.yt == 2.0f &&
              point.waveform.zt == 3.0f &&
              (format != 10 || point.nir == 900));
        };

        check(3, 4, 28);
        check(3, 5, 34);
        check(4, 9, 30);
        check(4, 10, 38);

        usdlas::LasHeader header;
        std::string error;
        Check(!usdlas::InspectHeader(MakeHeader(2, 4), header, error));
    }

void TestVariableLengthRecords() {
    auto bytes = MakeHeader(2, 0);
    constexpr std::size_t recordOffset = 227;
    const std::string wkt = "WKT[\"EPSG:4978\"]";
    bytes.resize(recordOffset + 54 + wkt.size() + 1);
    Write(bytes, 100, std::uint32_t{1});
    Write(bytes, 96, static_cast<std::uint32_t>(bytes.size()));
    std::memcpy(bytes.data() + recordOffset + 2, "LASF_Projection", 15);
    Write(bytes, recordOffset + 18, std::uint16_t{2112});
    Write(bytes, recordOffset + 20, static_cast<std::uint16_t>(wkt.size() + 1));
    std::memcpy(bytes.data() + recordOffset + 54, wkt.c_str(), wkt.size() + 1);

    usdlas::LasHeader header;
    std::string error;
    Check(usdlas::InspectMetadata(bytes, header, error));
    Check(header.variableLengthRecords.size() == 1);
    Check(header.variableLengthRecords.front().userId == "LASF_Projection");
    Check(header.crsWkt == wkt);

    auto evlrBytes = MakeHeader(4, 6);
    constexpr std::size_t evlrOffset = 375;
    const std::string evlrText = "extended";
    evlrBytes.resize(evlrOffset + 60 + evlrText.size());
    Write(evlrBytes, 96, static_cast<std::uint32_t>(evlrOffset));
    Write(evlrBytes, 235, static_cast<std::uint64_t>(evlrOffset));
    Write(evlrBytes, 243, std::uint32_t{1});
    std::memcpy(evlrBytes.data() + evlrOffset + 2, "LASF_Projection", 15);
    Write(evlrBytes, evlrOffset + 18, std::uint16_t{2112});
    Write(evlrBytes, evlrOffset + 20, static_cast<std::uint64_t>(evlrText.size()));
    std::memcpy(evlrBytes.data() + evlrOffset + 60, evlrText.data(), evlrText.size());
    Check(usdlas::InspectMetadata(evlrBytes, header, error));
    Check(header.variableLengthRecords.size() == 1 &&
          header.variableLengthRecords.front().isExtended);

    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdlas::InspectRecords(std::vector<std::uint8_t>(54, 0), 227, 1,
                                  false, header.variableLengthRecords,
                                  diagnostics));
    Check(diagnostics.size() == 1 && diagnostics.front().byteOffset.has_value() &&
          diagnostics.front().byteOffset.value() == 227);

        auto structuredBytes = MakeHeader(2, 0);
        auto appendRecord = [&](const char* userId,
                        std::uint16_t recordId,
                        const std::vector<std::uint8_t>& data) {
          const auto offset = structuredBytes.size();
          structuredBytes.resize(offset + 54 + data.size());
          std::memcpy(structuredBytes.data() + offset + 2, userId,
                  std::strlen(userId));
          Write(structuredBytes, offset + 18, recordId);
          Write(structuredBytes, offset + 20,
              static_cast<std::uint16_t>(data.size()));
          std::copy(data.begin(), data.end(), structuredBytes.begin() + offset + 54);
        };

        std::vector<std::uint8_t> keyDirectory(24, 0);
        Write(keyDirectory, 0, std::uint16_t{1});
        Write(keyDirectory, 2, std::uint16_t{1});
        Write(keyDirectory, 6, std::uint16_t{2});
        Write(keyDirectory, 8, std::uint16_t{2048});
        Write(keyDirectory, 12, std::uint16_t{1});
        Write(keyDirectory, 14, std::uint16_t{4326});
        Write(keyDirectory, 16, std::uint16_t{3072});
        Write(keyDirectory, 20, std::uint16_t{1});
        Write(keyDirectory, 22, std::uint16_t{32610});
        appendRecord("LASF_Projection", 34735, keyDirectory);

        std::vector<std::uint8_t> doubleParameters(2 * sizeof(double));
        Write(doubleParameters, 0, 0.01);
        Write(doubleParameters, sizeof(double), 1000.0);
        appendRecord("LASF_Projection", 34736, doubleParameters);

        const std::string asciiParameters = "WGS 84|UTM zone 10 N|";
        appendRecord("LASF_Projection", 34737,
                 std::vector<std::uint8_t>(asciiParameters.begin(),
                                 asciiParameters.end()));

        std::vector<std::uint8_t> extraBytes(192, 0);
        Write(extraBytes, 2, std::uint8_t{9});
        std::memcpy(extraBytes.data() + 4, "temperature", 11);
        Write(extraBytes, 40, -9999.0);
        Write(extraBytes, 64, -50.0);
        Write(extraBytes, 88, 100.0);
        Write(extraBytes, 112, 0.01);
        Write(extraBytes, 136, 100.0);
        std::memcpy(extraBytes.data() + 160, "scaled temperature", 18);
        appendRecord("LASF_Spec", 4, extraBytes);
        Write(structuredBytes, 100, std::uint32_t{4});
        Write(structuredBytes, 96, static_cast<std::uint32_t>(structuredBytes.size()));

        Check(usdlas::InspectMetadata(structuredBytes, header, error));
        Check(header.geoTiffMetadata.has_value());
        Check(header.geoTiffMetadata->keys.size() == 2 &&
            header.geoTiffMetadata->keys[0].keyId == 2048 &&
            header.geoTiffMetadata->keys[1].valueOffset == 32610);
        Check(header.geoTiffMetadata->doubleParameters.size() == 2 &&
            header.geoTiffMetadata->doubleParameters[1] == 1000.0 &&
            header.geoTiffMetadata->asciiParameters == "WGS 84|UTM zone 10 N|");
        Check(header.extraBytes.size() == 1 &&
            header.extraBytes.front().dataType == 9 &&
            header.extraBytes.front().name == "temperature" &&
            header.extraBytes.front().scale.x == 0.01 &&
            header.extraBytes.front().offset.x == 100.0 &&
            header.extraBytes.front().description == "scaled temperature");
}

} // namespace

int main() {
    TestHeaderAndPoint();
    TestPointCloudAssetHelpers();
    TestValidation();
    TestRangeReader();
    TestReaderFailureKinds();
    TestWaveformPointFormats();
    TestVariableLengthRecords();
    return 0;
}