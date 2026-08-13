#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"
#include "lazperf/las.hpp"

#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

class FakeDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 3;
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::string&) override {
        Check(maximumPoints == 2);
        points.clear();
        if (offset_ == 0) {
            points.resize(2);
            offset_ = 2;
            complete = false;
        } else {
            points.resize(1);
            offset_ = 3;
            complete = true;
        }
        return true;
    }

private:
    std::size_t offset_ = 0;
};

class IncompleteDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 1;
        return true;
    }

    bool ReadChunk(std::size_t,
                   std::vector<usdlas::LasPoint>& points,
                   bool&,
                   std::string&) override {
        if (calls_++ == 0) {
            points.resize(1);
        }
        return true;
    }

private:
    std::size_t calls_ = 0;
};

class TypedFailureDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 3;
        return true;
    }

    bool ReadChunk(std::size_t,
                   std::vector<usdlas::LasPoint>&,
                   bool&,
                   std::string& error) override {
        error = "string diagnostic path was used";
        return false;
    }

    bool ReadChunk(std::size_t,
                   std::vector<usdlas::LasPoint>&,
                   bool&,
                   std::vector<usdgeo::Diagnostic>& diagnostics) override {
        diagnostics.push_back({usdgeo::DiagnosticCode::NonFiniteCoordinate,
                               usdgeo::Severity::Error,
                               "typed chunk failure", std::nullopt, 2});
        return false;
    }
};

class BudgetDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 3;
        header.pointRecordLength = 20;
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::string&) override {
        Check(maximumPoints == 1);
        points.resize(1);
        ++offset_;
        complete = offset_ == 3;
        return true;
    }

private:
    std::size_t offset_ = 0;
};

class FilterDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 3;
        header.pointRecordLength = 20;
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::string&) override {
        points.clear();
        const auto count = (std::min)(maximumPoints, std::size_t{3} - offset_);
        points.resize(count);
        for (std::size_t index = 0; index < count; ++index) {
            points[index].sourcePosition = {
                static_cast<double>(offset_ + index), 0.0, 0.0};
            points[index].classification =
                static_cast<std::uint8_t>(offset_ + index + 1);
        }
        offset_ += count;
        complete = offset_ == 3;
        return true;
    }

private:
    std::size_t offset_ = 0;
};

void TestChunkForwarding() {
    usdlaz::LazReader reader(std::make_unique<FakeDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 2;
    usdlas::LasHeader header;
    std::string error;
    std::size_t chunks = 0;
    std::size_t points = 0;
    Check(reader.Read(
        options,
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>& data) {
            ++chunks;
            points += data.size();
            return true;
        },
        header, error));
    Check(error.empty());
    Check(chunks == 2);
    Check(points == 3);
}

void TestFiltering() {
    usdlaz::LazReader reader(std::make_unique<FilterDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 2;
    options.bounds = usdgeo::SpatialBounds{{0.5, -1.0, -1.0},
                                           {2.5, 1.0, 1.0}};
    options.classifications = {2};
    usdlas::LasHeader header;
    std::string error;
    std::size_t points = 0;
    Check(reader.Read(
        options,
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>& data) {
            points += data.size();
            Check(data.size() == 1 && data.front().classification == 2);
            return true;
        },
        header, error));
    Check(points == 1 && error.empty());
}

void TestCompleteFlagIsResetBeforeEachChunk() {
    usdlaz::LazReader reader(std::make_unique<IncompleteDecoder>());
    usdlas::LasHeader header;
    std::string error;
    std::size_t points = 0;
    Check(!reader.Read(
        {},
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>& data) {
            points += data.size();
            return true;
        },
        header, error));
    Check(points == 1);
    Check(error == "LAZ decoder returned an empty incomplete chunk");

    usdlaz::LazReader typedReader(std::make_unique<IncompleteDecoder>());
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!typedReader.Read(
        {},
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>&) { return true; },
        header, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::DecodeFailure &&
          diagnostics.front().severity == usdgeo::Severity::Error);
}

void TestFileDecoderReportsOpenFailure() {
    std::string error;
    auto decoder = usdlaz::CreateFileDecoder("missing-file.laz", error);
    Check(!decoder);
    Check(!error.empty());

    std::vector<usdgeo::Diagnostic> diagnostics;
    decoder = usdlaz::CreateFileDecoder("missing-file.laz", diagnostics);
    Check(!decoder);
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::DecodeFailure);
}

