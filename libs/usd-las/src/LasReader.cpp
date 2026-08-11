#include "LasInternal.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace usdlas::detail {

bool ReadFileRange(usdgeo::RandomAccessSource& source,
                   std::uint64_t offset,
                   std::size_t size,
                   std::vector<std::uint8_t>& bytes,
                   std::string& error,
                   RangeReadFailure& failure,
                   std::vector<usdgeo::Diagnostic>* diagnostics) {
    failure = RangeReadFailure::None;
    if (offset > static_cast<std::uint64_t>(
                     (std::numeric_limits<std::streamoff>::max)())) {
        failure = RangeReadFailure::InvalidOffset;
        error = "LAS byte range offset is invalid";
        if (diagnostics != nullptr) {
            diagnostics->push_back(
                {usdgeo::DiagnosticCode::InvalidOffset,
                 usdgeo::Severity::Error, error, offset, std::nullopt});
        }
        return false;
    }
    std::vector<usdgeo::Diagnostic> sourceDiagnostics;
    if (!source.Read(offset, size, bytes, sourceDiagnostics)) {
        failure = sourceDiagnostics.empty() ||
                          sourceDiagnostics.front().code ==
                              usdgeo::DiagnosticCode::InvalidOffset
                      ? RangeReadFailure::InvalidOffset
                      : RangeReadFailure::Read;
        error = sourceDiagnostics.empty() ? "LAS byte range read failed"
                                           : sourceDiagnostics.front().message;
        if (diagnostics != nullptr) {
            if (sourceDiagnostics.empty()) {
                diagnostics->push_back(
                    {usdgeo::DiagnosticCode::DecodeFailure,
                     usdgeo::Severity::Error, error, offset, std::nullopt});
            } else {
                diagnostics->insert(diagnostics->end(),
                                    sourceDiagnostics.begin(),
                                    sourceDiagnostics.end());
            }
        }
        return false;
    }
    if (bytes.size() != size) {
        failure = RangeReadFailure::Read;
        error = "LAS byte range read returned a short buffer";
        if (diagnostics != nullptr) {
            diagnostics->push_back(
                {usdgeo::DiagnosticCode::TruncatedRecord,
                 usdgeo::Severity::Error, error, offset, std::nullopt});
        }
        return false;
    }
    return true;
}

} // namespace usdlas::detail

