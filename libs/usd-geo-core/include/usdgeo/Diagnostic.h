#pragma once

#include <cstdint>
#include <optional>
#include <string>

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
    DecodeFailure,
    UnknownFormatArgument,
    UnsupportedFormatArgument,
    InvalidFormatArgument,
    ConflictingFormatArguments,
    InvalidPointTileId,
    InvalidPointSourceRange,
    InvalidLodItem,
    InvalidLodHierarchy,
    InvalidPointTile,
};

struct Diagnostic {
    DiagnosticCode code = DiagnosticCode::DecodeFailure;
    Severity severity = Severity::Error;
    std::string message;
    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pointIndex;
};

} // namespace usdgeo