void TestTypedReaderPreservesDecoderDiagnostic() {
    usdlaz::LazReader reader(std::make_unique<TypedFailureDecoder>());
    usdlas::LasHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.Read(
        {},
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>&) { return true; },
        header, diagnostics));
    Check(diagnostics.size() == 1);
    Check(diagnostics.front().message == "typed chunk failure");
    Check(diagnostics.front().pointIndex == 2);
}

void TestTypedReaderPreservesConsumerDiagnostic() {
    usdlaz::LazReader reader(std::make_unique<FakeDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 1;
    options.memoryBudgetBytes = sizeof(usdlas::LasPoint) + 40;
    usdlas::LasHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!reader.Read(
        options,
        [&](const usdlas::LasHeader&,
            const std::vector<usdlas::LasPoint>&,
            std::string& callbackError) {
            callbackError = "consumer rejected with detail";
            return false;
        },
        header, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().message == "consumer rejected with detail");
}

void TestRangeMemoryBudgetAndCancellation() {
    usdlaz::LazReader reader(std::make_unique<BudgetDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 2;
    options.memoryBudgetBytes = sizeof(usdlas::LasPoint) + 40;
    options.range = {1, 1};
    usdlas::LasHeader header;
    std::string error;
    std::size_t points = 0;
    Check(reader.Read(
        options,
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>& data) {
            points += data.size();
            return true;
        },
        header, error));
    Check(points == 1);

    usdlaz::LazReader cancelled(std::make_unique<BudgetDecoder>());
    options.range = {};
    options.isCancelled = [] { return true; };
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!cancelled.Read(
        options,
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>&) {
            return true;
        },
        header, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().message == "LAZ read cancelled");
}

void TestLazPerfFileDecoder() {
    const auto filename =
        std::filesystem::temp_directory_path() / "usd-pointcloud-plugins-test.laz";
    {
        lazperf::writer::named_file::config config;
        config.scale = {0.01, 0.01, 0.01};
        config.chunk_size = 2;
        config.minor_version = 2;
        lazperf::writer::named_file writer(filename.string(), config);
        for (int index = 0; index < 3; ++index) {
            lazperf::las::point10 source;
            source.x = index * 100;
            source.y = index * 200;
            source.z = index * 300;
            source.intensity = static_cast<unsigned short>(index + 1);
            char record[20];
            source.pack(record);
            writer.writePoint(record);
        }
        writer.close();
    }

    {
        std::string error;
        auto decoder = usdlaz::CreateFileDecoder(filename.string(), error);
        Check(decoder != nullptr);
        usdlaz::LazReader reader(std::move(decoder));
        usdlaz::LazReadOptions options;
        options.chunkPointLimit = 2;
        usdlas::LasHeader header;
        std::size_t points = 0;
        Check(reader.Read(
            options,
            [&](const usdlas::LasHeader&,
                const std::vector<usdlas::LasPoint>& data) {
                points += data.size();
                return true;
            },
            header, error));
        Check(error.empty());
        Check(header.pointCount == 3);
        Check(points == 3);
    }
    {
        usdlaz::LazReadOptions options;
        options.chunkPointLimit = 2;
        usdlas::LasHeader header;
        std::vector<usdgeo::Diagnostic> diagnostics;
        auto stream = usdlaz::OpenLazPointStream(
            filename.string(), options, header, diagnostics);
        Check(stream != nullptr && diagnostics.empty());
        std::size_t points = 0;
        std::size_t chunks = 0;
        for (;;) {
            usdpointcloud::PointChunk chunk;
            usdpointcloud::PointData data;
            usdgeo::Diagnostic diagnostic;
            const auto status = stream->ReadNext(chunk, data, diagnostic);
            if (status == usdpointcloud::PointStreamStatus::End) break;
            Check(status == usdpointcloud::PointStreamStatus::Chunk);
            Check(chunk.IsValid() && data.IsValid());
            points += data.positions.size();
            ++chunks;
        }
        Check(header.pointCount == 3 && points == 3 && chunks == 2);
    }

    {
        usdlas::LasHeader header;
        std::vector<usdgeo::Diagnostic> diagnostics;
        auto fileDecoder =
            usdlaz::CreateFileDecoder(filename.string(), diagnostics);
        Check(fileDecoder != nullptr &&
              fileDecoder->ReadHeader(header, diagnostics));
        auto file =
            std::make_shared<std::ifstream>(filename, std::ios::binary);
        Check(static_cast<bool>(*file));
        file->seekg(static_cast<std::streamoff>(header.pointDataOffset),
                    std::ios::beg);
        Check(static_cast<bool>(*file));
        auto remaining = std::filesystem::file_size(filename) -
                         header.pointDataOffset;
        const usdlaz::LazInput input =
            [file, remaining](unsigned char* output,
                              std::size_t size) mutable {
                if (size > remaining) {
                    throw std::runtime_error("test LAZ range is truncated");
                }
                file->read(reinterpret_cast<char*>(output),
                           static_cast<std::streamsize>(size));
                if (!*file) {
                    throw std::runtime_error("test LAZ range is truncated");
                }
                remaining -= size;
            };
        auto decoder = usdlaz::CreateLazChunkDecoder(
            header, header.pointCount, input, diagnostics);
        Check(decoder != nullptr && diagnostics.empty());

        for (int index = 0; index < 3; ++index) {
            std::vector<usdlas::LasPoint> points;
            bool complete = false;
            Check(decoder->ReadChunk(1, points, complete, diagnostics));
            Check(diagnostics.empty() && points.size() == 1);
            Check(complete == (index == 2));
        }
    }
    Check(std::remove(filename.string().c_str()) == 0);
}

