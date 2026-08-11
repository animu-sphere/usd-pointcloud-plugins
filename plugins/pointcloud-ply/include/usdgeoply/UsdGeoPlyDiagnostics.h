#pragma once

#include <string>

namespace usdgeoply::diagnostics {

inline constexpr const char* InvalidReadRequest = "PLY001";
inline constexpr const char* FormatArgumentInvalid = "PLY002";
inline constexpr const char* CrsRequired = "PLY003";
inline constexpr const char* DecodeFailed = "PLY004";
inline constexpr const char* UsdLayerCreateFailed = "PLY005";
inline constexpr const char* StageMetricsFailed = "PLY006";
inline constexpr const char* PointCloudAuthorFailed = "PLY007";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace usdgeoply::diagnostics