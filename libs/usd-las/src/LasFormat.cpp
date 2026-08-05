#include "LasInternal.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace usdlas::detail {

bool Has(const std::vector<std::uint8_t>& bytes,
         std::size_t offset,
         std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

std::string ReadText(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset,
                     std::size_t size) {
    const auto end = std::find(bytes.begin() + offset,
                               bytes.begin() + offset + size, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       static_cast<std::size_t>(end - (bytes.begin() + offset)));
}

bool IsSupportedFormat(std::uint8_t format) {
    return format <= 5 || (format >= 6 && format <= 10);
}

bool IsSupportedFormatForVersion(std::uint8_t versionMinor,
                                 std::uint8_t format) {
    if (!IsSupportedFormat(format)) {
        return false;
    }
    if (format == 4 || format == 5) {
        return versionMinor == 3 || versionMinor == 4;
    }
    return format < 6 || versionMinor == 4;
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
    if (error == "LAS GeoTIFF key directory is truncated" ||
        error == "LAS GeoTIFF double parameters are truncated") {
        return usdgeo::DiagnosticCode::InvalidCrs;
    }
    if (error == "LAS GeoTIFF CRS keys conflict" ||
        error == "LAS CRS definitions conflict") {
        return usdgeo::DiagnosticCode::ConflictingCrs;
    }
    if (error == "LAS Extra Bytes VLR has an invalid length") {
        return usdgeo::DiagnosticCode::InvalidRecordLength;
    }
    if (error == "LAS point record length does not match Extra Bytes descriptors") {
        return usdgeo::DiagnosticCode::InvalidRecordLength;
    }
    if (error == "unsupported LAS Extra Bytes data type") {
        return usdgeo::DiagnosticCode::UnsupportedExtraBytesType;
    }
    if (error == "decoded LAS point contains a non-finite coordinate") {
        return usdgeo::DiagnosticCode::NonFiniteCoordinate;
    }
    if (error == "decoded LAS point contains a non-finite Extra Bytes value") {
        return usdgeo::DiagnosticCode::NonFiniteExtraBytes;
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
    case 4:
        return 57;
    case 5:
        return 63;
    case 6:
        return 30;
    case 7:
        return 36;
    case 8:
        return 38;
    case 9:
        return 59;
    case 10:
        return 67;
    default:
        return 0;
    }
}

std::size_t ExtraByteScalarSize(std::uint8_t dataType) {
    if (dataType >= 11 && dataType <= 30) {
        dataType = (dataType - 1) % 10 + 1;
    }
    switch (dataType) {
    case 1:
    case 2:
        return 1;
    case 3:
    case 4:
        return 2;
    case 5:
    case 6:
    case 9:
        return 4;
    case 7:
    case 8:
    case 10:
        return 8;
    default:
        return 0;
    }
}

std::uint8_t ExtraByteComponentCount(std::uint8_t dataType) {
    if (dataType >= 1 && dataType <= 10) {
        return 1;
    }
    if (dataType >= 11 && dataType <= 20) {
        return 2;
    }
    if (dataType >= 21 && dataType <= 30) {
        return 3;
    }
    return 0;
}

bool ValidateExtraBytesLayout(const LasHeader& header, std::string& error) {
    std::size_t extraBytesSize = 0;
    for (const auto& descriptor : header.extraBytes) {
        const auto scalarSize = ExtraByteScalarSize(descriptor.dataType);
        const auto componentCount =
            ExtraByteComponentCount(descriptor.dataType);
        if (scalarSize == 0 || componentCount == 0) {
            error = "unsupported LAS Extra Bytes data type";
            return false;
        }
        if (extraBytesSize > (std::numeric_limits<std::size_t>::max)() -
                                 scalarSize * componentCount) {
            error = "LAS Extra Bytes layout is too large";
            return false;
        }
        extraBytesSize += scalarSize * componentCount;
    }
    const auto baseSize = MinimumRecordLength(header.pointFormat);
    if (baseSize > (std::numeric_limits<std::size_t>::max)() -
                        extraBytesSize ||
        header.pointRecordLength != baseSize + extraBytesSize) {
        error = "LAS point record length does not match Extra Bytes descriptors";
        return false;
    }
    return true;
}

double ReadExtraByteScalar(const std::vector<std::uint8_t>& record,
                           std::size_t offset,
                           std::uint8_t dataType) {
    if (dataType >= 11 && dataType <= 30) {
        dataType = (dataType - 1) % 10 + 1;
    }
    switch (dataType) {
    case 1:
        return ReadLittle<std::uint8_t>(record, offset);
    case 2:
        return ReadLittle<std::int8_t>(record, offset);
    case 3:
        return ReadLittle<std::uint16_t>(record, offset);
    case 4:
        return ReadLittle<std::int16_t>(record, offset);
    case 5:
        return ReadLittle<std::uint32_t>(record, offset);
    case 6:
        return ReadLittle<std::int32_t>(record, offset);
    case 7:
        return static_cast<double>(ReadLittle<std::uint64_t>(record, offset));
    case 8:
        return static_cast<double>(ReadLittle<std::int64_t>(record, offset));
    case 9:
        return ReadLittle<float>(record, offset);
    case 10:
        return ReadLittle<double>(record, offset);
    default:
        return 0.0;
    }
}

bool ReadRecords(const std::vector<std::uint8_t>& bytes,
                 std::size_t offset,
                 std::uint32_t count,
                 bool extended,
                 std::vector<LasVariableLengthRecord>& records,
                 std::string& error) {
    const std::size_t headerLength = extended ? 60 : 54;
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!Has(bytes, offset, headerLength)) {
            error = "LAS variable-length record header is truncated";
            return false;
        }
        LasVariableLengthRecord record;
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

} // namespace usdlas::detail

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
    if (!detail::Has(bytes, 0, 227) ||
        std::memcmp(bytes.data(), "LASF", 4) != 0) {
        error = "LAS header is missing or has an invalid signature";
        return false;
    }

    header.versionMajor = detail::ReadLittle<std::uint8_t>(bytes, 24);
    header.versionMinor = detail::ReadLittle<std::uint8_t>(bytes, 25);
    header.headerSize = detail::ReadLittle<std::uint16_t>(bytes, 94);
    header.pointDataOffset = detail::ReadLittle<std::uint32_t>(bytes, 96);
    header.variableLengthRecordCount =
        detail::ReadLittle<std::uint32_t>(bytes, 100);
    header.pointFormat = detail::ReadLittle<std::uint8_t>(bytes, 104);
    header.pointRecordLength = detail::ReadLittle<std::uint16_t>(bytes, 105);
    const auto legacyCount = detail::ReadLittle<std::uint32_t>(bytes, 107);

    if (header.versionMajor != 1 || header.versionMinor < 2 ||
        header.versionMinor > 4) {
        error = "unsupported LAS version";
        return false;
    }
    if (!detail::IsSupportedFormatForVersion(header.versionMinor,
                                             header.pointFormat)) {
        error = "unsupported LAS point format";
        return false;
    }
    const auto minimumHeaderSize = detail::MinimumHeaderSize(header.versionMinor);
    if (header.headerSize < minimumHeaderSize ||
        header.pointDataOffset < header.headerSize ||
        !detail::Has(bytes, 0, header.headerSize)) {
        error = "LAS header offsets are invalid";
        return false;
    }

    header.pointCount = legacyCount;
    if (header.versionMinor == 4) {
        if (!detail::Has(bytes, 247, sizeof(std::uint64_t))) {
            error = "LAS 1.4 extended point count is missing";
            return false;
        }
        header.pointCount = detail::ReadLittle<std::uint64_t>(bytes, 247);
        header.firstExtendedVariableLengthRecordOffset =
            detail::ReadLittle<std::uint64_t>(bytes, 235);
        header.extendedVariableLengthRecordCount =
            detail::ReadLittle<std::uint32_t>(bytes, 243);
    }

    header.xScale = detail::ReadLittle<double>(bytes, 131);
    header.yScale = detail::ReadLittle<double>(bytes, 139);
    header.zScale = detail::ReadLittle<double>(bytes, 147);
    header.xOffset = detail::ReadLittle<double>(bytes, 155);
    header.yOffset = detail::ReadLittle<double>(bytes, 163);
    header.zOffset = detail::ReadLittle<double>(bytes, 171);
    header.bounds.maximum = {detail::ReadLittle<double>(bytes, 179),
                             detail::ReadLittle<double>(bytes, 195),
                             detail::ReadLittle<double>(bytes, 211)};
    header.bounds.minimum = {detail::ReadLittle<double>(bytes, 187),
                             detail::ReadLittle<double>(bytes, 203),
                             detail::ReadLittle<double>(bytes, 219)};

    if (!header.IsValid()) {
        error = "LAS header contains invalid scales, offsets, or bounds";
        return false;
    }
    if (header.pointRecordLength <
        detail::MinimumRecordLength(header.pointFormat)) {
        error = "LAS point record length is too small for its format";
        return false;
    }
    return true;
}

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::string& error) {
    return detail::ReadRecords(bytes, offset, count, extended, records, error);
}

bool InspectHeader(const std::vector<std::uint8_t>& bytes,
                   LasHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (InspectHeader(bytes, header, error)) {
        return true;
    }
    detail::AddErrorDiagnostic(error, diagnostics);
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
    detail::AddErrorDiagnostic(error, diagnostics);
    if (!diagnostics.empty()) {
        diagnostics.front().byteOffset = offset;
    }
    return false;
}

} // namespace usdlas