#include "usdpointcloud/Sampling.h"

#include <algorithm>

namespace usdpointcloud {
namespace {

void AddDiagnostic(const char* message,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back({usdgeo::DiagnosticCode::InvalidLodItem,
                           usdgeo::Severity::Error, message, std::nullopt,
                           std::nullopt});
}

template <typename T>
void SampleAttribute(const std::vector<T>& source,
                     const std::vector<std::size_t>& indices,
                     std::vector<T>& sampled) {
    sampled.clear();
    if (source.empty()) {
        return;
    }

    sampled.reserve(indices.size());
    for (const auto index : indices) {
        sampled.push_back(source[index]);
    }
}

void SampleExtraByteAttribute(const std::vector<double>& source,
                              std::uint8_t componentCount,
                              const std::vector<std::size_t>& indices,
                              std::vector<double>& sampled) {
    sampled.clear();
    if (source.empty()) {
        return;
    }

    sampled.reserve(indices.size() * componentCount);
    for (const auto index : indices) {
        const auto firstComponent = index * componentCount;
        sampled.insert(sampled.end(), source.begin() + firstComponent,
                       source.begin() + firstComponent + componentCount);
    }
}

} // namespace

bool PointSamplingOptions::IsValid(
    std::uint64_t sourcePointCount) const noexcept {
    return targetPointCount != 0 && targetPointCount <= sourcePointCount &&
           algorithm == kFixedStrideSamplingAlgorithm &&
           algorithmVersion == kFixedStrideSamplingVersion;
}

bool SamplePointData(const PointData& source,
                     const PointSamplingOptions& options,
                     PointData& sampled,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (&source == &sampled) {
        AddDiagnostic("source and sampled point data must be distinct",
                      diagnostics);
        return false;
    }
    if (!source.IsValid()) {
        AddDiagnostic("point data is invalid", diagnostics);
        return false;
    }

    const auto sourcePointCount = source.positions.size();
    if (!options.IsValid(sourcePointCount)) {
        AddDiagnostic("point sampling options are invalid", diagnostics);
        return false;
    }

    std::vector<std::size_t> indices;
    indices.reserve(static_cast<std::size_t>(options.targetPointCount));
    for (std::uint64_t index = 0; index < options.targetPointCount; ++index) {
        const auto sourceIndex = static_cast<std::size_t>(
            index * static_cast<std::uint64_t>(sourcePointCount) /
            options.targetPointCount);
        indices.push_back(sourceIndex);
    }

    SampleAttribute(source.positions, indices, sampled.positions);
    SampleAttribute(source.intensity, indices, sampled.intensity);
    SampleAttribute(source.returnNumber, indices, sampled.returnNumber);
    SampleAttribute(source.numberOfReturns, indices, sampled.numberOfReturns);
    SampleAttribute(source.classification, indices, sampled.classification);
    SampleAttribute(source.classificationFlags, indices,
                    sampled.classificationFlags);
    SampleAttribute(source.scannerChannel, indices, sampled.scannerChannel);
    SampleAttribute(source.scanDirectionFlag, indices,
                    sampled.scanDirectionFlag);
    SampleAttribute(source.edgeOfFlightLine, indices,
                    sampled.edgeOfFlightLine);
    SampleAttribute(source.userData, indices, sampled.userData);
    SampleAttribute(source.scanAngle, indices, sampled.scanAngle);
    SampleAttribute(source.pointSourceId, indices, sampled.pointSourceId);
    SampleAttribute(source.red, indices, sampled.red);
    SampleAttribute(source.green, indices, sampled.green);
    SampleAttribute(source.blue, indices, sampled.blue);
    sampled.colorBitDepth = source.colorBitDepth;
    SampleAttribute(source.nir, indices, sampled.nir);
    SampleAttribute(source.gpsTime, indices, sampled.gpsTime);
    SampleAttribute(source.waveformDescriptorIndex, indices,
                    sampled.waveformDescriptorIndex);
    SampleAttribute(source.waveformDataOffset, indices,
                    sampled.waveformDataOffset);
    SampleAttribute(source.waveformPacketSize, indices,
                    sampled.waveformPacketSize);
    SampleAttribute(source.returnPointWaveformLocation, indices,
                    sampled.returnPointWaveformLocation);
    SampleAttribute(source.waveformXt, indices, sampled.waveformXt);
    SampleAttribute(source.waveformYt, indices, sampled.waveformYt);
    SampleAttribute(source.waveformZt, indices, sampled.waveformZt);
    SampleAttribute(source.waveformDataExternal, indices,
                    sampled.waveformDataExternal);
    sampled.waveformDataFile = source.waveformDataFile;
    sampled.extraByteNames = source.extraByteNames;
    sampled.extraByteComponentCounts = source.extraByteComponentCounts;
    sampled.extraBytes.resize(source.extraBytes.size());
    for (std::size_t index = 0; index < source.extraBytes.size(); ++index) {
        const auto componentCount = source.extraByteComponentCounts.empty()
                                        ? std::uint8_t{1}
                                        : source.extraByteComponentCounts[index];
        SampleExtraByteAttribute(source.extraBytes[index], componentCount,
                                 indices, sampled.extraBytes[index]);
    }
    return true;
}

bool BuildPointLodAssets(const PointCloudAsset& source,
                         LodProfile profile,
                         std::vector<PointCloudAsset>& levels,
                         PointLodHierarchy& hierarchy,
                         std::vector<usdgeo::Diagnostic>& diagnostics) {
    levels.clear();
    hierarchy = {};
    diagnostics.clear();
    if (!source.IsValid() || profile == LodProfile::Off) {
        AddDiagnostic("LOD source asset or profile is invalid", diagnostics);
        return false;
    }

    const auto pointCount = source.data.positions.size();
    std::vector<std::uint64_t> targets;
    const auto addTarget = [&](std::uint64_t divisor) {
        targets.push_back(std::max<std::uint64_t>(
            1, (pointCount + divisor - 1) / divisor));
    };
    if (profile == LodProfile::Preview) {
        addTarget(4);
    } else if (profile == LodProfile::Balanced) {
        addTarget(16);
        addTarget(4);
    } else {
        addTarget(64);
        addTarget(16);
        addTarget(4);
    }
    targets.push_back(pointCount);
    std::reverse(targets.begin(), targets.end());

    for (std::size_t index = 0; index < targets.size(); ++index) {
        PointCloudAsset level = source;
        if (targets[index] != pointCount) {
            PointData sampled;
            PointSamplingOptions options;
            options.targetPointCount = targets[index];
            if (!SamplePointData(source.data, options, sampled, diagnostics)) {
                return false;
            }
            level.data = std::move(sampled);
            level.chunk = MakePointChunk(level.data, source.bounds);
        }
        levels.push_back(std::move(level));
    }

    hierarchy.bounds = source.bounds;
    hierarchy.defaultIndex = 0;
    hierarchy.screenSizeThresholds = {0.75F, 0.25F, 0.05F};
    hierarchy.screenSizeThresholds.resize(levels.size() - 1);
    for (std::size_t index = 0; index < levels.size(); ++index) {
        hierarchy.items.push_back(
            {static_cast<std::uint32_t>(index), levels[index].chunk.pointCount,
             source.bounds, {0, pointCount}});
    }
    return hierarchy.IsValid();
}

usdgeo::CacheArguments MakeSamplingCacheArguments(
    const PointSamplingOptions& options) {
    return {{"samplingAlgorithm", options.algorithm},
            {"samplingAlgorithmVersion",
             std::to_string(options.algorithmVersion)},
            {"samplingTargetPointCount",
             std::to_string(options.targetPointCount)}};
}

} // namespace usdpointcloud