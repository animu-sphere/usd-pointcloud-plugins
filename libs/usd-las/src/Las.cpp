#include "usdlas/Las.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

template <typename T>
T ReadLittle(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(std::uint64_t));
    std::array<std::uint8_t, sizeof(T)> encoded{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        encoded[index] = bytes[offset + index];
    }
    const std::uint16_t marker = 1;
    const bool nativeLittleEndian = *reinterpret_cast<const std::uint8_t*>(&marker) == 1;
    if (!nativeLittleEndian) {
        std::reverse(encoded.begin(), encoded.end());
    }
    T value{};
    std::memcpy(&value, encoded.data(), sizeof(T));
    return value;
}

bool Has(const std::vector<std::uint8_t>& bytes,
         std::size_t offset,
         std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

bool IsSupportedFormat(std::uint8_t format) {
    return format <= 3 || (format >= 6 && format <= 8);
}

bool IsSupportedFormatForVersion(std::uint8_t versionMinor,
                                 std::uint8_t format) {
    return IsSupportedFormat(format) && (format < 6 || versionMinor == 4);
}

usdgeo::DiagnosticCode CodeForError(const std::string& error) {
    if (error == "LAS header is missing or has an invalid signature") {
        return usdgeo::DiagnosticCode::InvalidSignature;
    }
    if (error == "unsupported LAS version") {
        return usdgeo::DiagnosticCode::UnsupportedVersion;
    }
    if (error == "unsupported LAS point format") {
        return usdgeo::DiagnosticCode::UnsupportedPointFormat;
    }
    if (error == "LAS header offsets are invalid" ||
        error == "LAS extended variable-length record offset is invalid") {
        return usdgeo::DiagnosticCode::InvalidOffset;
    }
    if (error == "LAS point record length is too small for its format") {
        return usdgeo::DiagnosticCode::InvalidRecordLength;
    }
    if (error == "LAS variable-length record header is truncated" ||
        error == "LAS 1.4 extended point count is missing") {
        return usdgeo::DiagnosticCode::TruncatedHeader;
    }
    if (error == "LAS variable-length record data is truncated") {
        return usdgeo::DiagnosticCode::TruncatedRecord;
    }
    if (error == "decoded LAS point contains a non-finite coordinate") {
        return usdgeo::DiagnosticCode::NonFiniteCoordinate;
    }
    return usdgeo::DiagnosticCode::DecodeFailure;
}

void AddErrorDiagnostic(const std::string& error,
                        std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back({CodeForError(error), usdgeo::Severity::Error, error,
                            std::nullopt, std::nullopt});
}

std::size_t MinimumHeaderSize(std::uint8_t versionMinor) {
    switch (versionMinor) {
    case 2:
        return 227;
    case 3:
        return 235;
    case 4:
        return 375;
    default:
        return 0;
    }
}

std::size_t MinimumRecordLength(std::uint8_t format) {
    switch (format) {
    case 0:
        return 20;
    case 1:
        return 28;
    case 2:
        return 26;
    case 3:
        return 34;
    case 6:
        return 30;
    case 7:
        return 36;
    case 8:
        return 38;
    default:
        return 0;
    }
}

std::string ReadText(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset,
                     std::size_t size) {
    const auto end = std::find(bytes.begin() + offset,
                               bytes.begin() + offset + size, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       static_cast<std::size_t>(end - (bytes.begin() + offset)));
}

bool ReadRecords(const std::vector<std::uint8_t>& bytes,
                 std::size_t offset,
                 std::uint32_t count,
                 bool extended,
                 std::vector<usdlas::LasVariableLengthRecord>& records,
                 std::string& error) {
    const std::size_t headerLength = extended ? 60 : 54;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!Has(bytes, offset, headerLength)) {
            error = "LAS variable-length record header is truncated";
            return false;
        }
        usdlas::LasVariableLengthRecord record;
        record.userId = ReadText(bytes, offset + 2, 16);
        record.recordId = ReadLittle<std::uint16_t>(bytes, offset + 18);
        const auto length = extended
                                ? ReadLittle<std::uint64_t>(bytes, offset + 20)
                                : ReadLittle<std::uint16_t>(bytes, offset + 20);
        record.description = ReadText(bytes, offset + (extended ? 28 : 22), 32);
        record.isExtended = extended;
        offset += headerLength;
        if (length > bytes.size() - offset) {
            error = "LAS variable-length record data is truncated";
            return false;
        }
        record.data.assign(bytes.begin() + offset,
                           bytes.begin() + offset + static_cast<std::size_t>(length));
        records.push_back(std::move(record));
        offset += static_cast<std::size_t>(length);
    }
    return true;
}

} // namespace

