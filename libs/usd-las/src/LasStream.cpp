#include "LasInternal.h"

#include <algorithm>
#include <limits>

namespace usdlas {

namespace {

std::size_t EffectiveChunkPointLimit(const LasHeader& header,
                                     const LasReadOptions& options) {
    const auto recordLength =
        static_cast<std::uint64_t>(header.pointRecordLength);
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
        diagnostic = {detail::CodeForError(error), usdgeo::Severity::Error,
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

} // namespace usdlas