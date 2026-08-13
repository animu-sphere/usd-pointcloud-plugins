#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <streambuf>
#include <utility>
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
    if (error == "unsupported LAZ waveform point format") {
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

class ChunkInput {
public:
    explicit ChunkInput(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes) {}

    void Read(unsigned char* output, std::size_t size) {
        if (offset_ > bytes_.size() || size > bytes_.size() - offset_) {
            throw std::runtime_error("LAZ chunk is truncated");
        }
        std::memcpy(output, bytes_.data() + offset_, size);
        offset_ += size;
    }

    lazperf::InputCb Callback() {
        return [this](unsigned char* output, std::size_t size) {
            Read(output, size);
        };
    }

private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_ = 0;
};

std::size_t BasePointSize(std::uint8_t pointFormat) {
    switch (pointFormat) {
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

using LazPerfDecoderPtr = decltype(lazperf::build_las_decompressor(
    std::declval<lazperf::InputCb>(), std::declval<std::uint8_t>(),
    std::declval<std::size_t>()));

class LazPerfChunkDecoder final : public usdlaz::LazChunkDecoder {
public:
    LazPerfChunkDecoder(usdlas::LasHeader header,
                        std::uint64_t pointCount,
                        LazPerfDecoderPtr decoder)
        : header_(std::move(header)),
          pointCount_(pointCount),
          decoder_(std::move(decoder)) {}

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::vector<usdgeo::Diagnostic>& diagnostics) override {
            points.clear();
        complete = false;
        diagnostics.clear();
        if (maximumPoints == 0 || pointsRead_ >= pointCount_) {
            complete = true;
            return true;
        }

        const auto remaining = pointCount_ - pointsRead_;
        const auto count = static_cast<std::size_t>(
            remaining < maximumPoints ? remaining : maximumPoints);
        std::vector<char> record(header_.pointRecordLength);
        points.reserve(count);
        try {
            for (std::size_t index = 0; index < count; ++index) {
                decoder_->decompress(record.data());
                std::vector<std::uint8_t> encoded(
                    reinterpret_cast<std::uint8_t*>(record.data()),
                    reinterpret_cast<std::uint8_t*>(record.data()) +
                        record.size());
                usdlas::LasPoint point;
                if (!usdlas::DecodePoint(header_, encoded, point,
                                         diagnostics)) {
                    if (!diagnostics.empty()) {
                        diagnostics.back().pointIndex = pointsRead_ + index;
                    }
                    points.clear();
                    return false;
                }
                points.push_back(std::move(point));
            }
        } catch (const std::exception& exception) {
            points.clear();
            AddDiagnostic(exception.what(), diagnostics);
            if (!diagnostics.empty()) {
                diagnostics.back().pointIndex = pointsRead_;
            }
            return false;
        }

        pointsRead_ += points.size();
        complete = pointsRead_ == pointCount_;
        return true;
    }

private:
    usdlas::LasHeader header_;
    std::uint64_t pointCount_ = 0;
    LazPerfDecoderPtr decoder_;
    std::uint64_t pointsRead_ = 0;
};

class CountingStreambuf final : public std::streambuf {
public:
    explicit CountingStreambuf(std::streambuf* source) : source_(source) {}

    std::uint64_t BytesRead() const noexcept { return bytesRead_; }

protected:
    std::streamsize xsgetn(char* output, std::streamsize count) override {
        const auto read = source_->sgetn(output, count);
        if (read > 0) bytesRead_ += static_cast<std::uint64_t>(read);
        return read;
    }

    int_type underflow() override { return source_->sgetc(); }

    int_type uflow() override {
        const auto value = source_->sbumpc();
        if (value != traits_type::eof()) ++bytesRead_;
        return value;
    }

    pos_type seekoff(off_type offset,
                     std::ios_base::seekdir direction,
                     std::ios_base::openmode mode) override {
        return source_->pubseekoff(offset, direction, mode);
    }

    pos_type seekpos(pos_type position,
                     std::ios_base::openmode mode) override {
        return source_->pubseekpos(position, mode);
    }

    int sync() override { return source_->pubsync(); }

private:
    std::streambuf* source_;
    std::uint64_t bytesRead_ = 0;
};

bool ReadHeaderBytes(const std::string& filename,
                     std::vector<std::uint8_t>& bytes,
                     std::string& error,
                     std::uint64_t& bytesRead) {
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
    bytesRead += static_cast<std::uint64_t>(bytes.size());
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
    bytesRead += static_cast<std::uint64_t>(bytes.size());
    bytes[104] &= 0x3f;
    return true;
}

bool ReadExtendedRecords(const std::string& filename,
                         std::uint64_t offset,
                         std::uint32_t count,
                         std::vector<usdlas::LasVariableLengthRecord>& records,
                         std::string& error,
                         std::uint64_t& bytesRead) {
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
        bytesRead += static_cast<std::uint64_t>(recordHeader.size());
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
        bytesRead += length;
    }
    return usdlas::InspectRecords(bytes, 0, count, true, records, error);
}

class LazPerfDecoder final : public usdlaz::LazDecoder {
public:
    LazPerfDecoder(std::unique_ptr<std::ifstream> input,
                   std::unique_ptr<CountingStreambuf> buffer,
                   std::unique_ptr<std::istream> stream,
                   std::unique_ptr<lazperf::reader::generic_file> file,
                   std::uint64_t initialSourceBytesRead)
        : input_(std::move(input)),
          buffer_(std::move(buffer)),
          stream_(std::move(stream)),
          file_(std::move(file)),
          initialSourceBytesRead_(initialSourceBytesRead) {}

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

    std::uint64_t SourceBytesRead() const noexcept override {
        return initialSourceBytesRead_ +
               (buffer_ ? buffer_->BytesRead() : 0);
    }

    usdlas::LasHeader header_;

private:
    std::unique_ptr<std::ifstream> input_;
    std::unique_ptr<CountingStreambuf> buffer_;
    std::unique_ptr<std::istream> stream_;
    std::unique_ptr<lazperf::reader::generic_file> file_;
    std::uint64_t initialSourceBytesRead_ = 0;
    std::uint64_t pointsRead_ = 0;
};

} // namespace

