#include "usdlaz/Laz.h"

#include "lazperf/io.hpp"
#include "lazperf/las.hpp"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <memory>
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

void TestRangeMemoryBudgetAndCancellation() {
    usdlaz::LazReader reader(std::make_unique<BudgetDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 2;
    options.memoryBudgetBytes = sizeof(usdlas::LasPoint);
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
        std::filesystem::temp_directory_path() / "usd-geo-plugins-test.laz";
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
    Check(std::remove(filename.string().c_str()) == 0);
}

} // namespace

int main() {
    TestChunkForwarding();
    TestCompleteFlagIsResetBeforeEachChunk();
    TestFileDecoderReportsOpenFailure();
    TestTypedReaderPreservesDecoderDiagnostic();
    TestRangeMemoryBudgetAndCancellation();
    TestLazPerfFileDecoder();
    return 0;
}
