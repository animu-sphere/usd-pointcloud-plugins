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
// Generated-cache decision codes. Each one projects a group of the stable,
// transport-neutral categories `usdgeo::cache::CacheDecisionName` publishes;
// the emitted message always names the exact category it carries.
//
//   COPC009  reuse disabled     resolver-identity-unavailable
//                               resolver-identity-unstable
//                               generated-cache-reuse-disabled
//   COPC010  reuse permitted    resolver-identity-stable
//                               generated-cache-hit
//   COPC011  regeneration       resolver-identity-changed
//   COPC012  entry removed      generated-cache-invalidated
inline constexpr const char* ResolverCacheReuseDisabled = "COPC009";
inline constexpr const char* ResolverCacheReusePermitted = "COPC010";
inline constexpr const char* ResolverIdentityChanged = "COPC011";
inline constexpr const char* ResolverCacheInvalidated = "COPC012";

inline std::string Message(const char* code, const std::string& message) {
    return "[" + std::string(code) + "] " + message;
}

} // namespace usdgeocopc::diagnostics
