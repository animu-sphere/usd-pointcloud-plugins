#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usdgeo {

enum class Severity {
    Warning,
    Error,
};

enum class DiagnosticCode {
    InvalidSignature,
    UnsupportedVersion,
    UnsupportedPointFormat,
    TruncatedHeader,
    InvalidOffset,
    InvalidRecordLength,
    TruncatedRecord,
    InvalidCrs,
    ConflictingCrs,
    UnsupportedExtraBytesType,
    MissingWaveformData,
    NonFiniteCoordinate,
    NonFiniteExtraBytes,
    DecodeFailure,
    SourceOpenFailed,
    SourceSizeUnavailable,
    UnknownFormatArgument,
    UnsupportedFormatArgument,
    InvalidFormatArgument,
    ConflictingFormatArguments,
    InvalidPointTileId,
    InvalidPointSourceRange,
    InvalidLodItem,
    InvalidLodHierarchy,
    InvalidPointTile,
    // Tiled working storage. These describe the generator's own files rather
    // than the source, so a caller can tell a full disk, a refused workspace,
    // or a process out of file handles apart from bad input.
    SpoolWorkspaceFailed,
    SpoolWriteFailed,
    SpoolIncomplete,
    OpenFileBudgetExhausted,
    PayloadCommitFailed,
};

// True for the codes that report a failure of the generator's own working
// storage (tile spools, their workspace, or payload publication) rather than
// a problem with the source or the request.
constexpr bool IsWorkingStorageFailure(DiagnosticCode code) noexcept {
    switch (code) {
    case DiagnosticCode::SpoolWorkspaceFailed:
    case DiagnosticCode::SpoolWriteFailed:
    case DiagnosticCode::SpoolIncomplete:
    case DiagnosticCode::OpenFileBudgetExhausted:
    case DiagnosticCode::PayloadCommitFailed:
        return true;
    default:
        return false;
    }
}

struct Diagnostic {
    DiagnosticCode code = DiagnosticCode::DecodeFailure;
    Severity severity = Severity::Error;
    std::string message;
    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pointIndex;
};

// True when the first error, the one that stopped the operation, is a working
// storage failure. Later entries only add context to that first error.
inline bool StoppedByWorkingStorageFailure(
    const std::vector<Diagnostic>& diagnostics) noexcept {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.severity == Severity::Error) {
            return IsWorkingStorageFailure(diagnostic.code);
        }
    }
    return false;
}

} // namespace usdgeo