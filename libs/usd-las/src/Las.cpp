#include "usdlas/Las.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <limits>
#include <type_traits>
#include <utility>

namespace {

enum class RangeReadFailure {
    None,
    InvalidOffset,
    Seek,
    Read,
};

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

bool ValidateExtraBytesLayout(const usdlas::LasHeader& header,
                              std::string& error) {
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

std::string ReadText(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset,
                     std::size_t size) {
    const auto end = std::find(bytes.begin() + offset,
                               bytes.begin() + offset + size, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(bytes.data() + offset),
                       static_cast<std::size_t>(end - (bytes.begin() + offset)));
}

constexpr std::size_t kExtraBytesDescriptorSize = 192;

bool IsProjectionRecord(const usdlas::LasVariableLengthRecord& record,
                        std::uint16_t recordId) {
    return record.userId == "LASF_Projection" && record.recordId == recordId;
}

bool IsExtraBytesRecord(const usdlas::LasVariableLengthRecord& record) {
    return record.userId == "LASF_Spec" && record.recordId == 4;
}

bool ParseGeoTiffRecords(
    const std::vector<usdlas::LasVariableLengthRecord>& records,
    std::optional<usdlas::LasGeoTiffMetadata>& metadata,
    std::string& error) {
    const usdlas::LasVariableLengthRecord* keyDirectory = nullptr;
    const usdlas::LasVariableLengthRecord* doubleParameters = nullptr;
    const usdlas::LasVariableLengthRecord* asciiParameters = nullptr;
    for (const auto& record : records) {
        if (IsProjectionRecord(record, 34735) && keyDirectory == nullptr) {
            keyDirectory = &record;
        } else if (IsProjectionRecord(record, 34736) &&
                   doubleParameters == nullptr) {
            doubleParameters = &record;
        } else if (IsProjectionRecord(record, 34737) &&
                   asciiParameters == nullptr) {
            asciiParameters = &record;
        }
    }
    if (keyDirectory == nullptr && doubleParameters == nullptr &&
        asciiParameters == nullptr) {
        metadata.reset();
        return true;
    }

    usdlas::LasGeoTiffMetadata parsed;
    if (keyDirectory != nullptr) {
        const auto& data = keyDirectory->data;
        if (data.size() < 8) {
            error = "LAS GeoTIFF key directory is truncated";
            return false;
        }
        const auto keyCount = ReadLittle<std::uint16_t>(data, 6);
        const auto expectedSize = std::size_t{8} +
                                  static_cast<std::size_t>(keyCount) * 8;
        if (data.size() != expectedSize) {
            error = "LAS GeoTIFF key directory is truncated";
            return false;
        }
        parsed.keyDirectoryVersion = ReadLittle<std::uint16_t>(data, 0);
        parsed.keyRevision = ReadLittle<std::uint16_t>(data, 2);
        parsed.minorRevision = ReadLittle<std::uint16_t>(data, 4);
        parsed.keys.reserve(keyCount);
        for (std::uint16_t index = 0; index < keyCount; ++index) {
            const auto offset = std::size_t{8} +
                                static_cast<std::size_t>(index) * 8;
            parsed.keys.push_back({ReadLittle<std::uint16_t>(data, offset),
                                   ReadLittle<std::uint16_t>(data, offset + 2),
                                   ReadLittle<std::uint16_t>(data, offset + 4),
                                   ReadLittle<std::uint16_t>(data, offset + 6)});
        }
    }
    if (doubleParameters != nullptr) {
        const auto& data = doubleParameters->data;
        if (data.size() % sizeof(double) != 0) {
            error = "LAS GeoTIFF double parameters are truncated";
            return false;
        }
        parsed.doubleParameters.reserve(data.size() / sizeof(double));
        for (std::size_t offset = 0; offset < data.size();
             offset += sizeof(double)) {
            parsed.doubleParameters.push_back(ReadLittle<double>(data, offset));
        }
    }
    if (asciiParameters != nullptr) {
        parsed.asciiParameters.assign(
            reinterpret_cast<const char*>(asciiParameters->data.data()),
            asciiParameters->data.size());
        while (!parsed.asciiParameters.empty() &&
               parsed.asciiParameters.back() == '\0') {
            parsed.asciiParameters.pop_back();
        }
    }
    metadata = std::move(parsed);
    return true;
}

bool ParseExtraBytesRecords(
    const std::vector<usdlas::LasVariableLengthRecord>& records,
    std::vector<usdlas::LasExtraBytesDescriptor>& descriptors,
    std::string& error) {
    descriptors.clear();
    for (const auto& record : records) {
        if (!IsExtraBytesRecord(record)) {
            continue;
        }
        if (record.data.size() % kExtraBytesDescriptorSize != 0) {
            error = "LAS Extra Bytes VLR has an invalid length";
            return false;
        }
        for (std::size_t offset = 0; offset < record.data.size();
             offset += kExtraBytesDescriptorSize) {
            const auto& data = record.data;
            usdlas::LasExtraBytesDescriptor descriptor;
            descriptor.dataType = ReadLittle<std::uint8_t>(data, offset + 2);
            descriptor.options = ReadLittle<std::uint8_t>(data, offset + 3);
            descriptor.name = ReadText(data, offset + 4, 32);
            descriptor.noData = {ReadLittle<double>(data, offset + 40),
                                 ReadLittle<double>(data, offset + 48),
                                 ReadLittle<double>(data, offset + 56)};
            descriptor.minimum = {ReadLittle<double>(data, offset + 64),
                                  ReadLittle<double>(data, offset + 72),
                                  ReadLittle<double>(data, offset + 80)};
            descriptor.maximum = {ReadLittle<double>(data, offset + 88),
                                  ReadLittle<double>(data, offset + 96),
                                  ReadLittle<double>(data, offset + 104)};
            descriptor.scale = {ReadLittle<double>(data, offset + 112),
                                ReadLittle<double>(data, offset + 120),
                                ReadLittle<double>(data, offset + 128)};
            descriptor.offset = {ReadLittle<double>(data, offset + 136),
                                 ReadLittle<double>(data, offset + 144),
                                 ReadLittle<double>(data, offset + 152)};
            descriptor.description = ReadText(data, offset + 160, 32);
            descriptors.push_back(std::move(descriptor));
        }
    }
    return true;
}

bool ReadFileRange(std::ifstream& stream,
                   std::uint64_t offset,
                   std::size_t size,
                   std::vector<std::uint8_t>& bytes,
                   std::string& error,
                   RangeReadFailure& failure) {
    failure = RangeReadFailure::None;
    if (offset > static_cast<std::uint64_t>(
                     (std::numeric_limits<std::streamoff>::max)())) {
        failure = RangeReadFailure::InvalidOffset;
        error = "LAS byte range offset is invalid";
        return false;
    }
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!stream) {
        failure = RangeReadFailure::Seek;
        error = "LAS byte range seek failed";
        return false;
    }
    bytes.resize(size);
    if (!bytes.empty() &&
        !stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()))) {
        failure = RangeReadFailure::Read;
        error = "LAS byte range read failed";
        return false;
    }
    return true;
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