namespace usdlas {

bool LasHeader::IsValid() const noexcept {
    return versionMajor == 1 && versionMinor >= 2 && versionMinor <= 4 &&
           headerSize > 0 && pointRecordLength > 0 &&
           std::isfinite(xScale) && std::isfinite(yScale) &&
           std::isfinite(zScale) && xScale != 0.0 && yScale != 0.0 &&
           zScale != 0.0 && bounds.IsValid();
}

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::string& error) {
    header = {};
    error.clear();
    if (!Has(bytes, 0, 227) || std::memcmp(bytes.data(), "LASF", 4) != 0) {
        error = "LAS header is missing or has an invalid signature";
        return false;
    }

    header.versionMajor = ReadLittle<std::uint8_t>(bytes, 24);
    header.versionMinor = ReadLittle<std::uint8_t>(bytes, 25);
    header.headerSize = ReadLittle<std::uint16_t>(bytes, 94);
    header.pointDataOffset = ReadLittle<std::uint32_t>(bytes, 96);
    header.variableLengthRecordCount = ReadLittle<std::uint32_t>(bytes, 100);
    header.pointFormat = ReadLittle<std::uint8_t>(bytes, 104);
    header.pointRecordLength = ReadLittle<std::uint16_t>(bytes, 105);
    const auto legacyCount = ReadLittle<std::uint32_t>(bytes, 107);

    if (header.versionMajor != 1 || header.versionMinor < 2 ||
        header.versionMinor > 4) {
        error = "unsupported LAS version";
        return false;
    }
    if (!IsSupportedFormatForVersion(header.versionMinor,
                                     header.pointFormat)) {
        error = "unsupported LAS point format";
        return false;
    }
    const auto minimumHeaderSize = MinimumHeaderSize(header.versionMinor);
    if (header.headerSize < minimumHeaderSize ||
        header.pointDataOffset < header.headerSize ||
        !Has(bytes, 0, header.headerSize)) {
        error = "LAS header offsets are invalid";
        return false;
    }

    header.pointCount = legacyCount;
    if (header.versionMinor == 4) {
        if (!Has(bytes, 247, sizeof(std::uint64_t))) {
            error = "LAS 1.4 extended point count is missing";
            return false;
        }
        header.pointCount = ReadLittle<std::uint64_t>(bytes, 247);
        header.firstExtendedVariableLengthRecordOffset =
            ReadLittle<std::uint64_t>(bytes, 235);
        header.extendedVariableLengthRecordCount =
            ReadLittle<std::uint32_t>(bytes, 243);
    }

    header.xScale = ReadLittle<double>(bytes, 131);
    header.yScale = ReadLittle<double>(bytes, 139);
    header.zScale = ReadLittle<double>(bytes, 147);
    header.xOffset = ReadLittle<double>(bytes, 155);
    header.yOffset = ReadLittle<double>(bytes, 163);
    header.zOffset = ReadLittle<double>(bytes, 171);
    header.bounds.maximum = {ReadLittle<double>(bytes, 179),
                             ReadLittle<double>(bytes, 195),
                             ReadLittle<double>(bytes, 211)};
    header.bounds.minimum = {ReadLittle<double>(bytes, 187),
                             ReadLittle<double>(bytes, 203),
                             ReadLittle<double>(bytes, 219)};

    if (!header.IsValid()) {
        error = "LAS header contains invalid scales, offsets, or bounds";
        return false;
    }
    if (header.pointRecordLength < MinimumRecordLength(header.pointFormat)) {
        error = "LAS point record length is too small for its format";
        return false;
    }
    return true;
}

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::string& error) {
    if (!InspectHeader(bytes, header, error)) {
        return false;
    }
    header.variableLengthRecords.clear();
    if (!ReadRecords(bytes, header.headerSize, header.variableLengthRecordCount,
                     false, header.variableLengthRecords, error)) {
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset == 0 ||
            header.firstExtendedVariableLengthRecordOffset > bytes.size()) {
            error = "LAS extended variable-length record offset is invalid";
            return false;
        }
        if (!ReadRecords(bytes,
                         static_cast<std::size_t>(header.firstExtendedVariableLengthRecordOffset),
                         header.extendedVariableLengthRecordCount, true,
                         header.variableLengthRecords, error)) {
            return false;
        }
    }
    header.crsWkt = ExtractWktCrs(header.variableLengthRecords);
    return true;
}

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::string& error) {
    return ReadRecords(bytes, offset, count, extended, records, error);
}

