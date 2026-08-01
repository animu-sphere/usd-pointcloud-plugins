#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

namespace {

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
        error == "LAZ point data offset is outside the file" ||
        error == "LAZ EVLR offset is outside the file") {
        return usdgeo::DiagnosticCode::InvalidOffset;
    }
    if (error == "LAS point record length is too small for its format") {
        return usdgeo::DiagnosticCode::InvalidRecordLength;
    }
    if (error == "LAS variable-length record header is truncated" ||
        error == "LAZ header is truncated" ||
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
    if (error == "decoded LAS point contains a non-finite coordinate") {
        return usdgeo::DiagnosticCode::NonFiniteCoordinate;
    }
    return usdgeo::DiagnosticCode::DecodeFailure;
}

void AddDiagnostic(const std::string& error,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back({CodeForError(error), usdgeo::Severity::Error, error,
                            std::nullopt, std::nullopt});
}

bool ReadHeaderBytes(const std::string& filename,
                     std::vector<std::uint8_t>& bytes,
                     std::string& error) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        error = "could not open LAZ file: " + filename;
        return false;
    }
    stream.seekg(0, std::ios::end);
    const auto fileSize = stream.tellg();
    if (fileSize < 0 || fileSize > (std::numeric_limits<std::size_t>::max)()) {
        error = "could not determine LAZ file size";
        return false;
    }
    std::size_t prefixSize = 375;
    if (fileSize < std::streampos(375)) {
        prefixSize = static_cast<std::size_t>(fileSize);
    }
    stream.seekg(0);
    bytes.resize(prefixSize);
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) {
        error = "could not read LAZ file";
        return false;
    }
    if (bytes.size() <= 104) {
        error = "LAZ header is truncated";
        return false;
    }
    bytes[104] &= 0x3f;
    usdlas::LasHeader inspectedHeader;
    if (!usdlas::InspectHeader(bytes, inspectedHeader, error)) {
        return false;
    }
    if (inspectedHeader.pointDataOffset > static_cast<std::size_t>(fileSize)) {
        error = "LAZ point data offset is outside the file";
        return false;
    }
    bytes.resize(inspectedHeader.pointDataOffset);
    stream.clear();
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) {
        error = "could not read LAZ header and VLRs";
        return false;
    }
    bytes[104] &= 0x3f;
    return true;
}

bool ReadExtendedRecords(const std::string& filename,
                         std::uint64_t offset,
                         std::uint32_t count,
                         std::vector<usdlas::LasVariableLengthRecord>& records,
                         std::string& error) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        error = "could not open LAZ file for EVLRs: " + filename;
        return false;
    }
    stream.seekg(0, std::ios::end);
    const auto fileSize = stream.tellg();
    if (fileSize < 0 || offset > static_cast<std::uint64_t>(fileSize)) {
        error = "LAZ EVLR offset is outside the file";
        return false;
    }
    stream.seekg(static_cast<std::streamoff>(offset));
    std::vector<std::uint8_t> bytes;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::array<std::uint8_t, 60> recordHeader{};
        stream.read(reinterpret_cast<char*>(recordHeader.data()),
                    static_cast<std::streamsize>(recordHeader.size()));
        if (!stream) {
            error = "LAS extended variable-length record header is truncated";
            return false;
        }
        std::uint64_t length = 0;
        std::memcpy(&length, recordHeader.data() + 20, sizeof(length));
        const auto dataOffset = stream.tellg();
        if (dataOffset < 0 ||
            length > static_cast<std::uint64_t>(fileSize - dataOffset) ||
            length > (std::numeric_limits<std::size_t>::max)() -
                          bytes.size()) {
            error = "LAS extended variable-length record data is truncated";
            return false;
        }
        bytes.insert(bytes.end(), recordHeader.begin(), recordHeader.end());
        const auto oldSize = bytes.size();
        bytes.resize(oldSize + static_cast<std::size_t>(length));
        stream.read(reinterpret_cast<char*>(bytes.data() + oldSize),
                    static_cast<std::streamsize>(length));
        if (!stream) {
            error = "LAS extended variable-length record data is truncated";
            return false;
        }
    }
    return usdlas::InspectRecords(bytes, 0, count, true, records, error);
}

