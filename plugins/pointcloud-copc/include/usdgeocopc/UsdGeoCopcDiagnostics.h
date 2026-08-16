#pragma once

#include <string>

namespace usdgeocopc::diagnostics {

inline constexpr const char* InvalidReadRequest = "COPC001";
inline constexpr const char* FileOpenFailed = "COPC002";
inline constexpr const char* DecodeFailed = "COPC003";
inline constexpr const char* BoundsTransformFailed = "COPC004";
inline constexpr const char* UsdLayerCreateFailed = "COPC005";
inline constexpr const char* StageMetricsFailed = "COPC006";
inline constexpr const char* PointCloudAuthorFailed = "COPC007";
inline constexpr const char* FormatArgumentInvalid = "COPC008";
inline constexpr const char* ResolverCacheReuseDisabled = "COPC009";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace usdgeocopc::diagnostics