void TestLazPerfPointFormat7Decoder() {
    const auto filename =
        std::filesystem::temp_directory_path() /
        "usd-pointcloud-plugins-test-point-format-7.laz";
    {
        lazperf::writer::named_file::config config;
        config.scale = {0.01, 0.01, 0.01};
        config.chunk_size = 3;
        config.minor_version = 4;
        config.pdrf = 7;
        lazperf::writer::named_file writer(filename.string(), config);
        for (int index = 0; index < 3; ++index) {
            std::array<char, 36> record{};
            lazperf::utils::pack(static_cast<std::int32_t>(index * 100),
                                 record.data());
            lazperf::utils::pack(static_cast<std::int32_t>(index * 200),
                                 record.data() + 4);
            lazperf::utils::pack(static_cast<std::int32_t>(index * 300),
                                 record.data() + 8);
            lazperf::utils::pack(static_cast<std::uint16_t>(index + 1),
                                 record.data() + 12);
            record[14] = static_cast<char>(0x21);
            record[15] = 0;
            record[16] = static_cast<char>(2 + index);
            record[17] = static_cast<char>(3 + index);
            lazperf::utils::pack(static_cast<std::int16_t>(-4 + index),
                                 record.data() + 18);
            lazperf::utils::pack(static_cast<std::uint16_t>(42 + index),
                                 record.data() + 20);
            lazperf::utils::pack(12.5 + index, record.data() + 22);
            lazperf::utils::pack(static_cast<std::uint16_t>(100 + index),
                                 record.data() + 30);
            lazperf::utils::pack(static_cast<std::uint16_t>(200 + index),
                                 record.data() + 32);
            lazperf::utils::pack(static_cast<std::uint16_t>(300 + index),
                                 record.data() + 34);
            writer.writePoint(record.data());
        }
        writer.close();
    }

    {
        std::string error;
        auto decoder = usdlaz::CreateFileDecoder(filename.string(), error);
        Check(decoder != nullptr && error.empty());
        usdlaz::LazReader reader(std::move(decoder));
        usdlaz::LazReadOptions options;
        options.chunkPointLimit = 2;
        usdlas::LasHeader header;
        std::size_t points = 0;
        Check(reader.Read(
            options,
            [&](const usdlas::LasHeader&,
                const std::vector<usdlas::LasPoint>& data) {
                points += data.size();
                return true;
            },
            header, error));
        Check(error.empty() && header.pointFormat == 7 && points == 3);
    }
    Check(std::remove(filename.string().c_str()) == 0);
}

} // namespace

int main() {
    TestChunkForwarding();
    TestFiltering();
    TestCompleteFlagIsResetBeforeEachChunk();
    TestFileDecoderReportsOpenFailure();
    TestTypedReaderPreservesDecoderDiagnostic();
    TestRangeMemoryBudgetAndCancellation();
    TestLazPerfFileDecoder();
    TestLazPerfPointFormat7Decoder();
    return 0;
}