class LazPerfDecoder final : public usdlaz::LazDecoder {
public:
    explicit LazPerfDecoder(std::unique_ptr<lazperf::reader::named_file> file)
        : file_(std::move(file)) {}

    bool ReadHeader(usdlas::LasHeader& header, std::string& error) override {
        header = header_;
        error.clear();
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::string& error) override {
        points.clear();
        complete = false;
        error.clear();
        if (maximumPoints == 0 || pointsRead_ >= header_.pointCount) {
            complete = true;
            return true;
        }

        const auto remaining = header_.pointCount - pointsRead_;
        const auto count = static_cast<std::size_t>(
            remaining < maximumPoints ? remaining : maximumPoints);
        std::vector<char> record(header_.pointRecordLength);
        points.reserve(count);
        try {
            for (std::size_t index = 0; index < count; ++index) {
                file_->readPoint(record.data());
                usdlas::LasPoint point;
                if (usdlas::DecodePoint(header_,
                                         std::vector<std::uint8_t>(
                                             reinterpret_cast<std::uint8_t*>(record.data()),
                                             reinterpret_cast<std::uint8_t*>(record.data()) +
                                                 record.size()),
                                         point, error)) {
                    points.push_back(point);
                } else {
                    points.clear();
                    return false;
                }
            }
        } catch (const std::exception& exception) {
            points.clear();
            error = exception.what();
            return false;
        }
        pointsRead_ += points.size();
        complete = pointsRead_ == header_.pointCount;
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::vector<usdgeo::Diagnostic>& diagnostics) override {
        diagnostics.clear();
        std::string error;
        const bool result = ReadChunk(maximumPoints, points, complete, error);
        if (!result) {
            AddDiagnostic(error, diagnostics);
            if (!diagnostics.empty()) {
                diagnostics.front().pointIndex = pointsRead_;
            }
        }
        return result;
    }

    usdlas::LasHeader header_;

private:
    std::unique_ptr<lazperf::reader::named_file> file_;
    std::uint64_t pointsRead_ = 0;
};

} // namespace

namespace usdlaz {

std::unique_ptr<LazDecoder> CreateFileDecoder(const std::string& filename,
                                              std::string& error) {
    error.clear();
    try {
        auto file = std::make_unique<lazperf::reader::named_file>(filename);
        std::vector<std::uint8_t> bytes;
        if (!ReadHeaderBytes(filename, bytes, error)) {
            return nullptr;
        }
        usdlas::LasHeader header;
        if (!usdlas::InspectHeader(bytes, header, error)) {
            return nullptr;
        }
        header.variableLengthRecords.clear();
        if (!usdlas::InspectRecords(bytes, header.headerSize,
                                    header.variableLengthRecordCount, false,
                                    header.variableLengthRecords, error)) {
            return nullptr;
        }
        if (header.extendedVariableLengthRecordCount != 0 &&
            !ReadExtendedRecords(filename,
                                 header.firstExtendedVariableLengthRecordOffset,
                                 header.extendedVariableLengthRecordCount,
                                 header.variableLengthRecords, error)) {
            return nullptr;
        }
        if (!usdlas::ParseKnownMetadata(header.variableLengthRecords, header,
                                         error)) {
            return nullptr;
        }
        auto decoder = std::make_unique<LazPerfDecoder>(std::move(file));
        decoder->header_ = std::move(header);
        return decoder;
    } catch (const std::exception& exception) {
        error = exception.what();
        return nullptr;
    }
}

std::unique_ptr<LazDecoder> CreateFileDecoder(
    const std::string& filename,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    auto decoder = CreateFileDecoder(filename, error);
    if (decoder || error.empty()) {
        return decoder;
    }
    AddDiagnostic(error, diagnostics);
    return nullptr;
}

} // namespace usdlaz