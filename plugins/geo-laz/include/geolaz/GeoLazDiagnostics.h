#pragma once

#include <string>

namespace geolaz::diagnostics {

inline constexpr const char* InvalidReadRequest = "LAZ001";
inline constexpr const char* FileOpenFailed = "LAZ002";
inline constexpr const char* DecodeFailed = "LAZ003";
inline constexpr const char* BoundsTransformFailed = "LAZ004";
inline constexpr const char* UsdLayerCreateFailed = "LAZ005";
inline constexpr const char* StageMetricsFailed = "LAZ006";
inline constexpr const char* PointCloudAuthorFailed = "LAZ007";
inline constexpr const char* FormatArgumentInvalid = "LAZ008";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace geolaz::diagnostics