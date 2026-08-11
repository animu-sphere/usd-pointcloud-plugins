#include "LasInternal.h"

#include "usdpointcloud/FileFormatArguments.h"

#include <filesystem>

namespace usdlas {

bool AppendPointData(const LasHeader& header,
                     const std::vector<LasPoint>& points,
                     const std::string& sourceFilename,
                     usdpointcloud::PointData& data,
                     std::string& error) {
    if (!header.IsValid() || !data.IsValid() ||
        data.positions.size() > header.pointCount ||
        points.size() > header.pointCount - data.positions.size()) {
        error = "LAS point data does not match the header";
        return false;
    }

    if (data.positions.empty()) {
        const auto pointCount = points.size();
        data.positions.reserve(pointCount);
        data.intensity.reserve(pointCount);
        data.returnNumber.reserve(pointCount);
        data.numberOfReturns.reserve(pointCount);
        data.classification.reserve(pointCount);
        if (header.pointFormat >= 6) {
            data.classificationFlags.reserve(pointCount);
            data.scannerChannel.reserve(pointCount);
        }
        data.scanDirectionFlag.reserve(pointCount);
        data.edgeOfFlightLine.reserve(pointCount);
        data.userData.reserve(pointCount);
        data.scanAngle.reserve(pointCount);
        data.pointSourceId.reserve(pointCount);
        if (header.pointFormat == 2 || header.pointFormat == 3 ||
            header.pointFormat == 7 || header.pointFormat == 8) {
            data.red.reserve(pointCount);
            data.green.reserve(pointCount);
            data.blue.reserve(pointCount);
        }
        if (header.pointFormat == 1 || header.pointFormat == 3 ||
            header.pointFormat >= 6) {
            data.gpsTime.reserve(pointCount);
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            data.nir.reserve(pointCount);
        }
        if (header.pointFormat == 4 || header.pointFormat == 5 ||
            header.pointFormat == 9 || header.pointFormat == 10) {
            data.waveformDescriptorIndex.reserve(pointCount);
            data.waveformDataOffset.reserve(pointCount);
            data.waveformPacketSize.reserve(pointCount);
            data.returnPointWaveformLocation.reserve(pointCount);
            data.waveformXt.reserve(pointCount);
            data.waveformYt.reserve(pointCount);
            data.waveformZt.reserve(pointCount);
            data.waveformDataExternal.reserve(pointCount);
        }
        data.extraByteNames.clear();
        data.extraByteNames.reserve(header.extraBytes.size());
        data.extraByteComponentCounts.clear();
        data.extraByteComponentCounts.reserve(header.extraBytes.size());
        data.extraBytes.clear();
        data.extraBytes.resize(header.extraBytes.size());
        std::vector<std::string> extraByteNames;
        extraByteNames.reserve(header.extraBytes.size());
        for (const auto& descriptor : header.extraBytes) {
            extraByteNames.push_back(descriptor.name);
            data.extraByteComponentCounts.push_back(
                detail::ExtraByteComponentCount(descriptor.dataType));
        }
        data.extraByteNames =
            usdpointcloud::NormalizeExtraByteNames(extraByteNames);
        for (auto& values : data.extraBytes) {
            values.reserve(pointCount);
        }
    }

    for (const auto& point : points) {
        data.positions.push_back(point.sourcePosition);
        data.intensity.push_back(point.intensity);
        data.returnNumber.push_back(point.returnNumber);
        data.numberOfReturns.push_back(point.numberOfReturns);
        data.classification.push_back(point.classification);
        if (header.pointFormat >= 6) {
            data.classificationFlags.push_back(point.classificationFlags);
            data.scannerChannel.push_back(point.scannerChannel);
        }
        data.scanDirectionFlag.push_back(point.scanDirectionFlag);
        data.edgeOfFlightLine.push_back(point.edgeOfFlightLine);
        data.userData.push_back(point.userData);
        data.scanAngle.push_back(point.scanAngle);
        data.pointSourceId.push_back(point.pointSourceId);
        if (point.hasColor) {
            data.red.push_back(point.red);
            data.green.push_back(point.green);
            data.blue.push_back(point.blue);
        }
        if (point.hasGpsTime) {
            data.gpsTime.push_back(point.gpsTime);
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            data.nir.push_back(point.nir);
        }
        if (point.hasWaveform) {
            data.waveformDescriptorIndex.push_back(point.waveform.descriptorIndex);
            data.waveformDataOffset.push_back(point.waveform.dataOffset);
            data.waveformPacketSize.push_back(point.waveform.packetSize);
            data.returnPointWaveformLocation.push_back(
                point.waveform.returnPointLocation);
            data.waveformXt.push_back(point.waveform.xt);
            data.waveformYt.push_back(point.waveform.yt);
            data.waveformZt.push_back(point.waveform.zt);
            data.waveformDataExternal.push_back(point.waveform.external ? 1 : 0);
            if (point.waveform.external && data.waveformDataFile.empty()) {
                auto waveformPath = std::filesystem::path(sourceFilename);
                waveformPath.replace_extension(".wdp");
                data.waveformDataFile = waveformPath.string();
            }
        }
        std::size_t extraByteValueCount = 0;
        for (const auto componentCount : data.extraByteComponentCounts) {
            extraByteValueCount += componentCount;
        }
        if (point.extraBytes.size() != extraByteValueCount) {
            error = "LAS point Extra Bytes do not match the header";
            return false;
        }
        std::size_t pointExtraByteIndex = 0;
        for (std::size_t index = 0; index < data.extraBytes.size(); ++index) {
            const auto componentCount = data.extraByteComponentCounts[index];
            data.extraBytes[index].insert(
                data.extraBytes[index].end(),
                point.extraBytes.begin() + pointExtraByteIndex,
                point.extraBytes.begin() + pointExtraByteIndex + componentCount);
            pointExtraByteIndex += componentCount;
        }
    }

    if (!data.IsValid()) {
        error = "LAS point attributes have inconsistent lengths";
        return false;
    }
    return true;
}

bool BuildPointCloudAsset(const LasHeader& header,
                          const usdpointcloud::PointData& data,
                          const std::string& missingCrsMessage,
                          usdpointcloud::PointCloudAsset& asset,
                          std::string& error) {
    if (!header.IsValid() || !data.IsValid() ||
        data.positions.empty() || data.positions.size() > header.pointCount) {
        error = "LAS point data does not match the header";
        return false;
    }

    asset = {};
    asset.reference.epsgCode = header.epsgCode;
    asset.reference.wkt = header.crsWkt.empty() ? missingCrsMessage
                                                 : header.crsWkt;
    asset.reference.sourceUpAxis = "Z";
    asset.reference.stageUpAxis = "Y";
    asset.reference.localOrigin = header.bounds.minimum;
    usdgeo::SpatialBounds sourceBounds = usdgeo::SpatialBounds::Empty();
    for (const auto& position : data.positions) {
        sourceBounds.Expand(position);
    }
    if (!asset.reference.TryToLocal(sourceBounds, asset.bounds)) {
        error = "LAS bounds could not be transformed to local coordinates";
        return false;
    }
    asset.data = data;
    asset.chunk = usdpointcloud::MakePointChunk(asset.data, asset.bounds);
    if (!asset.IsValid()) {
        error = "LAS point cloud asset is invalid";
        return false;
    }
    return true;
}

bool ReadPointCloud(const std::string& filename,
                    const LasReadOptions& options,
                    const std::vector<std::string>& attributes,
                    const std::string& missingCrsMessage,
                    usdpointcloud::PointCloudAsset& asset,
                    LasReadFailure& failure,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    asset = {};
    failure = LasReadFailure::None;
    usdpointcloud::PointData pointData;
    LasHeader header;
    LasReader reader(filename);
    const auto consume = [&](const LasHeader& chunkHeader,
                             const std::vector<LasPoint>& points,
                             std::string& error) {
        return AppendPointData(chunkHeader, points, filename, pointData, error);
    };
    if (!reader.Read(options, consume, header, diagnostics)) {
        failure = reader.FailureKind();
        return false;
    }

    std::string selectionError;
    if (!usdpointcloud::SelectPointDataAttributes(
            pointData, attributes, selectionError)) {
        diagnostics.clear();
        diagnostics.push_back({usdgeo::DiagnosticCode::InvalidFormatArgument,
                               usdgeo::Severity::Error, selectionError,
                               std::nullopt, std::nullopt});
        failure = LasReadFailure::InvalidRequest;
        return false;
    }
    std::string assetError;
    if (!BuildPointCloudAsset(header, pointData, missingCrsMessage, asset,
                              assetError)) {
        diagnostics.clear();
        diagnostics.push_back({usdgeo::DiagnosticCode::DecodeFailure,
                               usdgeo::Severity::Error, assetError,
                               std::nullopt, std::nullopt});
        failure = LasReadFailure::Other;
        return false;
    }
    return true;
}

bool BuildPointCloudMetadata(const LasHeader& header,
                             usdpointcloud::PointChunk& chunk,
                             usdgeo::GeoReference& reference,
                             usdgeo::SpatialBounds& bounds,
                             std::string& error) {
    if (!header.IsValid()) {
        error = "LAS metadata does not contain a valid point cloud";
        return false;
    }
    reference = {};
    reference.epsgCode = header.epsgCode;
    reference.wkt = header.crsWkt.empty()
                        ? "LAS CRS unavailable; inspect VLR metadata"
                        : header.crsWkt;
    reference.sourceUpAxis = "Z";
    reference.stageUpAxis = "Y";
    reference.localOrigin = header.bounds.minimum;
    if (!reference.TryToLocal(header.bounds, bounds)) {
        error = "LAS bounds could not be transformed to local coordinates";
        return false;
    }
    chunk = {};
    chunk.pointCount = header.pointCount;
    chunk.bounds = bounds;
    chunk.attributes = {{"xyz", usdpointcloud::PointAttributeType::Float64}};
    chunk.attributes.push_back(
        {"intensity", usdpointcloud::PointAttributeType::UInt16});
    chunk.attributes.push_back(
        {"returnNumber", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"numberOfReturns", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"classification", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"scanDirectionFlag", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"edgeOfFlightLine", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"userData", usdpointcloud::PointAttributeType::UInt8});
    chunk.attributes.push_back(
        {"scanAngle", usdpointcloud::PointAttributeType::Int16});
    chunk.attributes.push_back(
        {"pointSourceId", usdpointcloud::PointAttributeType::UInt16});
    if (header.pointFormat >= 6) {
        chunk.attributes.push_back(
            {"classificationFlags", usdpointcloud::PointAttributeType::UInt8});
        chunk.attributes.push_back(
            {"scannerChannel", usdpointcloud::PointAttributeType::UInt8});
    }
    if (header.pointFormat == 2 || header.pointFormat == 3 ||
        header.pointFormat == 7 || header.pointFormat == 8) {
        chunk.attributes.push_back(
            {"red", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back(
            {"green", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back(
            {"blue", usdpointcloud::PointAttributeType::UInt16});
    }
    if (header.pointFormat == 1 || header.pointFormat == 3 ||
        header.pointFormat >= 6) {
        chunk.attributes.push_back(
            {"gpsTime", usdpointcloud::PointAttributeType::Float64});
    }
    if (header.pointFormat == 8 || header.pointFormat == 10) {
        chunk.attributes.push_back(
            {"nir", usdpointcloud::PointAttributeType::UInt16});
    }
    if (header.pointFormat == 4 || header.pointFormat == 5 ||
        header.pointFormat == 9 || header.pointFormat == 10) {
        chunk.attributes.push_back(
            {"waveformDescriptorIndex", usdpointcloud::PointAttributeType::UInt8});
        chunk.attributes.push_back(
            {"waveformDataOffset", usdpointcloud::PointAttributeType::UInt64});
        chunk.attributes.push_back(
            {"waveformPacketSize", usdpointcloud::PointAttributeType::UInt32});
        chunk.attributes.push_back(
            {"returnPointWaveformLocation", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back(
            {"waveformXt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back(
            {"waveformYt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back(
            {"waveformZt", usdpointcloud::PointAttributeType::Float32});
        chunk.attributes.push_back(
            {"waveformDataExternal", usdpointcloud::PointAttributeType::UInt8});
    }
    std::vector<std::string> extraByteNames;
    extraByteNames.reserve(header.extraBytes.size());
    for (const auto& extra : header.extraBytes) {
        extraByteNames.push_back(extra.name);
    }
    const auto normalizedExtraByteNames =
        usdpointcloud::NormalizeExtraByteNames(extraByteNames);
    for (std::size_t index = 0; index < extraByteNames.size(); ++index) {
        const auto componentCount =
            detail::ExtraByteComponentCount(header.extraBytes[index].dataType);
        if (componentCount == 0) {
            error = "unsupported LAS Extra Bytes data type";
            return false;
        }
        const auto type = componentCount == 1
                              ? usdpointcloud::PointAttributeType::Float64
                          : componentCount == 2
                              ? usdpointcloud::PointAttributeType::Float64Vec2
                              : usdpointcloud::PointAttributeType::Float64Vec3;
        chunk.attributes.push_back({normalizedExtraByteNames[index], type});
    }
    return chunk.IsValid();
}

} // namespace usdlas