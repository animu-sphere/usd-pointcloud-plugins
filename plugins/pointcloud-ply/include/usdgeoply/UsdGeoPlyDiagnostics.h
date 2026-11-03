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
// A tiled read stopped by its own working storage (tile spools, their
// workspace, the process file-handle budget, or payload publication) rather
// than by the source; see usdgeo::IsWorkingStorageFailure.
inline constexpr const char* TiledStorageFailed = "PLY008";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace usdgeoply::diagnostics