LasReader::LasReader(std::string filename) : filename_(std::move(filename)) {}

LasReadFailure LasReader::FailureKind() const noexcept {
    return failureKind_;
}

bool LasReader::ReadMetadata(
    LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics,
    std::size_t memoryBudgetBytes) {
    diagnostics.clear();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;

    std::ifstream stream(filename_, std::ios::binary | std::ios::ate);
    if (!stream) {
        failureKind_ = LasReadFailure::FileOpen;
        diagnostics.push_back({usdgeo::DiagnosticCode::DecodeFailure,
                               usdgeo::Severity::Error,
                               "could not open LAS file: " + filename_,
                               std::nullopt, std::nullopt});
        return false;
    }
    const auto fileSizePosition = stream.tellg();
    if (fileSizePosition < 0) {
        failureKind_ = LasReadFailure::FileSize;
        diagnostics.push_back({usdgeo::DiagnosticCode::DecodeFailure,
                               usdgeo::Severity::Error,
                               "could not determine LAS file size",
                               std::nullopt, std::nullopt});
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(fileSizePosition);
    std::vector<std::uint8_t> bytes;
    RangeReadFailure rangeFailure;
    const auto headerReadSize = (std::min)(fileSize, std::uint64_t{375});
    std::string error;
    if (!ReadFileRange(stream, 0, static_cast<std::size_t>(headerReadSize),
                       bytes, error, rangeFailure) ||
        !InspectHeader(bytes, header, error)) {
        failureKind_ = LasReadFailure::Header;
        AddErrorDiagnostic(error, diagnostics);
        return false;
    }
    if (header.pointDataOffset > fileSize ||
        header.pointDataOffset > memoryBudgetBytes) {
        failureKind_ = header.pointDataOffset > fileSize
                            ? LasReadFailure::Vlr
                            : LasReadFailure::Other;
        error = header.pointDataOffset > fileSize
                    ? "LAS point data offset is outside the file"
                    : "LAS metadata exceeds the configured memory budget";
        AddErrorDiagnostic(error, diagnostics);
        return false;
    }
    if (!ReadFileRange(stream, 0, static_cast<std::size_t>(header.pointDataOffset),
                       bytes, error, rangeFailure)) {
        failureKind_ = LasReadFailure::Vlr;
        AddErrorDiagnostic(error, diagnostics);
        return false;
    }
    if (!InspectRecords(bytes, header.headerSize,
                        header.variableLengthRecordCount, false,
                        header.variableLengthRecords, diagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset > fileSize) {
            failureKind_ = LasReadFailure::EvlrOffset;
            error = "LAS extended variable-length record offset is invalid";
            AddErrorDiagnostic(error, diagnostics);
            return false;
        }
        const auto extendedByteCount =
            fileSize - header.firstExtendedVariableLengthRecordOffset;
        if (extendedByteCount > memoryBudgetBytes - header.pointDataOffset) {
            failureKind_ = LasReadFailure::Other;
            error = "LAS metadata exceeds the configured memory budget";
            AddErrorDiagnostic(error, diagnostics);
            return false;
        }
        std::vector<std::uint8_t> extendedBytes;
        if (!ReadFileRange(stream, header.firstExtendedVariableLengthRecordOffset,
                           static_cast<std::size_t>(extendedByteCount),
                           extendedBytes, error, rangeFailure)) {
            failureKind_ = LasReadFailure::Evlr;
            AddErrorDiagnostic(error, diagnostics);
            return false;
        }
        if (!InspectRecords(extendedBytes, 0,
                            header.extendedVariableLengthRecordCount, true,
                            header.variableLengthRecords, diagnostics)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
    }
    if (!ParseKnownMetadata(header.variableLengthRecords, header, diagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    return true;
}

bool LasReader::ReadPoints(const LasReadOptions& options,
                           const LasPointChunkConsumer& consume,
                           const LasHeader& header,
                           std::string& error) {
    error.clear();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;
    if (!options.IsValid() || !consume) {
        failureKind_ = LasReadFailure::InvalidRequest;
        error = "LAS read options or consumer are invalid";
        return false;
    }

    std::ifstream stream(filename_, std::ios::binary | std::ios::ate);
    if (!stream) {
        failureKind_ = LasReadFailure::FileOpen;
        error = "could not open LAS file: " + filename_;
        return false;
    }
    const auto fileSizePosition = stream.tellg();
    if (fileSizePosition < 0 ||
        static_cast<std::uintmax_t>(fileSizePosition) >
            (std::numeric_limits<std::uint64_t>::max)()) {
        failureKind_ = LasReadFailure::FileSize;
        error = "could not determine LAS file size";
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(fileSizePosition);
    const auto recordLength = static_cast<std::uint64_t>(
        header.pointRecordLength);
    RangeReadFailure rangeFailure;
    std::vector<std::uint8_t> bytes;
    if (recordLength == 0 ||
        header.pointCount > (fileSize - header.pointDataOffset) / recordLength) {
        failureKind_ = LasReadFailure::PointDataTruncated;
        error = "LAS point data is truncated";
        return false;
    }
    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount >
             header.pointCount - options.range.firstPoint)) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS point range is outside the header";
        return false;
    }

    const auto rangeEnd = options.range.pointCount == 0
                              ? header.pointCount
                              : options.range.firstPoint +
                                    options.range.pointCount;
    if (recordLength >
        (std::numeric_limits<std::size_t>::max)() / 2 - sizeof(LasPoint)) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS point record size is invalid";
        return false;
    }
    const auto bytesPerPoint = static_cast<std::size_t>(recordLength) * 2 +
                               sizeof(LasPoint);
    const auto budgetPointLimit = options.memoryBudgetBytes / bytesPerPoint;
    const auto maximumPoints =
        (std::min)(options.chunkPointLimit, budgetPointLimit);
    if (maximumPoints == 0) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS memory budget is too small for one point";
        return false;
    }

    std::uint64_t pointsRead = options.range.firstPoint;
    std::uint64_t selectedPointsRead = 0;
    while (pointsRead < rangeEnd) {
        if (options.isCancelled && options.isCancelled()) {
            failureKind_ = LasReadFailure::Other;
            error = "LAS read cancelled";
            return false;
        }
        const auto remaining = rangeEnd - pointsRead;
        const auto count = (std::min)(
            static_cast<std::uint64_t>(maximumPoints), remaining);
        const auto byteOffset = header.pointDataOffset +
                                pointsRead * recordLength;
        const auto byteCount = static_cast<std::size_t>(count * recordLength);
        if (!ReadFileRange(stream, byteOffset, byteCount, bytes, error,
                           rangeFailure)) {
            failureKind_ = rangeFailure == RangeReadFailure::Seek
                               ? LasReadFailure::PointDataSeek
                               : LasReadFailure::PointDataRead;
            failureByteOffset_ = byteOffset;
            return false;
        }

        std::vector<LasPoint> points;
        points.reserve(static_cast<std::size_t>(count));
        for (std::size_t index = 0; index < static_cast<std::size_t>(count);
             ++index) {
            std::vector<std::uint8_t> record(
                bytes.begin() + index * static_cast<std::size_t>(recordLength),
                bytes.begin() + (index + 1) *
                                    static_cast<std::size_t>(recordLength));
            LasPoint point;
            if (!DecodePoint(header, record, point, error)) {
                failureKind_ = LasReadFailure::PointDecode;
                failureByteOffset_ = byteOffset +
                                     static_cast<std::uint64_t>(index) *
                                         recordLength;
                failurePointIndex_ = pointsRead + index;
                return false;
            }
            points.push_back(point);
        }

        const auto chunkStart = pointsRead;
        pointsRead += count;
        const auto selectedStart =
            (std::max)(chunkStart, options.range.firstPoint);
        const auto selectedEnd = (std::min)(pointsRead, rangeEnd);
        if (selectedStart < selectedEnd) {
            const auto first = static_cast<std::size_t>(selectedStart -
                                                         chunkStart);
            const auto last = static_cast<std::size_t>(selectedEnd -
                                                        chunkStart);
            points.erase(points.begin(), points.begin() + first);
            points.erase(points.begin() + (last - first), points.end());
            selectedPointsRead += points.size();
            if (!consume(header, points)) {
                failureKind_ = LasReadFailure::Other;
                error = "LAS chunk consumer rejected a chunk";
                return false;
            }
        }
    }
    if (selectedPointsRead != rangeEnd - options.range.firstPoint) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS reader point count does not match the range";
        return false;
    }
    return true;
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkConsumer& consume,
                     LasHeader& header,
                     std::string& error) {
    error.clear();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;
    if (!options.IsValid() || !consume) {
        failureKind_ = LasReadFailure::InvalidRequest;
        error = "LAS read options or consumer are invalid";
        return false;
    }

    std::ifstream stream(filename_, std::ios::binary | std::ios::ate);
    if (!stream) {
        failureKind_ = LasReadFailure::FileOpen;
        error = "could not open LAS file: " + filename_;
        return false;
    }
    const auto fileSizePosition = stream.tellg();
    if (fileSizePosition < 0 ||
        static_cast<std::uintmax_t>(fileSizePosition) >
            (std::numeric_limits<std::uint64_t>::max)()) {
        failureKind_ = LasReadFailure::FileSize;
        error = "could not determine LAS file size";
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(fileSizePosition);

    std::vector<std::uint8_t> bytes;
    const auto headerReadSize = (std::min)(fileSize, std::uint64_t{375});
    RangeReadFailure rangeFailure;
    if (!ReadFileRange(stream, 0, static_cast<std::size_t>(headerReadSize),
                       bytes, error, rangeFailure)) {
        failureKind_ = LasReadFailure::Header;
        return false;
    }
    if (!InspectHeader(bytes, header, error)) {
        failureKind_ = LasReadFailure::Header;
        return false;
    }

    if (header.pointDataOffset > fileSize) {
        failureKind_ = LasReadFailure::PointDataTruncated;
        error = "LAS point data offset is outside the file";
        return false;
    }
    if (!ReadFileRange(stream, 0, static_cast<std::size_t>(header.pointDataOffset),
                       bytes, error, rangeFailure)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (!InspectRecords(bytes, header.headerSize,
                        header.variableLengthRecordCount, false,
                        header.variableLengthRecords, error)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset > fileSize) {
            failureKind_ = LasReadFailure::EvlrOffset;
            error = "LAS extended variable-length record offset is invalid";
            return false;
        }
        std::vector<std::uint8_t> extendedBytes;
        if (!ReadFileRange(
                stream, header.firstExtendedVariableLengthRecordOffset,
                static_cast<std::size_t>(
                    fileSize - header.firstExtendedVariableLengthRecordOffset),
                extendedBytes, error, rangeFailure)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
        if (!InspectRecords(extendedBytes, 0,
                            header.extendedVariableLengthRecordCount, true,
                            header.variableLengthRecords, error)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
    }
    if (!ParseKnownMetadata(header.variableLengthRecords, header, error)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    return ReadPoints(options, consume, header, error);
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkConsumer& consume,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (Read(options, consume, header, error)) {
        return true;
    }
    diagnostics.push_back({CodeForError(error), usdgeo::Severity::Error, error,
                           failureByteOffset_, failurePointIndex_});
    return false;
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkErrorConsumer& consume,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string callbackError;
    const LasPointChunkConsumer bridge =
        [&](const LasHeader& chunkHeader,
            const std::vector<LasPoint>& points) {
            callbackError.clear();
            return consume(chunkHeader, points, callbackError);
        };
    if (Read(options, bridge, header, diagnostics)) {
        return true;
    }
    if (!callbackError.empty() && !diagnostics.empty()) {
        diagnostics.front().message = callbackError;
    }
    return false;
}

namespace {

std::size_t EffectiveChunkPointLimit(const LasHeader& header,
                                     const LasReadOptions& options) {
    const auto recordLength = static_cast<std::uint64_t>(
        header.pointRecordLength);
    if (recordLength == 0 ||
        recordLength >
            (std::numeric_limits<std::size_t>::max)() / 2 - sizeof(LasPoint)) {
        return 0;
    }
    const auto bytesPerPoint = static_cast<std::size_t>(recordLength) * 2 +
                               sizeof(LasPoint);
    return (std::min)(options.chunkPointLimit,
                      options.memoryBudgetBytes / bytesPerPoint);
}

void AddStreamDiagnostic(usdgeo::DiagnosticCode code,
                         const std::string& message,
                         std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, std::nullopt, std::nullopt});
}

} // namespace

LasPointStream::LasPointStream(std::string filename,
                               LasReadOptions options,
                               LasHeader header,
                               std::size_t effectiveChunkPointLimit)
    : filename_(std::move(filename)),
                                reader_(filename_),
      options_(std::move(options)),
      header_(std::move(header)),
      nextPoint_(options_.range.firstPoint),
      endPoint_(options_.range.pointCount == 0
                    ? header_.pointCount
                    : options_.range.firstPoint + options_.range.pointCount),
      effectiveChunkPointLimit_(effectiveChunkPointLimit) {}

LasPointStream::~LasPointStream() = default;

usdpointcloud::PointStreamStatus LasPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    diagnostic = {};
    chunk = {};
    data = {};
    if (nextPoint_ >= endPoint_) {
        return usdpointcloud::PointStreamStatus::End;
    }

    const auto count = (std::min)(
        static_cast<std::uint64_t>(effectiveChunkPointLimit_),
        endPoint_ - nextPoint_);
    LasReadOptions chunkOptions = options_;
    chunkOptions.chunkPointLimit = effectiveChunkPointLimit_;
    chunkOptions.range = {nextPoint_, count};
    usdpointcloud::PointData pointData;
    std::string callbackError;
    const auto consume = [&](const LasHeader& callbackHeader,
                             const std::vector<LasPoint>& points) {
        callbackError.clear();
        if (!AppendPointData(callbackHeader, points, filename_, pointData,
                             callbackError)) {
            return false;
        }
        return true;
    };
    std::string error;
    if (!reader_.ReadPoints(chunkOptions, consume, header_, error)) {
        failureKind_ = reader_.FailureKind();
        diagnostic = {CodeForError(error), usdgeo::Severity::Error,
                      callbackError.empty() ? error : callbackError,
                      reader_.failureByteOffset_, reader_.failurePointIndex_};
        return usdpointcloud::PointStreamStatus::Error;
    }

    data = std::move(pointData);
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    for (const auto& position : data.positions) {
        bounds.Expand(position);
    }
    chunk = usdpointcloud::MakePointChunk(data, bounds);
    if (!chunk.IsValid() || chunk.pointCount == 0) {
        failureKind_ = LasReadFailure::Other;
        diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                      usdgeo::Severity::Error,
                      "LAS stream produced an invalid point chunk",
                      std::nullopt, nextPoint_};
        return usdpointcloud::PointStreamStatus::Error;
    }
    nextPoint_ += chunk.pointCount;
    return usdpointcloud::PointStreamStatus::Chunk;
}

