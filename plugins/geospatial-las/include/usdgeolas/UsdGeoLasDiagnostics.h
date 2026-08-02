#pragma once

#include <string>

namespace usdgeolas::diagnostics {

inline constexpr const char* InvalidReadRequest = "LAS001";
inline constexpr const char* FileOpenFailed = "LAS002";
inline constexpr const char* FileSizeUnavailable = "LAS003";
inline constexpr const char* HeaderReadFailed = "LAS004";
inline constexpr const char* HeaderInvalid = "LAS005";
inline constexpr const char* VlrInvalid = "LAS006";
inline constexpr const char* EvlrOffsetInvalid = "LAS007";
inline constexpr const char* EvlrInvalid = "LAS008";
inline constexpr const char* PointDataTruncated = "LAS009";
inline constexpr const char* PointDataSeekFailed = "LAS010";
inline constexpr const char* PointReadFailed = "LAS011";
inline constexpr const char* PointDecodeFailed = "LAS012";
inline constexpr const char* BoundsTransformFailed = "LAS013";
inline constexpr const char* UsdLayerCreateFailed = "LAS014";
inline constexpr const char* StageMetricsFailed = "LAS015";
inline constexpr const char* PointCloudAuthorFailed = "LAS016";
inline constexpr const char* FormatArgumentInvalid = "LAS017";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace usdgeolas::diagnostics