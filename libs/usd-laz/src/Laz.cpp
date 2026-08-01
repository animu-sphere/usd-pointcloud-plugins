#include "usdlaz/Laz.h"

#include <cstdint>
#include <limits>
#include <utility>

namespace {

usdgeo::DiagnosticCode CodeForError(const std::string& error) {
    if (error == "LAZ header is truncated" ||
        error == "LAS extended variable-length record header is truncated") {
        return usdgeo::DiagnosticCode::TruncatedHeader;
    }
    if (error == "LAZ point data offset is outside the file" ||
        error == "LAZ EVLR offset is outside the file") {
        return usdgeo::DiagnosticCode::InvalidOffset;
    }
    if (error == "LAS extended variable-length record data is truncated") {
        return usdgeo::DiagnosticCode::TruncatedRecord;
    }
    return usdgeo::DiagnosticCode::DecodeFailure;
}

void AddDiagnostic(const std::string& error,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back({CodeForError(error), usdgeo::Severity::Error, error,
                            std::nullopt, std::nullopt});
}

void AddDiagnostic(const std::string& error,
                   std::uint64_t pointIndex,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    AddDiagnostic(error, diagnostics);
    diagnostics.back().pointIndex = pointIndex;
}

} // namespace

namespace usdlaz {

bool LazDecoder::ReadHeader(usdlas::LasHeader& header,
                            std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (ReadHeader(header, error)) {
        return true;
    }
    AddDiagnostic(error, diagnostics);
    return false;
}

bool LazDecoder::ReadChunk(
    std::size_t maximumPoints,
    std::vector<usdlas::LasPoint>& points,
    bool& complete,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (ReadChunk(maximumPoints, points, complete, error)) {
        return true;
    }
    AddDiagnostic(error, diagnostics);
    return false;
}

LazReader::LazReader(std::unique_ptr<LazDecoder> decoder)
    : decoder_(std::move(decoder)) {}

bool LazReader::Read(
    const LazReadOptions& options,
    const std::function<bool(const usdlas::LasHeader&,
                             const std::vector<usdlas::LasPoint>&)>& consume,
    usdlas::LasHeader& header,
    std::string& error) {
    if (!decoder_) {
        error = "LAZ decoder is not configured";
        return false;
    }
    if (options.chunkPointLimit == 0 || !consume) {
        error = "LAZ read options or consumer are invalid";
        return false;
    }
    if (!decoder_->ReadHeader(header, error)) {
        return false;
    }

    std::uint64_t pointsRead = 0;
    bool complete = false;
    while (!complete) {
        std::vector<usdlas::LasPoint> points;
        complete = false;
        if (!decoder_->ReadChunk(options.chunkPointLimit, points, complete,
                                 error)) {
            return false;
        }
        if (points.empty() && !complete) {
            error = "LAZ decoder returned an empty incomplete chunk";
            return false;
        }
        if (points.size() > options.chunkPointLimit) {
            error = "LAZ decoder exceeded the requested chunk size";
            return false;
        }
        if (points.size() > header.pointCount - pointsRead ||
            pointsRead > std::numeric_limits<std::uint64_t>::max() -
                              points.size()) {
            error = "LAZ decoder returned too many points";
            return false;
        }
        pointsRead += points.size();
        if (!consume(header, points)) {
            error = "LAZ chunk consumer rejected a chunk";
            return false;
        }
    }
    if (pointsRead != header.pointCount) {
        error = "LAZ decoder point count does not match the header";
        return false;
    }
    return true;
}

bool LazReader::Read(
    const LazReadOptions& options,
    const std::function<bool(const usdlas::LasHeader&,
                             const std::vector<usdlas::LasPoint>&)>& consume,
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    if (!decoder_) {
        AddDiagnostic("LAZ decoder is not configured", diagnostics);
        return false;
    }
    if (options.chunkPointLimit == 0 || !consume) {
        AddDiagnostic("LAZ read options or consumer are invalid", diagnostics);
        return false;
    }
    if (!decoder_->ReadHeader(header, diagnostics)) {
        return false;
    }

    std::uint64_t pointsRead = 0;
    bool complete = false;
    while (!complete) {
        std::vector<usdlas::LasPoint> points;
        complete = false;
        if (!decoder_->ReadChunk(options.chunkPointLimit, points, complete,
                                 diagnostics)) {
            if (diagnostics.empty()) {
                AddDiagnostic("LAZ decoder failed", pointsRead, diagnostics);
            }
            return false;
        }
        if (points.empty() && !complete) {
            AddDiagnostic("LAZ decoder returned an empty incomplete chunk",
                          pointsRead, diagnostics);
            return false;
        }
        if (points.size() > options.chunkPointLimit) {
            AddDiagnostic("LAZ decoder exceeded the requested chunk size",
                          pointsRead, diagnostics);
            return false;
        }
        if (points.size() > header.pointCount - pointsRead ||
            pointsRead > std::numeric_limits<std::uint64_t>::max() -
                              points.size()) {
            AddDiagnostic("LAZ decoder returned too many points", pointsRead,
                          diagnostics);
            return false;
        }
        pointsRead += points.size();
        if (!consume(header, points)) {
            AddDiagnostic("LAZ chunk consumer rejected a chunk", pointsRead,
                          diagnostics);
            return false;
        }
    }
    if (pointsRead != header.pointCount) {
        AddDiagnostic("LAZ decoder point count does not match the header",
                      pointsRead, diagnostics);
        return false;
    }
    return true;
}

} // namespace usdlaz
