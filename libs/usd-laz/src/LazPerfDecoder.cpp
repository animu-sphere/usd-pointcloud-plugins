#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"

#include <fstream>
#include <limits>
#include <memory>
#include <vector>

namespace {

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
    const auto prefixSize = static_cast<std::size_t>(
        fileSize < static_cast<std::streamoff>(375) ? fileSize : 375);
    stream.seekg(0);
    bytes.resize(prefixSize);
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty()) {
        error = "could not read LAZ file";
        return false;
    }
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
    return true;
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
                if (!usdlas::DecodePoint(header_,
                                         std::vector<std::uint8_t>(
                                             reinterpret_cast<std::uint8_t*>(record.data()),
                                             reinterpret_cast<std::uint8_t*>(record.data()) +
                                                 record.size()),
                                         point, error)) {
                    points.clear();
                    return false;
                }
                points.push_back(point);
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
        if (!usdlas::InspectMetadata(bytes, header, error)) {
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

} // namespace usdlaz