const LasHeader& LasPointStream::Header() const noexcept {
    return header_;
}

LasReadFailure LasPointStream::FailureKind() const noexcept {
    return failureKind_;
}

std::unique_ptr<LasPointStream> OpenLasPointStream(
    const std::string& filename,
    const LasReadOptions& options,
    LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    header = {};
    if (!options.IsValid()) {
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "LAS stream options are invalid", diagnostics);
        return nullptr;
    }

    LasReader reader(filename);
    if (!reader.ReadMetadata(header, diagnostics, options.memoryBudgetBytes)) {
        return nullptr;
    }
    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount >
             header.pointCount - options.range.firstPoint)) {
        diagnostics.clear();
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidPointSourceRange,
                            "LAS point range is outside the header",
                            diagnostics);
        return nullptr;
    }

    const auto effectiveChunkPointLimit =
        EffectiveChunkPointLimit(header, options);
    if (effectiveChunkPointLimit == 0) {
        diagnostics.clear();
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "LAS memory budget is too small for one point",
                            diagnostics);
        return nullptr;
    }
    return std::unique_ptr<LasPointStream>(new LasPointStream(
        filename, options, header, effectiveChunkPointLimit));
}

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
    return ParseKnownMetadata(header.variableLengthRecords, header, error);
}

