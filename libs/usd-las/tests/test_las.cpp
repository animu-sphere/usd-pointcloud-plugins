#include "usdlas/Las.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
    Write(bytes, 105, std::uint16_t{34});
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
}

} // namespace

int main() {
    TestHeaderAndPoint();
    TestValidation();
    TestVariableLengthRecords();
    return 0;
}