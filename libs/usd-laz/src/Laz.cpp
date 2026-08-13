#include "usdlaz/Laz.h"

#include "usdpointcloud/FileFormatArguments.h"

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

void AddStreamDiagnostic(usdgeo::DiagnosticCode code,
                         const std::string& message,
                         std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message,
                           std::nullopt, std::nullopt});
}

} // namespace

namespace usdlaz {

bool ReadPointCloud(const std::string& filename,
                    const usdpointcloud::PointReadOptions& options,
                    const std::vector<std::string>& attributes,
                    const std::string& missingCrsMessage,
                    usdpointcloud::PointCloudAsset& asset,
                    LazReadFailure& failure,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    asset = {};
    failure = LazReadFailure::None;
    auto decoder = CreateFileDecoder(filename, diagnostics);
    if (!decoder) {
        failure = LazReadFailure::FileOpen;
        return false;
    }

    LazReader reader(std::move(decoder));
    usdlas::LasHeader header;
    usdpointcloud::PointData pointData;
    const auto consume = [&](const usdlas::LasHeader& chunkHeader,
                             const std::vector<usdlas::LasPoint>& points,
                             std::string& error) {
        return usdlas::AppendPointData(chunkHeader, points, filename,
                                       pointData, error);
    };
    if (!reader.Read(options, consume, header, diagnostics)) {
        failure = LazReadFailure::Decode;
        return false;
    }

    std::string selectionError;
    if (!usdpointcloud::SelectPointDataAttributes(
            pointData, attributes, selectionError)) {
        diagnostics.clear();
        diagnostics.push_back({usdgeo::DiagnosticCode::InvalidFormatArgument,
                               usdgeo::Severity::Error, selectionError,
                               std::nullopt, std::nullopt});
        failure = LazReadFailure::InvalidRequest;
        return false;
    }
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(header, pointData, missingCrsMessage,
                                      asset, assetError)) {
        diagnostics.clear();
        diagnostics.push_back({usdgeo::DiagnosticCode::DecodeFailure,
                               usdgeo::Severity::Error, assetError,
                               std::nullopt, std::nullopt});
        failure = LazReadFailure::Asset;
        return false;
    }
    return true;
}

LazPointStream::LazPointStream(std::unique_ptr<LazDecoder> decoder,
                               LazReadOptions options,
                               usdlas::LasHeader header,
                               std::size_t maximumPoints)
    : decoder_(std::move(decoder)),
      options_(std::move(options)),
      header_(std::move(header)),
      endPoint_(options_.range.pointCount == 0
                    ? header_.pointCount
                    : options_.range.firstPoint + options_.range.pointCount),
      maximumPoints_(maximumPoints) {}

LazPointStream::~LazPointStream() = default;

usdpointcloud::PointStreamStatus LazPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    chunk = {};
    data = {};
    diagnostic = {};
    if (ended_) return usdpointcloud::PointStreamStatus::End;

    while (!complete_) {
        if (options_.isCancelled && options_.isCancelled()) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, "LAZ read cancelled",
                          std::nullopt, pointsRead_};
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        std::vector<usdlas::LasPoint> points;
        std::vector<usdgeo::Diagnostic> chunkDiagnostics;
        if (!decoder_->ReadChunk(maximumPoints_, points, complete_,
                                 chunkDiagnostics)) {
            diagnostic = chunkDiagnostics.empty()
                             ? usdgeo::Diagnostic{
                                   usdgeo::DiagnosticCode::DecodeFailure,
                                   usdgeo::Severity::Error,
                                   "LAZ decoder failed", std::nullopt,
                                   pointsRead_}
                             : chunkDiagnostics.front();
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        if (points.empty() && !complete_) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error,
                          "LAZ decoder returned an empty incomplete chunk",
                          std::nullopt, pointsRead_};
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        if (points.size() > maximumPoints_ ||
            points.size() > header_.pointCount - pointsRead_) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error,
                          "LAZ decoder returned too many points", std::nullopt,
                          pointsRead_};
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        const auto chunkStart = pointsRead_;
        pointsRead_ += points.size();
        const auto selectedStart =
            (std::max)(chunkStart, options_.range.firstPoint);
        const auto selectedEnd = (std::min)(pointsRead_, endPoint_);
        if (selectedStart >= selectedEnd) continue;
        const auto first = static_cast<std::size_t>(selectedStart - chunkStart);
        const auto last = static_cast<std::size_t>(selectedEnd - chunkStart);
        points.erase(points.begin(), points.begin() + first);
        points.erase(points.begin() + (last - first), points.end());
        points.erase(
            std::remove_if(points.begin(), points.end(),
                           [&](const usdlas::LasPoint& point) {
                               return !usdlas::MatchesReadOptions(point, options_);
                           }),
            points.end());
        if (points.empty()) continue;
        usdpointcloud::PointData pointData;
        std::string error;
        if (!usdlas::AppendPointData(header_, points, {}, pointData, error)) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, error, std::nullopt,
                          selectedStart};
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
        for (const auto& position : pointData.positions) bounds.Expand(position);
        chunk = usdpointcloud::MakePointChunk(pointData, bounds);
        if (!chunk.IsValid() || chunk.pointCount == 0) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error,
                          "LAZ stream produced an invalid point chunk",
                          std::nullopt, selectedStart};
            ended_ = true;
            return usdpointcloud::PointStreamStatus::Error;
        }
        data = std::move(pointData);
        return usdpointcloud::PointStreamStatus::Chunk;
    }

    if (pointsRead_ != header_.pointCount) {
        diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                      usdgeo::Severity::Error,
                      "LAZ decoder point count does not match the header",
                      std::nullopt, pointsRead_};
        ended_ = true;
        return usdpointcloud::PointStreamStatus::Error;
    }
    ended_ = true;
    return usdpointcloud::PointStreamStatus::End;
}