bool InspectRecords(const std::vector<std::uint8_t>& bytes,
                    std::size_t offset,
                    std::uint32_t count,
                    bool extended,
                    std::vector<LasVariableLengthRecord>& records,
                    std::string& error) {
    return ReadRecords(bytes, offset, count, extended, records, error);
}

bool ParseKnownMetadata(const std::vector<LasVariableLengthRecord>& records,
                        LasHeader& header,
                        std::string& error) {
    error.clear();
    header.crsWkt = ExtractWktCrs(records);
    if (!ParseGeoTiffRecords(records, header.geoTiffMetadata, error)) {
        return false;
    }
    if (!ParseExtraBytesRecords(records, header.extraBytes, error)) {
        return false;
    }
    return ValidateExtraBytesLayout(header, error);
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
        const auto flags = ReadLittle<std::uint8_t>(record, 15);
        point.classificationFlags = flags & 0x0f;
        point.scannerChannel = (flags >> 4) & 0x03;
        point.scanDirectionFlag = (flags >> 6) & 0x01;
        point.edgeOfFlightLine = (flags >> 7) & 0x01;
        point.userData = ReadLittle<std::uint8_t>(record, 17);
        point.scanAngle = ReadLittle<std::int16_t>(record, 18);
        point.pointSourceId = ReadLittle<std::uint16_t>(record, 20);
        if (header.pointFormat >= 7) {
            point.red = ReadLittle<std::uint16_t>(record, 30);
            point.green = ReadLittle<std::uint16_t>(record, 32);
            point.blue = ReadLittle<std::uint16_t>(record, 34);
            point.hasColor = true;
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            point.nir = ReadLittle<std::uint16_t>(record, 36);
        }
        point.gpsTime = ReadLittle<double>(record, 22);
        point.hasGpsTime = true;
    } else {
        point.scanAngle = static_cast<std::int8_t>(
            ReadLittle<std::uint8_t>(record, 16));
        point.userData = ReadLittle<std::uint8_t>(record, 17);
        point.pointSourceId = ReadLittle<std::uint16_t>(record, 18);
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
    if (header.pointFormat == 4 || header.pointFormat == 5 ||
        header.pointFormat == 9 || header.pointFormat == 10) {
        const auto waveformOffset = header.pointFormat == 4 ? std::size_t{28}
                                    : header.pointFormat == 5 ? std::size_t{34}
                                    : header.pointFormat == 9 ? std::size_t{30}
                                                               : std::size_t{38};
        const auto descriptorIndex =
            ReadLittle<std::uint8_t>(record, waveformOffset);
        const auto encodedOffset =
            ReadLittle<std::uint64_t>(record, waveformOffset + 1);
        point.waveform.descriptorIndex = descriptorIndex & 0x7f;
        point.waveform.external = (descriptorIndex & 0x80) != 0;
        point.waveform.dataOffset = encodedOffset;
        point.waveform.packetSize =
            ReadLittle<std::uint32_t>(record, waveformOffset + 9);
        point.waveform.returnPointLocation =
            ReadLittle<float>(record, waveformOffset + 13);
        point.waveform.xt = ReadLittle<float>(record, waveformOffset + 17);
        point.waveform.yt = ReadLittle<float>(record, waveformOffset + 21);
        point.waveform.zt = ReadLittle<float>(record, waveformOffset + 25);
        point.hasWaveform = true;
    }
    const auto extraOffset = MinimumRecordLength(header.pointFormat);
    std::size_t extraBytesOffset = extraOffset;
    point.extraBytes.reserve(header.extraBytes.size());
    for (const auto& descriptor : header.extraBytes) {
        const auto scalarSize = ExtraByteScalarSize(descriptor.dataType);
        const auto componentCount =
            ExtraByteComponentCount(descriptor.dataType);
        if (scalarSize == 0 || componentCount == 0) {
            error = "unsupported LAS Extra Bytes data type";
            return false;
        }
        if (!Has(record, extraBytesOffset, scalarSize * componentCount)) {
            error = "LAS point record Extra Bytes are truncated";
            return false;
        }
        const auto scalarDataType = descriptor.dataType >= 11
                                        ? (descriptor.dataType - 1) % 10 + 1
                                        : descriptor.dataType;
        for (std::uint8_t component = 0; component < componentCount;
             ++component) {
            const auto componentOffset =
                extraBytesOffset + scalarSize * component;
            if (scalarDataType == 7 &&
                ReadLittle<std::uint64_t>(record, componentOffset) >
                    (std::uint64_t{1} << 53)) {
                error = "LAS Extra Bytes integer cannot be represented exactly as double";
                return false;
            }
            if (scalarDataType == 8) {
                const auto rawInteger =
                    ReadLittle<std::int64_t>(record, componentOffset);
                if (rawInteger > (std::int64_t{1} << 53) ||
                    rawInteger < -(std::int64_t{1} << 53)) {
                    error = "LAS Extra Bytes integer cannot be represented exactly as double";
                    return false;
                }
            }
            const auto raw = ReadExtraByteScalar(record, componentOffset,
                                                  descriptor.dataType);
            const auto scale = component == 0 ? descriptor.scale.x
                               : component == 1 ? descriptor.scale.y
                                                : descriptor.scale.z;
            const auto offset = component == 0 ? descriptor.offset.x
                                : component == 1 ? descriptor.offset.y
                                                 : descriptor.offset.z;
            const auto value = raw * scale + offset;
            if (!std::isfinite(value)) {
                error = "decoded LAS point contains a non-finite Extra Bytes value";
                return false;
            }
            point.extraBytes.push_back(value);
        }
        extraBytesOffset += scalarSize * componentCount;
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
    if (!diagnostics.empty()) {
        diagnostics.front().byteOffset = offset;
    }
    return false;
}

bool ParseKnownMetadata(const std::vector<LasVariableLengthRecord>& records,
                        LasHeader& header,
                        std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (ParseKnownMetadata(records, header, error)) {
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

bool AppendPointData(const LasHeader& header,
                     const std::vector<LasPoint>& points,
                     const std::string& sourceFilename,
                     usdpointcloud::PointData& data,
                     std::string& error) {
    if (!header.IsValid() || !data.IsValid() ||
        data.positions.size() > header.pointCount ||
        points.size() > header.pointCount - data.positions.size()) {
        error = "LAS point data does not match the header";
        return false;
    }

    if (data.positions.empty()) {
        const auto pointCount = points.size();
        data.positions.reserve(pointCount);
        data.intensity.reserve(pointCount);
        data.returnNumber.reserve(pointCount);
        data.numberOfReturns.reserve(pointCount);
        data.classification.reserve(pointCount);
        if (header.pointFormat >= 6) {
            data.classificationFlags.reserve(pointCount);
            data.scannerChannel.reserve(pointCount);
        }
        data.scanDirectionFlag.reserve(pointCount);
        data.edgeOfFlightLine.reserve(pointCount);
        data.userData.reserve(pointCount);
        data.scanAngle.reserve(pointCount);
        data.pointSourceId.reserve(pointCount);
        if (header.pointFormat == 2 || header.pointFormat == 3 ||
            header.pointFormat == 7 || header.pointFormat == 8) {
            data.red.reserve(pointCount);
            data.green.reserve(pointCount);
            data.blue.reserve(pointCount);
        }
        if (header.pointFormat == 1 || header.pointFormat == 3 ||
            header.pointFormat >= 6) {
            data.gpsTime.reserve(pointCount);
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            data.nir.reserve(pointCount);
        }
        if (header.pointFormat == 4 || header.pointFormat == 5 ||
            header.pointFormat == 9 || header.pointFormat == 10) {
            data.waveformDescriptorIndex.reserve(pointCount);
            data.waveformDataOffset.reserve(pointCount);
            data.waveformPacketSize.reserve(pointCount);
            data.returnPointWaveformLocation.reserve(pointCount);
            data.waveformXt.reserve(pointCount);
            data.waveformYt.reserve(pointCount);
            data.waveformZt.reserve(pointCount);
            data.waveformDataExternal.reserve(pointCount);
        }
        data.extraByteNames.clear();
        data.extraByteNames.reserve(header.extraBytes.size());
        data.extraByteComponentCounts.clear();
        data.extraByteComponentCounts.reserve(header.extraBytes.size());
        data.extraBytes.clear();
        data.extraBytes.resize(header.extraBytes.size());
        std::vector<std::string> extraByteNames;
        extraByteNames.reserve(header.extraBytes.size());
        for (const auto& descriptor : header.extraBytes) {
            extraByteNames.push_back(descriptor.name);
            data.extraByteComponentCounts.push_back(
                ExtraByteComponentCount(descriptor.dataType));
        }
        data.extraByteNames =
            usdpointcloud::NormalizeExtraByteNames(extraByteNames);
        for (auto& values : data.extraBytes) {
            values.reserve(pointCount);
        }
    }

    for (const auto& point : points) {
        data.positions.push_back(point.sourcePosition);
        data.intensity.push_back(point.intensity);
        data.returnNumber.push_back(point.returnNumber);
        data.numberOfReturns.push_back(point.numberOfReturns);
        data.classification.push_back(point.classification);
        if (header.pointFormat >= 6) {
            data.classificationFlags.push_back(point.classificationFlags);
            data.scannerChannel.push_back(point.scannerChannel);
        }
        data.scanDirectionFlag.push_back(point.scanDirectionFlag);
        data.edgeOfFlightLine.push_back(point.edgeOfFlightLine);
        data.userData.push_back(point.userData);
        data.scanAngle.push_back(point.scanAngle);
        data.pointSourceId.push_back(point.pointSourceId);
        if (point.hasColor) {
            data.red.push_back(point.red);
            data.green.push_back(point.green);
            data.blue.push_back(point.blue);
        }
        if (point.hasGpsTime) {
            data.gpsTime.push_back(point.gpsTime);
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            data.nir.push_back(point.nir);
        }
        if (point.hasWaveform) {
            data.waveformDescriptorIndex.push_back(point.waveform.descriptorIndex);
            data.waveformDataOffset.push_back(point.waveform.dataOffset);
            data.waveformPacketSize.push_back(point.waveform.packetSize);
            data.returnPointWaveformLocation.push_back(
                point.waveform.returnPointLocation);
            data.waveformXt.push_back(point.waveform.xt);
            data.waveformYt.push_back(point.waveform.yt);
            data.waveformZt.push_back(point.waveform.zt);
            data.waveformDataExternal.push_back(point.waveform.external ? 1 : 0);
            if (point.waveform.external && data.waveformDataFile.empty()) {
                auto waveformPath = std::filesystem::path(sourceFilename);
                waveformPath.replace_extension(".wdp");
                data.waveformDataFile = waveformPath.string();
            }
        }
        std::size_t extraByteValueCount = 0;
        for (const auto componentCount : data.extraByteComponentCounts) {
            extraByteValueCount += componentCount;
        }
        if (point.extraBytes.size() != extraByteValueCount) {
            error = "LAS point Extra Bytes do not match the header";
            return false;
        }
        std::size_t pointExtraByteIndex = 0;
        for (std::size_t index = 0; index < data.extraBytes.size(); ++index) {
            const auto componentCount = data.extraByteComponentCounts[index];
            data.extraBytes[index].insert(
                data.extraBytes[index].end(),
                point.extraBytes.begin() + pointExtraByteIndex,
                point.extraBytes.begin() + pointExtraByteIndex + componentCount);
            pointExtraByteIndex += componentCount;
        }
    }

    if (!data.IsValid()) {
        error = "LAS point attributes have inconsistent lengths";
        return false;
    }
    return true;
}

bool BuildPointCloudAsset(const LasHeader& header,
                          const usdpointcloud::PointData& data,
                          const std::string& missingCrsMessage,
                          usdpointcloud::PointCloudAsset& asset,
                          std::string& error) {
    if (!header.IsValid() || !data.IsValid() ||
        data.positions.empty() || data.positions.size() > header.pointCount) {
        error = "LAS point data does not match the header";
        return false;
    }

    asset = {};
    asset.reference.wkt = header.crsWkt.empty() ? missingCrsMessage
                                                 : header.crsWkt;
    asset.reference.sourceUpAxis = "Z";
    asset.reference.stageUpAxis = "Y";
    asset.reference.localOrigin = header.bounds.minimum;
    usdgeo::SpatialBounds sourceBounds = usdgeo::SpatialBounds::Empty();
    for (const auto& position : data.positions) {
        sourceBounds.Expand(position);
    }
    if (!asset.reference.TryToLocal(sourceBounds, asset.bounds)) {
        error = "LAS bounds could not be transformed to local coordinates";
        return false;
    }
    asset.data = data;
    asset.chunk = usdpointcloud::MakePointChunk(asset.data, asset.bounds);
    if (!asset.IsValid()) {
        error = "LAS point cloud asset is invalid";
        return false;
    }
    return true;
}

bool BuildPointCloudMetadata(const LasHeader& header,
                             usdpointcloud::PointChunk& chunk,
                             usdgeo::GeoReference& reference,
                             usdgeo::SpatialBounds& bounds,
                             std::string& error) {
    if (!header.IsValid()) {
        error = "LAS metadata does not contain a valid point cloud";
        return false;
    }
    reference = {};
    reference.wkt = header.crsWkt.empty()
                        ? "LAS CRS unavailable; inspect VLR metadata"
                        : header.crsWkt;
    reference.sourceUpAxis = "Z";
    reference.stageUpAxis = "Y";
    reference.localOrigin = header.bounds.minimum;
    if (!reference.TryToLocal(header.bounds, bounds)) {
        error = "LAS bounds could not be transformed to local coordinates";
        return false;
    }
    chunk = {};
    chunk.pointCount = header.pointCount;
    chunk.bounds = bounds;
    chunk.attributes = {{"xyz", usdpointcloud::PointAttributeType::Float64}};
    chunk.attributes.push_back({"intensity", usdpointcloud::PointAttributeType::UInt16});
    chunk.attributes.push_back({"returnNumber", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"numberOfReturns", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"classification", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"scanDirectionFlag", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"edgeOfFlightLine", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"userData", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back({"scanAngle", usdpointcloud::PointAttributeType::Int16});
    chunk.attributes.push_back({"pointSourceId", usdpointcloud::PointAttributeType::UInt16});
    if (header.pointFormat >= 6) {
        chunk.attributes.push_back({"classificationFlags", usdpointcloud::PointAttributeType::UInt8});
        chunk.attributes.push_back({"scannerChannel", usdpointcloud::PointAttributeType::UInt8});
    }
    if (header.pointFormat == 2 || header.pointFormat == 3 ||
        header.pointFormat == 7 || header.pointFormat == 8) {
        chunk.attributes.push_back({"red", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back({"green", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back({"blue", usdpointcloud::PointAttributeType::UInt16});
    }
    if (header.pointFormat == 1 || header.pointFormat == 3 ||
        header.pointFormat >= 6) {
        chunk.attributes.push_back({"gpsTime", usdpointcloud::PointAttributeType::Float64});
    }
    if (header.pointFormat == 8 || header.pointFormat == 10) {
        chunk.attributes.push_back({"nir", usdpointcloud::PointAttributeType::UInt16});
    }
    if (header.pointFormat == 4 || header.pointFormat == 5 ||
        header.pointFormat == 9 || header.pointFormat == 10) {
        chunk.attributes.push_back({"waveformDescriptorIndex", usdpointcloud::PointAttributeType::UInt8});
        chunk.attributes.push_back({"waveformDataOffset", usdpointcloud::PointAttributeType::UInt64});
        chunk.attributes.push_back({"waveformPacketSize", usdpointcloud::PointAttributeType::UInt32});
        chunk.attributes.push_back({"returnPointWaveformLocation", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back({"waveformXt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back({"waveformYt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back({"waveformZt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back({"waveformDataExternal", usdpointcloud::PointAttributeType::UInt8});
    }
    std::vector<std::string> extraByteNames;
    extraByteNames.reserve(header.extraBytes.size());
    for (const auto& extra : header.extraBytes) {
        extraByteNames.push_back(extra.name);
    }
    const auto normalizedExtraByteNames =
        usdpointcloud::NormalizeExtraByteNames(extraByteNames);
    for (std::size_t index = 0; index < extraByteNames.size(); ++index) {
        const auto componentCount =
            ExtraByteComponentCount(header.extraBytes[index].dataType);
        if (componentCount == 0) {
            error = "unsupported LAS Extra Bytes data type";
            return false;
        }
        const auto type = componentCount == 1
                              ? usdpointcloud::PointAttributeType::Float64
                          : componentCount == 2
                              ? usdpointcloud::PointAttributeType::Float64Vec2
                              : usdpointcloud::PointAttributeType::Float64Vec3;
        chunk.attributes.push_back(
            {normalizedExtraByteNames[index], type});
    }
    return chunk.IsValid();
}

} // namespace usdlas