namespace usdlas {

namespace {

LasReadFailure SourceFailureKind(
    const std::vector<usdgeo::Diagnostic>& diagnostics,
    LasReadFailure fallback) {
    if (!diagnostics.empty()) {
        switch (diagnostics.front().code) {
        case usdgeo::DiagnosticCode::SourceOpenFailed:
            return LasReadFailure::FileOpen;
        case usdgeo::DiagnosticCode::SourceSizeUnavailable:
            return LasReadFailure::FileSize;
        default:
            break;
        }
    }
    return fallback;
}

} // namespace

LasReader::LasReader(std::string filename)
    : filename_(std::move(filename)),
      source_(std::make_shared<usdgeo::LocalRandomAccessSource>(filename_)) {}

LasReader::LasReader(std::shared_ptr<usdgeo::RandomAccessSource> source)
    : source_(std::move(source)) {}

LasReadFailure LasReader::FailureKind() const noexcept {
    return failureKind_;
}

bool LasReader::ReadMetadata(
    LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics,
    std::size_t memoryBudgetBytes) {
    diagnostics.clear();
    failureDiagnostic_.reset();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;

    if (!source_) {
        failureKind_ = LasReadFailure::FileOpen;
        diagnostics.push_back({usdgeo::DiagnosticCode::DecodeFailure,
                               usdgeo::Severity::Error,
                               "LAS random-access source is missing",
                               std::nullopt, std::nullopt});
        return false;
    }
    std::uint64_t fileSize = 0;
    if (!source_->GetSize(fileSize, diagnostics)) {
        failureKind_ = SourceFailureKind(diagnostics, LasReadFailure::FileSize);
        if (diagnostics.empty()) {
            diagnostics.push_back(
                {usdgeo::DiagnosticCode::DecodeFailure,
                 usdgeo::Severity::Error,
                 "could not determine LAS file size", std::nullopt,
                 std::nullopt});
        }
        return false;
    }
    std::vector<std::uint8_t> bytes;
    detail::RangeReadFailure rangeFailure;
    const auto headerReadSize = (std::min)(fileSize, std::uint64_t{375});
    std::string error;
    if (!detail::ReadFileRange(*source_, 0,
                               static_cast<std::size_t>(headerReadSize), bytes,
                               error, rangeFailure, &diagnostics)) {
        failureKind_ = SourceFailureKind(diagnostics, LasReadFailure::Header);
        failureDiagnostic_ = diagnostics.back();
        return false;
    }
    if (!InspectHeader(bytes, header, error)) {
        failureKind_ = LasReadFailure::Header;
        detail::AddErrorDiagnostic(error, diagnostics);
        return false;
    }
    if (header.pointDataOffset > fileSize ||
        header.pointDataOffset > memoryBudgetBytes) {
        failureKind_ = header.pointDataOffset > fileSize
                            ? LasReadFailure::Vlr
                            : LasReadFailure::Other;
        error = header.pointDataOffset > fileSize
                    ? "LAS point data offset is outside the file"
                    : "LAS metadata exceeds the configured memory budget";
        detail::AddErrorDiagnostic(error, diagnostics);
        return false;
    }
        if (!detail::ReadFileRange(
            *source_, 0, static_cast<std::size_t>(header.pointDataOffset), bytes,
            error, rangeFailure, &diagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (!InspectRecords(bytes, header.headerSize,
                        header.variableLengthRecordCount, false,
                        header.variableLengthRecords, diagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset > fileSize) {
            failureKind_ = LasReadFailure::EvlrOffset;
            error = "LAS extended variable-length record offset is invalid";
            detail::AddErrorDiagnostic(error, diagnostics);
            return false;
        }
        const auto extendedByteCount =
            fileSize - header.firstExtendedVariableLengthRecordOffset;
        if (extendedByteCount > memoryBudgetBytes - header.pointDataOffset) {
            failureKind_ = LasReadFailure::Other;
            error = "LAS metadata exceeds the configured memory budget";
            detail::AddErrorDiagnostic(error, diagnostics);
            return false;
        }
        std::vector<std::uint8_t> extendedBytes;
        if (!detail::ReadFileRange(
            *source_, header.firstExtendedVariableLengthRecordOffset,
                static_cast<std::size_t>(extendedByteCount), extendedBytes,
                error, rangeFailure, &diagnostics)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
        if (!InspectRecords(extendedBytes, 0,
                            header.extendedVariableLengthRecordCount, true,
                            header.variableLengthRecords, diagnostics)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
    }
    if (!ParseKnownMetadata(header.variableLengthRecords, header, diagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    return true;
}

bool LasReader::ReadPoints(const LasReadOptions& options,
                           const LasPointChunkConsumer& consume,
                           const LasHeader& header,
                           std::string& error) {
    error.clear();
    failureDiagnostic_.reset();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;
    if (!options.IsValid() || !consume) {
        failureKind_ = LasReadFailure::InvalidRequest;
        error = "LAS read options or consumer are invalid";
        return false;
    }

    if (!source_) {
        failureKind_ = LasReadFailure::FileOpen;
        error = "LAS random-access source is missing";
        return false;
    }
    std::vector<usdgeo::Diagnostic> sourceDiagnostics;
    std::uint64_t fileSize = 0;
    if (!source_->GetSize(fileSize, sourceDiagnostics)) {
        failureKind_ = SourceFailureKind(sourceDiagnostics,
                                         LasReadFailure::FileSize);
        if (!sourceDiagnostics.empty()) {
            failureDiagnostic_ = sourceDiagnostics.front();
        }
        error = sourceDiagnostics.empty() ? "could not determine LAS file size"
                                          : sourceDiagnostics.front().message;
        return false;
    }

    detail::RangeReadFailure rangeFailure;
    std::vector<std::uint8_t> bytes;
    std::vector<usdgeo::Diagnostic> rangeDiagnostics;
    if (!detail::ReadFileRange(*source_, 104, 1, bytes, error,
                               rangeFailure, &rangeDiagnostics)) {
        failureKind_ = SourceFailureKind(rangeDiagnostics,
                         LasReadFailure::Header);
        if (!rangeDiagnostics.empty()) {
            failureDiagnostic_ = rangeDiagnostics.front();
        }
        return false;
    }
    if ((bytes.front() & 0xc0) != 0) {
        failureKind_ = LasReadFailure::Header;
        error = "LAS point reader cannot decode compressed point data";
        return false;
    }
    const auto recordLength =
        static_cast<std::uint64_t>(header.pointRecordLength);
    if (recordLength == 0 ||
        header.pointCount >
            (fileSize - header.pointDataOffset) / recordLength) {
        failureKind_ = LasReadFailure::PointDataTruncated;
        error = "LAS point data is truncated";
        return false;
    }
    if (options.range.firstPoint > header.pointCount ||
        (options.range.pointCount != 0 &&
         options.range.pointCount >
             header.pointCount - options.range.firstPoint)) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS point range is outside the header";
        return false;
    }

    const auto rangeEnd = options.range.pointCount == 0
                              ? header.pointCount
                              : options.range.firstPoint +
                                    options.range.pointCount;
    if (recordLength >
        (std::numeric_limits<std::size_t>::max)() / 2 - sizeof(LasPoint)) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS point record size is invalid";
        return false;
    }
    const auto bytesPerPoint = static_cast<std::size_t>(recordLength) * 2 +
                               sizeof(LasPoint);
    const auto budgetPointLimit = options.memoryBudgetBytes / bytesPerPoint;
    const auto maximumPoints =
        (std::min)(options.chunkPointLimit, budgetPointLimit);
    if (maximumPoints == 0) {
        failureKind_ = LasReadFailure::Other;
        error = "LAS memory budget is too small for one point";
        return false;
    }

    std::uint64_t pointsRead = options.range.firstPoint;
    while (pointsRead < rangeEnd) {
        if (options.isCancelled && options.isCancelled()) {
            failureKind_ = LasReadFailure::Other;
            error = "LAS read cancelled";
            return false;
        }
        const auto remaining = rangeEnd - pointsRead;
        const auto count =
            (std::min)(static_cast<std::uint64_t>(maximumPoints), remaining);
        const auto byteOffset = header.pointDataOffset +
                                pointsRead * recordLength;
        const auto byteCount = static_cast<std::size_t>(count * recordLength);
        rangeDiagnostics.clear();
        if (!detail::ReadFileRange(*source_, byteOffset, byteCount, bytes, error,
                                   rangeFailure, &rangeDiagnostics)) {
            failureKind_ = rangeFailure == detail::RangeReadFailure::Seek
                               ? LasReadFailure::PointDataSeek
                               : LasReadFailure::PointDataRead;
            if (!rangeDiagnostics.empty()) {
                failureDiagnostic_ = rangeDiagnostics.front();
            }
            failureByteOffset_ = byteOffset;
            return false;
        }

        std::vector<LasPoint> points;
        points.reserve(static_cast<std::size_t>(count));
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(count); ++index) {
            std::vector<std::uint8_t> record(
                bytes.begin() + index * static_cast<std::size_t>(recordLength),
                bytes.begin() + (index + 1) *
                                    static_cast<std::size_t>(recordLength));
            LasPoint point;
            if (!DecodePoint(header, record, point, error)) {
                failureKind_ = LasReadFailure::PointDecode;
                failureByteOffset_ =
                    byteOffset + static_cast<std::uint64_t>(index) * recordLength;
                failurePointIndex_ = pointsRead + index;
                return false;
            }
            if (MatchesReadOptions(point, options)) {
                points.push_back(point);
            }
        }

        pointsRead += count;
        if (!points.empty() && !consume(header, points)) {
            failureKind_ = LasReadFailure::Other;
            error = "LAS chunk consumer rejected a chunk";
            return false;
        }
    }
    return true;
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkConsumer& consume,
                     LasHeader& header,
                     std::string& error) {
    error.clear();
    failureDiagnostic_.reset();
    failureByteOffset_.reset();
    failurePointIndex_.reset();
    failureKind_ = LasReadFailure::None;
    if (!options.IsValid() || !consume) {
        failureKind_ = LasReadFailure::InvalidRequest;
        error = "LAS read options or consumer are invalid";
        return false;
    }

    if (!source_) {
        failureKind_ = LasReadFailure::FileOpen;
        error = "LAS random-access source is missing";
        return false;
    }
    std::vector<usdgeo::Diagnostic> sourceDiagnostics;
    std::uint64_t fileSize = 0;
    if (!source_->GetSize(fileSize, sourceDiagnostics)) {
        failureKind_ = SourceFailureKind(sourceDiagnostics,
                                         LasReadFailure::FileSize);
        if (!sourceDiagnostics.empty()) {
            failureDiagnostic_ = sourceDiagnostics.front();
        }
        error = sourceDiagnostics.empty() ? "could not determine LAS file size"
                                          : sourceDiagnostics.front().message;
        return false;
    }

    std::vector<std::uint8_t> bytes;
    const auto headerReadSize = (std::min)(fileSize, std::uint64_t{375});
    detail::RangeReadFailure rangeFailure;
    std::vector<usdgeo::Diagnostic> rangeDiagnostics;
    if (!detail::ReadFileRange(*source_, 0,
                               static_cast<std::size_t>(headerReadSize), bytes,
                               error, rangeFailure, &rangeDiagnostics)) {
        failureKind_ = LasReadFailure::Header;
        if (!rangeDiagnostics.empty()) {
            failureDiagnostic_ = rangeDiagnostics.front();
        }
        return false;
    }
    if (!InspectHeader(bytes, header, error)) {
        failureKind_ = LasReadFailure::Header;
        return false;
    }

    if (header.pointDataOffset > fileSize) {
        failureKind_ = LasReadFailure::PointDataTruncated;
        error = "LAS point data offset is outside the file";
        return false;
    }
        if (!detail::ReadFileRange(
            *source_, 0, static_cast<std::size_t>(header.pointDataOffset), bytes,
            error, rangeFailure, &rangeDiagnostics)) {
        failureKind_ = LasReadFailure::Vlr;
        if (!rangeDiagnostics.empty()) {
            failureDiagnostic_ = rangeDiagnostics.front();
        }
        return false;
    }
    if (!InspectRecords(bytes, header.headerSize,
                        header.variableLengthRecordCount, false,
                        header.variableLengthRecords, error)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        if (header.firstExtendedVariableLengthRecordOffset > fileSize) {
            failureKind_ = LasReadFailure::EvlrOffset;
            error = "LAS extended variable-length record offset is invalid";
            return false;
        }
        std::vector<std::uint8_t> extendedBytes;
        rangeDiagnostics.clear();
        if (!detail::ReadFileRange(
            *source_, header.firstExtendedVariableLengthRecordOffset,
                static_cast<std::size_t>(
                    fileSize - header.firstExtendedVariableLengthRecordOffset),
                extendedBytes, error, rangeFailure, &rangeDiagnostics)) {
            failureKind_ = LasReadFailure::Evlr;
            if (!rangeDiagnostics.empty()) {
                failureDiagnostic_ = rangeDiagnostics.front();
            }
            return false;
        }
        if (!InspectRecords(extendedBytes, 0,
                            header.extendedVariableLengthRecordCount, true,
                            header.variableLengthRecords, error)) {
            failureKind_ = LasReadFailure::Evlr;
            return false;
        }
    }
    if (!ParseKnownMetadata(header.variableLengthRecords, header, error)) {
        failureKind_ = LasReadFailure::Vlr;
        return false;
    }
    return ReadPoints(options, consume, header, error);
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkConsumer& consume,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (Read(options, consume, header, error)) {
        return true;
    }
    if (failureDiagnostic_) {
        diagnostics.push_back(*failureDiagnostic_);
        return false;
    }
    diagnostics.push_back({detail::CodeForError(error), usdgeo::Severity::Error,
                           error, failureByteOffset_, failurePointIndex_});
    return false;
}

bool LasReader::Read(const LasReadOptions& options,
                     const LasPointChunkErrorConsumer& consume,
                     LasHeader& header,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string callbackError;
    const LasPointChunkConsumer bridge =
        [&](const LasHeader& chunkHeader,
            const std::vector<LasPoint>& points) {
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

} // namespace usdlas