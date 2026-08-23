#pragma once

#include "usdgeo/cache/Cache.h"

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

// The projection itself, so what a test asserts is what OpenUSD is told. The
// message text is fixed by `usdgeo::cache` and the category name is an
// enumerated constant, so neither can carry a resolved identifier, a
// validation token, or any transport detail.
inline const char* DecisionCode(usdgeo::cache::CacheDecision decision) {
    switch (decision) {
    case usdgeo::cache::CacheDecision::IdentityStable:
    case usdgeo::cache::CacheDecision::Hit:
        return ResolverCacheReusePermitted;
    case usdgeo::cache::CacheDecision::IdentityChanged:
        return ResolverIdentityChanged;
    case usdgeo::cache::CacheDecision::Invalidated:
        return ResolverCacheInvalidated;
    case usdgeo::cache::CacheDecision::IdentityUnavailable:
    case usdgeo::cache::CacheDecision::IdentityUnstable:
    case usdgeo::cache::CacheDecision::ReuseDisabled:
        break;
    }
    return ResolverCacheReuseDisabled;
}

// A decision that removed or refused something is a warning; one that reports
// what reuse did is a status.
inline bool DecisionIsWarning(usdgeo::cache::CacheDecision decision) {
    const auto* code = DecisionCode(decision);
    return code == ResolverCacheReuseDisabled ||
           code == ResolverCacheInvalidated;
}

inline std::string DecisionMessage(usdgeo::cache::CacheDecision decision) {
    return Message(
        DecisionCode(decision),
        std::string(usdgeo::cache::CacheDecisionMessage(decision)) + " (" +
            usdgeo::cache::CacheDecisionName(decision) + ")");
}

} // namespace usdgeocopc::diagnostics