const usdlas::LasHeader& LazPointStream::Header() const noexcept {
    return header_;
}

std::uint64_t LazPointStream::SourceBytesRead() const noexcept {
    return decoder_ ? decoder_->SourceBytesRead() : 0;
}

std::unique_ptr<LazPointStream> OpenLazPointStream(
    const std::string& filename,
    const LazReadOptions& options,
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    header = {};
    if (!options.IsValid()) {
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "LAZ read options are invalid", diagnostics);
        return nullptr;
    }
    auto decoder = CreateFileDecoder(filename, diagnostics);
    if (!decoder || !decoder->ReadHeader(header, diagnostics)) return nullptr;
    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount > header.pointCount - options.range.firstPoint)) {
        diagnostics.clear();
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidPointSourceRange,
                            "LAZ point range is outside the header",
                            diagnostics);
        return nullptr;
    }
    const auto maximumSize = (std::numeric_limits<std::size_t>::max)();
    if (header.pointRecordLength >
        (maximumSize - sizeof(usdlas::LasPoint)) / 2) {
        AddDiagnostic("LAZ point record size is invalid", diagnostics);
        return nullptr;
    }
    const auto bytesPerPoint = sizeof(usdlas::LasPoint) +
                               static_cast<std::size_t>(header.pointRecordLength) * 2;
    const auto maximumPoints = (std::min)(options.chunkPointLimit,
                                          options.memoryBudgetBytes / bytesPerPoint);
    if (maximumPoints == 0) {
        diagnostics.clear();
        AddStreamDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                            "LAZ memory budget is too small for one point",
                            diagnostics);
        return nullptr;
    }
    return std::unique_ptr<LazPointStream>(new LazPointStream(
        std::move(decoder), options, header, maximumPoints));
}

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

bool LazReader::ReadMetadata(
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    if (!decoder_) {
        AddDiagnostic("LAZ decoder is not configured", diagnostics);
        return false;
    }
    return decoder_->ReadHeader(header, diagnostics);
}

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
            points.erase(
                std::remove_if(points.begin(), points.end(),
                               [&](const usdlas::LasPoint& point) {
                                   return !usdlas::MatchesReadOptions(point,
                                                                       options);
                               }),
                points.end());
            if (!points.empty() && !consume(header, points)) {
                error = "LAZ chunk consumer rejected a chunk";
                return false;
            }
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
            points.erase(
                std::remove_if(points.begin(), points.end(),
                               [&](const usdlas::LasPoint& point) {
                                   return !usdlas::MatchesReadOptions(point,
                                                                       options);
                               }),
                points.end());
            if (!points.empty() && !consume(header, points)) {
                AddDiagnostic("LAZ chunk consumer rejected a chunk",
                              pointsRead, diagnostics);
                return false;
            }
        }
    }
    if (pointsRead != header.pointCount) {
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
