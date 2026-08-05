#include "LasInternal.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace {

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
        const auto keyCount = usdlas::detail::ReadLittle<std::uint16_t>(data, 6);
        const auto expectedSize = std::size_t{8} +
                                  static_cast<std::size_t>(keyCount) * 8;
        if (data.size() != expectedSize) {
            error = "LAS GeoTIFF key directory is truncated";
            return false;
        }
        parsed.keyDirectoryVersion =
            usdlas::detail::ReadLittle<std::uint16_t>(data, 0);
        parsed.keyRevision = usdlas::detail::ReadLittle<std::uint16_t>(data, 2);
        parsed.minorRevision =
            usdlas::detail::ReadLittle<std::uint16_t>(data, 4);
        parsed.keys.reserve(keyCount);
        for (std::uint16_t index = 0; index < keyCount; ++index) {
            const auto offset = std::size_t{8} +
                                static_cast<std::size_t>(index) * 8;
            parsed.keys.push_back(
                {usdlas::detail::ReadLittle<std::uint16_t>(data, offset),
                 usdlas::detail::ReadLittle<std::uint16_t>(data, offset + 2),
                 usdlas::detail::ReadLittle<std::uint16_t>(data, offset + 4),
                 usdlas::detail::ReadLittle<std::uint16_t>(data, offset + 6)});
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
            parsed.doubleParameters.push_back(
                usdlas::detail::ReadLittle<double>(data, offset));
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

bool IsWktIdentifierCharacter(char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '_';
}

std::optional<int> ParseEpsgCodeInRange(const std::string& wkt,
                                       std::size_t begin,
                                       std::size_t end) {
    for (std::size_t offset = begin; offset < end;) {
        const auto marker = wkt.find("EPSG", offset);
        if (marker == std::string::npos || marker >= end) {
            break;
        }
        offset = marker + 4;
        std::size_t separator = offset;
        while (separator < end) {
            const auto character = wkt[separator];
            if (character != ' ' && character != '\t' && character != '\r' &&
                character != '\n' && character != ':' && character != '"' &&
                character != '[' && character != ']' && character != ',') {
                break;
            }
            ++separator;
        }
        if (separator == offset || separator >= end ||
            wkt[separator] < '0' || wkt[separator] > '9') {
            continue;
        }
        long long value = 0;
        while (separator < end && wkt[separator] >= '0' &&
               wkt[separator] <= '9') {
            value = value * 10 + (wkt[separator] - '0');
            if (value > (std::numeric_limits<int>::max)()) {
                value = 0;
                break;
            }
            ++separator;
        }
        if (value != 0) {
            return static_cast<int>(value);
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> FindWktBracketEnd(const std::string& wkt,
                                             std::size_t openBracket) {
    std::size_t depth = 0;
    bool inString = false;
    for (std::size_t index = openBracket; index < wkt.size(); ++index) {
        const auto character = wkt[index];
        if (inString) {
            if (character == '"') {
                if (index + 1 < wkt.size() && wkt[index + 1] == '"') {
                    ++index;
                } else {
                    inString = false;
                }
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '[') {
            ++depth;
        } else if (character == ']') {
            if (depth == 0) {
                return std::nullopt;
            }
            --depth;
            if (depth == 0) {
                return index;
            }
        }
    }
    return std::nullopt;
}

std::optional<int> ExtractWktEpsgCode(const std::string& wkt) {
    const auto rootOpenBracket = wkt.find('[');
    if (rootOpenBracket == std::string::npos) {
        return std::nullopt;
    }
    const auto rootEnd = FindWktBracketEnd(wkt, rootOpenBracket);
    if (!rootEnd.has_value()) {
        return std::nullopt;
    }

    std::size_t depth = 0;
    bool inString = false;
    for (std::size_t index = rootOpenBracket; index < rootEnd.value();
         ++index) {
        const auto character = wkt[index];
        if (inString) {
            if (character == '"') {
                if (index + 1 < rootEnd.value() && wkt[index + 1] == '"') {
                    ++index;
                } else {
                    inString = false;
                }
            }
            continue;
        }
        if (character == '"') {
            inString = true;
            continue;
        }
        if (character == '[') {
            ++depth;
            continue;
        }
        if (character == ']') {
            --depth;
            continue;
        }
        if (depth != 1 ||
            (index != 0 && IsWktIdentifierCharacter(wkt[index - 1]))) {
            continue;
        }

        std::size_t tokenLength = 0;
        if (wkt.compare(index, 9, "AUTHORITY") == 0) {
            tokenLength = 9;
        } else if (wkt.compare(index, 2, "ID") == 0) {
            tokenLength = 2;
        } else {
            continue;
        }
        if (index + tokenLength < rootEnd.value() &&
            IsWktIdentifierCharacter(wkt[index + tokenLength])) {
            continue;
        }
        const auto openBracket = wkt.find('[', index + tokenLength);
        if (openBracket == std::string::npos || openBracket >= rootEnd.value()) {
            continue;
        }
        const auto tokenEnd = FindWktBracketEnd(wkt, openBracket);
        if (!tokenEnd.has_value() || tokenEnd.value() > rootEnd.value()) {
            continue;
        }
        const auto code = ParseEpsgCodeInRange(wkt, index, tokenEnd.value());
        if (code.has_value()) {
            return code;
        }
    }
    return std::nullopt;
}

std::optional<int> GeoTiffEpsgCode(
    const usdlas::LasGeoTiffMetadata& metadata,
    std::string& error) {
    std::optional<int> projected;
    std::optional<int> geographic;
    for (const auto& key : metadata.keys) {
        if (key.keyId != 2048 && key.keyId != 3072) {
            continue;
        }
        if (key.tiffTagLocation != 0 || key.valueOffset == 0) {
            continue;
        }
        auto& target = key.keyId == 3072 ? projected : geographic;
        if (target.has_value() && target.value() != key.valueOffset) {
            error = "LAS GeoTIFF CRS keys conflict";
            return std::nullopt;
        }
        target = static_cast<int>(key.valueOffset);
    }
    return projected.has_value() ? projected : geographic;
}

bool ResolveEpsgCode(const std::string& wkt,
                     const std::optional<usdlas::LasGeoTiffMetadata>& metadata,
                     std::optional<int>& epsgCode,
                     std::string& error) {
    epsgCode = ExtractWktEpsgCode(wkt);
    if (!metadata.has_value()) {
        return true;
    }
    const auto geoTiffCode = GeoTiffEpsgCode(metadata.value(), error);
    if (!error.empty()) {
        return false;
    }
    if (epsgCode.has_value() && geoTiffCode.has_value() &&
        epsgCode.value() != geoTiffCode.value()) {
        error = "LAS CRS definitions conflict";
        return false;
    }
    if (!epsgCode.has_value()) {
        epsgCode = geoTiffCode;
    }
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
            descriptor.dataType =
                usdlas::detail::ReadLittle<std::uint8_t>(data, offset + 2);
            descriptor.options =
                usdlas::detail::ReadLittle<std::uint8_t>(data, offset + 3);
            descriptor.name = usdlas::detail::ReadText(data, offset + 4, 32);
            descriptor.noData = {
                usdlas::detail::ReadLittle<double>(data, offset + 40),
                usdlas::detail::ReadLittle<double>(data, offset + 48),
                usdlas::detail::ReadLittle<double>(data, offset + 56)};
            descriptor.minimum = {
                usdlas::detail::ReadLittle<double>(data, offset + 64),
                usdlas::detail::ReadLittle<double>(data, offset + 72),
                usdlas::detail::ReadLittle<double>(data, offset + 80)};
            descriptor.maximum = {
                usdlas::detail::ReadLittle<double>(data, offset + 88),
                usdlas::detail::ReadLittle<double>(data, offset + 96),
                usdlas::detail::ReadLittle<double>(data, offset + 104)};
            descriptor.scale = {
                usdlas::detail::ReadLittle<double>(data, offset + 112),
                usdlas::detail::ReadLittle<double>(data, offset + 120),
                usdlas::detail::ReadLittle<double>(data, offset + 128)};
            descriptor.offset = {
                usdlas::detail::ReadLittle<double>(data, offset + 136),
                usdlas::detail::ReadLittle<double>(data, offset + 144),
                usdlas::detail::ReadLittle<double>(data, offset + 152)};
            descriptor.description =
                usdlas::detail::ReadText(data, offset + 160, 32);
            descriptors.push_back(std::move(descriptor));
        }
    }
    return true;
}

} // namespace

namespace usdlas {

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::string& error) {
    if (!InspectHeader(bytes, header, error)) {
        return false;
    }
    header.variableLengthRecords.clear();
    if (!detail::ReadRecords(bytes, header.headerSize,
                             header.variableLengthRecordCount, false,
                             header.variableLengthRecords, error)) {
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset == 0 ||
            header.firstExtendedVariableLengthRecordOffset > bytes.size()) {
            error = "LAS extended variable-length record offset is invalid";
            return false;
        }
        if (!detail::ReadRecords(
                bytes,
                static_cast<std::size_t>(
                    header.firstExtendedVariableLengthRecordOffset),
                header.extendedVariableLengthRecordCount, true,
                header.variableLengthRecords, error)) {
            return false;
        }
    }
    return ParseKnownMetadata(header.variableLengthRecords, header, error);
}

bool ParseKnownMetadata(const std::vector<LasVariableLengthRecord>& records,
                        LasHeader& header,
                        std::string& error) {
    error.clear();
    header.crsWkt = ExtractWktCrs(records);
    if (!ParseGeoTiffRecords(records, header.geoTiffMetadata, error)) {
        return false;
    }
    if (!ResolveEpsgCode(header.crsWkt, header.geoTiffMetadata,
                         header.epsgCode, error)) {
        return false;
    }
    if (!ParseExtraBytesRecords(records, header.extraBytes, error)) {
        return false;
    }
    return detail::ValidateExtraBytesLayout(header, error);
}

std::string ExtractWktCrs(
    const std::vector<LasVariableLengthRecord>& records) {
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

bool InspectMetadata(const std::vector<std::uint8_t>& bytes,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (InspectMetadata(bytes, header, error)) {
        return true;
    }
    detail::AddErrorDiagnostic(error, diagnostics);
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
    detail::AddErrorDiagnostic(error, diagnostics);
    return false;
}

} // namespace usdlas