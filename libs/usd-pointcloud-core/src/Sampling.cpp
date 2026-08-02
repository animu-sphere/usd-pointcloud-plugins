#include "usdpointcloud/Sampling.h"

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
    return true;
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