std::string ExtractWktCrs(const std::vector<LasVariableLengthRecord>& records) {
    for (const auto& record : records) {
        if (record.userId == "LASF_Projection" && record.recordId == 2112) {
            std::string wkt(reinterpret_cast<const char*>(record.data.data()),
                            record.data.size());
            while (!wkt.empty() && wkt.back() == '\0') {
                wkt.pop_back();
            }
            return wkt;
        }
    }
    return {};
}

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::string& error) {
    point = {};
    error.clear();
    if (!header.IsValid() ||
        !IsSupportedFormatForVersion(header.versionMinor,
                                     header.pointFormat) ||
        header.pointRecordLength < MinimumRecordLength(header.pointFormat) ||
        record.size() < header.pointRecordLength) {
        error = "LAS point record is invalid or truncated";
        return false;
    }

    const bool modern = header.pointFormat >= 6;
    const auto x = ReadLittle<std::int32_t>(record, 0);
    const auto y = ReadLittle<std::int32_t>(record, 4);
    const auto z = ReadLittle<std::int32_t>(record, 8);
    point.sourcePosition = {x * header.xScale + header.xOffset,
                             y * header.yScale + header.yOffset,
                             z * header.zScale + header.zOffset};
    point.intensity = ReadLittle<std::uint16_t>(record, 12);
    const auto returnFlags = ReadLittle<std::uint8_t>(record, 14);
    point.returnNumber = returnFlags & 0x0f;
    point.numberOfReturns = (returnFlags >> 4) & 0x0f;
    point.classification = ReadLittle<std::uint8_t>(record, modern ? 16 : 15);

    if (modern) {
        if (header.pointFormat >= 7) {
            point.red = ReadLittle<std::uint16_t>(record, 30);
            point.green = ReadLittle<std::uint16_t>(record, 32);
            point.blue = ReadLittle<std::uint16_t>(record, 34);
            point.hasColor = true;
        }
        point.gpsTime = ReadLittle<double>(record, 22);
        point.hasGpsTime = true;
    } else {
        if (header.pointFormat == 1 || header.pointFormat == 3) {
            point.gpsTime = ReadLittle<double>(record, 20);
            point.hasGpsTime = true;
        }
        if (header.pointFormat == 2 || header.pointFormat == 3) {
            const auto colorOffset = header.pointFormat == 2 ? 20 : 28;
            point.red = ReadLittle<std::uint16_t>(record, colorOffset);
            point.green = ReadLittle<std::uint16_t>(record, colorOffset + 2);
            point.blue = ReadLittle<std::uint16_t>(record, colorOffset + 4);
            point.hasColor = true;
        }
    }
    if (!std::isfinite(point.sourcePosition.x) ||
        !std::isfinite(point.sourcePosition.y) ||
        !std::isfinite(point.sourcePosition.z)) {
        error = "decoded LAS point contains a non-finite coordinate";
        return false;
    }
    return true;
}

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (InspectHeader(bytes, header, error)) {
        return true;
    }
    AddErrorDiagnostic(error, diagnostics);
    return false;
}

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (InspectMetadata(bytes, header, error)) {
        return true;
    }
    AddErrorDiagnostic(error, diagnostics);
    return false;
}

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (InspectRecords(bytes, offset, count, extended, records, error)) {
        return true;
    }
    AddErrorDiagnostic(error, diagnostics);
    return false;
}

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (DecodePoint(header, record, point, error)) {
        return true;
    }
    AddErrorDiagnostic(error, diagnostics);
    return false;
}

} // namespace usdlas