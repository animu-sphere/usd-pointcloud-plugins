#pragma once

#include "usdgeo/CacheKey.h"
#include "usdgeo/Diagnostic.h"
#include "usdpointcloud/PointCloud.h"

#include <cstdint>
#include <string>
#include <vector>

namespace usdpointcloud {

inline constexpr const char* kFixedStrideSamplingAlgorithm = "fixed-stride";
inline constexpr std::uint32_t kFixedStrideSamplingVersion = 1;

struct PointSamplingOptions {
    std::uint64_t targetPointCount = 0;
    std::string algorithm = kFixedStrideSamplingAlgorithm;
    std::uint32_t algorithmVersion = kFixedStrideSamplingVersion;

    bool IsValid(std::uint64_t sourcePointCount) const noexcept;
};

bool SamplePointData(const PointData& source,
                     const PointSamplingOptions& options,
                     PointData& sampled,
                     std::vector<usdgeo::Diagnostic>& diagnostics);

usdgeo::CacheArguments MakeSamplingCacheArguments(
    const PointSamplingOptions& options);

} // namespace usdpointcloud