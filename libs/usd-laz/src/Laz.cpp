#include "usdlaz/Laz.h"

#include <algorithm>
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
    const LazPointChunkConsumer& consume,
    usdlas::LasHeader& header,
    std::string& error) {
    if (!decoder_) {
        error = "LAZ decoder is not configured";
        return false;
    }
    if (!options.IsValid() || !consume) {
        error = "LAZ read options or consumer are invalid";
        return false;
    }
    if (!decoder_->ReadHeader(header, error)) {
        return false;
    }

    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount > header.pointCount -
                                        options.range.firstPoint)) {
        error = "LAZ point range is outside the header";
        return false;
    }

    const auto rangeEnd = options.range.pointCount == 0
                              ? header.pointCount
                              : options.range.firstPoint +
                                    options.range.pointCount;
    const auto maximumSize = (std::numeric_limits<std::size_t>::max)();
    if (header.pointRecordLength >
        (maximumSize - sizeof(usdlas::LasPoint)) / 2) {
        error = "LAZ point record size is invalid";
        return false;
    }
    const auto bytesPerPoint =
        sizeof(usdlas::LasPoint) +
        static_cast<std::size_t>(header.pointRecordLength) * 2;
    const auto budgetPointLimit = options.memoryBudgetBytes / bytesPerPoint;
    const auto maximumPoints =
        (std::min)(options.chunkPointLimit, budgetPointLimit);
    if (maximumPoints == 0) {
        error = "LAZ memory budget is too small for one point";
        return false;
    }

    std::uint64_t pointsRead = 0;
    std::uint64_t selectedPointsRead = 0;
    bool complete = false;
    while (!complete) {
        if (options.isCancelled && options.isCancelled()) {
            error = "LAZ read cancelled";
            return false;
        }
        std::vector<usdlas::LasPoint> points;
        complete = false;
        if (!decoder_->ReadChunk(maximumPoints, points, complete,
                                 error)) {
            return false;
        }
        if (points.empty() && !complete) {
            error = "LAZ decoder returned an empty incomplete chunk";
            return false;
        }
        if (points.size() > maximumPoints) {
            error = "LAZ decoder exceeded the requested chunk size";
            return false;
        }
        if (points.size() > header.pointCount - pointsRead ||
            pointsRead > std::numeric_limits<std::uint64_t>::max() -
                              points.size()) {
            error = "LAZ decoder returned too many points";
            return false;
        }
        const auto chunkStart = pointsRead;
        pointsRead += points.size();
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
                error = "LAZ chunk consumer rejected a chunk";
                return false;
            }
        }
    }
    if (pointsRead != header.pointCount ||
        selectedPointsRead != rangeEnd - options.range.firstPoint) {
        error = "LAZ decoder point count does not match the header";
        return false;
    }
    return true;
}

bool LazReader::Read(
    const LazReadOptions& options,
    const LazPointChunkConsumer& consume,
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    if (!decoder_) {
        AddDiagnostic("LAZ decoder is not configured", diagnostics);
        return false;
    }
    if (!options.IsValid() || !consume) {
        AddDiagnostic("LAZ read options or consumer are invalid", diagnostics);
        return false;
    }
    if (!decoder_->ReadHeader(header, diagnostics)) {
        return false;
    }

    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount > header.pointCount -
                                        options.range.firstPoint)) {
        AddDiagnostic("LAZ point range is outside the header", diagnostics);
        return false;
    }

    const auto rangeEnd = options.range.pointCount == 0
                              ? header.pointCount
                              : options.range.firstPoint +
                                    options.range.pointCount;
    const auto maximumSize = (std::numeric_limits<std::size_t>::max)();
    if (header.pointRecordLength >
        (maximumSize - sizeof(usdlas::LasPoint)) / 2) {
        AddDiagnostic("LAZ point record size is invalid", diagnostics);
        return false;
    }
    const auto bytesPerPoint =
        sizeof(usdlas::LasPoint) +
        static_cast<std::size_t>(header.pointRecordLength) * 2;
    const auto budgetPointLimit = options.memoryBudgetBytes / bytesPerPoint;
    const auto maximumPoints =
        (std::min)(options.chunkPointLimit, budgetPointLimit);
    if (maximumPoints == 0) {
        AddDiagnostic("LAZ memory budget is too small for one point",
                      diagnostics);
        return false;
    }

    std::uint64_t pointsRead = 0;
    std::uint64_t selectedPointsRead = 0;
    bool complete = false;
    while (!complete) {
        if (options.isCancelled && options.isCancelled()) {
            AddDiagnostic("LAZ read cancelled", pointsRead, diagnostics);
            return false;
        }
        std::vector<usdlas::LasPoint> points;
        complete = false;
        if (!decoder_->ReadChunk(maximumPoints, points, complete,
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
        if (points.size() > maximumPoints) {
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
        const auto chunkStart = pointsRead;
        pointsRead += points.size();
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
                AddDiagnostic("LAZ chunk consumer rejected a chunk",
                              pointsRead, diagnostics);
                return false;
            }
        }
    }
    if (pointsRead != header.pointCount ||
        selectedPointsRead != rangeEnd - options.range.firstPoint) {
        AddDiagnostic("LAZ decoder point count does not match the header",
                      pointsRead, diagnostics);
        return false;
    }
    return true;
}

bool LazReader::Read(
    const LazReadOptions& options,
    const LazPointChunkErrorConsumer& consume,
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string callbackError;
    const LazPointChunkConsumer bridge =
        [&](const usdlas::LasHeader& chunkHeader,
            const std::vector<usdlas::LasPoint>& points) {
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

} // namespace usdlaz