namespace usdlaz {

bool DecodeLazChunk(const usdlas::LasHeader& header,
                    const std::vector<std::uint8_t>& bytes,
                    std::uint64_t pointCount,
                    std::vector<usdlas::LasPoint>& points,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    points.clear();
    const auto result = DecodeLazChunk(
        header, bytes, pointCount,
        [&](const usdlas::LasPoint& point, std::uint64_t) {
            points.push_back(point);
            return true;
        },
        diagnostics);
    if (!result) {
        points.clear();
    }
    return result;
}

bool DecodeLazChunk(const usdlas::LasHeader& header,
                    const std::vector<std::uint8_t>& bytes,
                    std::uint64_t pointCount,
                    const LazPointConsumer& consume,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    ChunkInput input(bytes);
    return DecodeLazChunk(header, pointCount, input.Callback(), consume,
                          diagnostics);
}

std::unique_ptr<LazChunkDecoder> CreateLazChunkDecoder(
    const usdlas::LasHeader& header,
    std::uint64_t pointCount,
    const LazInput& input,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    const auto basePointSize = BasePointSize(header.pointFormat);
    if (!header.IsValid() || basePointSize == 0 ||
        header.pointRecordLength < basePointSize || pointCount == 0 ||
        pointCount > header.pointCount || !input) {
        AddDiagnostic("invalid LAS header or LAZ chunk point count",
                      diagnostics);
        return nullptr;
    }

    try {
        const auto decoder = lazperf::build_las_decompressor(
            input, header.pointFormat,
            header.pointRecordLength - basePointSize);
        if (!decoder) {
            AddDiagnostic("unsupported LAZ point format", diagnostics);
            return nullptr;
        }
        return std::make_unique<LazPerfChunkDecoder>(
            header, pointCount, std::move(decoder));
    } catch (const std::exception& exception) {
        AddDiagnostic(exception.what(), diagnostics);
        return nullptr;
    }
}

bool DecodeLazChunk(const usdlas::LasHeader& header,
                    std::uint64_t pointCount,
                    const LazInput& input,
                    const LazPointConsumer& consume,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    auto decoder =
        CreateLazChunkDecoder(header, pointCount, input, diagnostics);
    if (!decoder || !consume) {
        if (decoder && !consume) {
            diagnostics.clear();
            AddDiagnostic("invalid LAZ point consumer", diagnostics);
        }
        return false;
    }

    const auto maximumPoints = (std::min)(
        pointCount,
        static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()));
    std::uint64_t pointsRead = 0;
    bool complete = false;
    while (!complete) {
        std::vector<usdlas::LasPoint> points;
        if (!decoder->ReadChunk(static_cast<std::size_t>(maximumPoints), points,
                                complete, diagnostics)) {
            return false;
        }
        if (points.empty() && !complete) {
            AddDiagnostic("LAZ decoder returned an empty incomplete chunk",
                          diagnostics);
            return false;
        }
        for (const auto& point : points) {
            if (!consume(point, pointsRead++)) {
                diagnostics.push_back(
                    {usdgeo::DiagnosticCode::DecodeFailure,
                     usdgeo::Severity::Error,
                     "LAZ chunk consumer rejected a point", std::nullopt,
                     pointsRead - 1});
                return false;
            }
        }
    }
    return true;
}

std::unique_ptr<LazDecoder> CreateFileDecoder(const std::string& filename,
                                              std::string& error) {
    error.clear();
    try {
        std::uint64_t initialSourceBytesRead = 0;
        std::vector<std::uint8_t> bytes;
        if (!ReadHeaderBytes(filename, bytes, error, initialSourceBytesRead)) {
            return nullptr;
        }
        usdlas::LasHeader header;
        if (!usdlas::InspectHeader(bytes, header, error)) {
            return nullptr;
        }
        if (header.pointFormat == 4 || header.pointFormat == 5 ||
            header.pointFormat == 9 || header.pointFormat == 10) {
            error = "unsupported LAZ waveform point format";
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
                                 header.variableLengthRecords, error,
                                 initialSourceBytesRead)) {
            return nullptr;
        }
        if (!usdlas::ParseKnownMetadata(header.variableLengthRecords, header,
                                         error)) {
            return nullptr;
        }
        auto input = std::make_unique<std::ifstream>(filename, std::ios::binary);
        if (!*input) {
            error = "could not open LAZ file: " + filename;
            return nullptr;
        }
        auto buffer = std::make_unique<CountingStreambuf>(input->rdbuf());
        auto stream = std::make_unique<std::istream>(buffer.get());
        auto file = std::make_unique<lazperf::reader::generic_file>(*stream);
        auto decoder = std::make_unique<LazPerfDecoder>(
            std::move(input), std::move(buffer), std::move(stream),
            std::move(file), initialSourceBytesRead);
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