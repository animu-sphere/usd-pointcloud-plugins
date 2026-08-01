#include "geolas/GeoLasFileFormat.h"
#include "geolas/GeoLasDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdlas/Las.h"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>

#include <cstdint>
#include <filesystem>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

std::string DiagnosticDetail(
    const std::vector<usdgeo::Diagnostic>& diagnostics,
    const std::string& fallback) {
    if (diagnostics.empty()) {
        return fallback;
    }
    const auto& diagnostic = diagnostics.front();
    std::string detail = diagnostic.message;
    if (diagnostic.byteOffset) {
        detail += " (byte offset " +
                  std::to_string(*diagnostic.byteOffset) + ")";
    }
    if (diagnostic.pointIndex) {
        detail += " (point " + std::to_string(*diagnostic.pointIndex) + ")";
    }
    return detail;
}

const char* ReaderDiagnosticCode(
    const std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return geolas::diagnostics::PointDecodeFailed;
    }
    const auto& diagnostic = diagnostics.front();
    if (diagnostic.message.rfind("could not open LAS file:", 0) == 0) {
        return geolas::diagnostics::FileOpenFailed;
    }
    if (diagnostic.message == "could not determine LAS file size") {
        return geolas::diagnostics::FileSizeUnavailable;
    }
    if (diagnostic.message == "LAS point data is truncated") {
        return geolas::diagnostics::PointDataTruncated;
    }
    if (diagnostic.message ==
        "LAS extended variable-length record offset is invalid") {
        return geolas::diagnostics::EvlrOffsetInvalid;
    }
    if (diagnostic.message.find("LAS variable-length record") !=
        std::string::npos) {
        return geolas::diagnostics::VlrInvalid;
    }
    if (diagnostic.message.find("LAS GeoTIFF") != std::string::npos ||
        diagnostic.message.find("LAS Extra Bytes") != std::string::npos) {
        return geolas::diagnostics::VlrInvalid;
    }
    if (diagnostic.pointIndex) {
        return geolas::diagnostics::PointDecodeFailed;
    }
    return geolas::diagnostics::HeaderInvalid;
}

} // namespace

TF_DEFINE_PUBLIC_TOKENS(GeoLasFileFormatTokens, GEOLAS_FILE_FORMAT_TOKENS);

GeoLasFileFormat::GeoLasFileFormat()
    : SdfFileFormat(GeoLasFileFormatTokens->Id,
                    GeoLasFileFormatTokens->Version,
                    GeoLasFileFormatTokens->Target,
                    GeoLasFileFormatTokens->Extension) {}

GeoLasFileFormat::~GeoLasFileFormat() = default;

bool GeoLasFileFormat::CanRead(const std::string& file) const {
    return SdfFileFormat::GetFileExtension(file) == "las";
}

bool GeoLasFileFormat::Read(SdfLayer* layer,
                            const std::string& resolvedPath,
                            bool metadataOnly) const {
    if (!layer || metadataOnly) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::InvalidReadRequest,
                                  "geoLas requires a writable layer and full point data")
                                  .c_str());
        return false;
    }

    usdlas::LasHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdgeo::PointCloudLayer::Data pointData;
    bool reserved = false;
    usdlas::LasReader reader(resolvedPath);
    usdlas::LasReadOptions options;
    const auto consume = [&](const usdlas::LasHeader& chunkHeader,
                             const std::vector<usdlas::LasPoint>& points) {
        const bool modern = chunkHeader.pointFormat >= 6;
        if (!reserved) {
            const auto pointCount = static_cast<std::size_t>(
                chunkHeader.pointCount);
            pointData.positions.reserve(pointCount);
            pointData.intensity.reserve(pointCount);
            pointData.returnNumber.reserve(pointCount);
            pointData.numberOfReturns.reserve(pointCount);
            pointData.classification.reserve(pointCount);
            if (modern) {
                pointData.classificationFlags.reserve(pointCount);
                pointData.scannerChannel.reserve(pointCount);
            }
            pointData.scanDirectionFlag.reserve(pointCount);
            pointData.edgeOfFlightLine.reserve(pointCount);
            pointData.userData.reserve(pointCount);
            pointData.scanAngle.reserve(pointCount);
            pointData.pointSourceId.reserve(pointCount);
            if (chunkHeader.pointFormat == 2 || chunkHeader.pointFormat == 3 ||
                chunkHeader.pointFormat == 7 || chunkHeader.pointFormat == 8) {
                pointData.red.reserve(pointCount);
                pointData.green.reserve(pointCount);
                pointData.blue.reserve(pointCount);
            }
            if (chunkHeader.pointFormat == 1 || chunkHeader.pointFormat == 3 ||
                chunkHeader.pointFormat >= 6) {
                pointData.gpsTime.reserve(pointCount);
            }
            if (chunkHeader.pointFormat == 8 || chunkHeader.pointFormat == 10) {
                pointData.nir.reserve(pointCount);
            }
            if (chunkHeader.pointFormat == 4 || chunkHeader.pointFormat == 5 ||
                chunkHeader.pointFormat == 9 || chunkHeader.pointFormat == 10) {
                pointData.waveformDescriptorIndex.reserve(pointCount);
                pointData.waveformDataOffset.reserve(pointCount);
                pointData.waveformPacketSize.reserve(pointCount);
                pointData.returnPointWaveformLocation.reserve(pointCount);
                pointData.waveformXt.reserve(pointCount);
                pointData.waveformYt.reserve(pointCount);
                pointData.waveformZt.reserve(pointCount);
                pointData.waveformDataExternal.reserve(pointCount);
            }
            reserved = true;
        }
        for (const auto& point : points) {
        pointData.positions.push_back(point.sourcePosition);
        pointData.intensity.push_back(point.intensity);
        pointData.returnNumber.push_back(point.returnNumber);
        pointData.numberOfReturns.push_back(point.numberOfReturns);
        pointData.classification.push_back(point.classification);
        if (modern) {
            pointData.classificationFlags.push_back(point.classificationFlags);
            pointData.scannerChannel.push_back(point.scannerChannel);
        }
        pointData.scanDirectionFlag.push_back(point.scanDirectionFlag);
        pointData.edgeOfFlightLine.push_back(point.edgeOfFlightLine);
        pointData.userData.push_back(point.userData);
        pointData.scanAngle.push_back(point.scanAngle);
        pointData.pointSourceId.push_back(point.pointSourceId);
        if (point.hasColor) {
            pointData.red.push_back(point.red);
            pointData.green.push_back(point.green);
            pointData.blue.push_back(point.blue);
        }
        if (point.hasGpsTime) {
            pointData.gpsTime.push_back(point.gpsTime);
        }
        if (chunkHeader.pointFormat == 8 || chunkHeader.pointFormat == 10) {
            pointData.nir.push_back(point.nir);
        }
        if (point.hasWaveform) {
            pointData.waveformDescriptorIndex.push_back(
                point.waveform.descriptorIndex);
            pointData.waveformDataOffset.push_back(point.waveform.dataOffset);
            pointData.waveformPacketSize.push_back(point.waveform.packetSize);
            pointData.returnPointWaveformLocation.push_back(
                point.waveform.returnPointLocation);
            pointData.waveformXt.push_back(point.waveform.xt);
            pointData.waveformYt.push_back(point.waveform.yt);
            pointData.waveformZt.push_back(point.waveform.zt);
            pointData.waveformDataExternal.push_back(
                point.waveform.external ? 1 : 0);
            if (point.waveform.external && pointData.waveformDataFile.empty()) {
                auto waveformPath = std::filesystem::path(resolvedPath);
                waveformPath.replace_extension(".wdp");
                pointData.waveformDataFile = waveformPath.string();
            }
        }
        }
        return true;
    };
    if (!reader.Read(options, consume, header, diagnostics)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  ReaderDiagnosticCode(diagnostics),
                                  "Unable to read LAS file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "read failed"))
                                  .c_str());
        return false;
    }

    const bool modern = header.pointFormat >= 6;
    usdgeo::GeoReference reference;
    reference.wkt = header.crsWkt.empty()
                        ? "LAS CRS unavailable; inspect VLR metadata"
                        : header.crsWkt;
    reference.sourceUpAxis = "Z";
    reference.stageUpAxis = "Y";
    reference.localOrigin = header.bounds.minimum;
    usdgeo::SpatialBounds bounds;
    if (!reference.TryToLocal(header.bounds, bounds)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::BoundsTransformFailed,
                                  "Unable to transform LAS bounds to USD: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    usdpointcloud::PointChunk chunk;
    chunk.pointCount = header.pointCount;
    chunk.bounds = bounds;
    chunk.attributes = {
        {"intensity", usdpointcloud::PointAttributeType::UInt16},
        {"returnNumber", usdpointcloud::PointAttributeType::UInt8},
        {"numberOfReturns", usdpointcloud::PointAttributeType::UInt8},
        {"classification", usdpointcloud::PointAttributeType::UInt8},
        {"scanDirectionFlag", usdpointcloud::PointAttributeType::UInt8},
        {"edgeOfFlightLine", usdpointcloud::PointAttributeType::UInt8},
        {"userData", usdpointcloud::PointAttributeType::UInt8},
        {"scanAngle", usdpointcloud::PointAttributeType::Int16},
        {"pointSourceId", usdpointcloud::PointAttributeType::UInt16}};
    if (modern) {
        chunk.attributes.push_back(
            {"classificationFlags", usdpointcloud::PointAttributeType::UInt8});
        chunk.attributes.push_back(
            {"scannerChannel", usdpointcloud::PointAttributeType::UInt8});
    }
    if (!pointData.red.empty()) {
        chunk.attributes.push_back(
            {"red", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back(
            {"green", usdpointcloud::PointAttributeType::UInt16});
        chunk.attributes.push_back(
            {"blue", usdpointcloud::PointAttributeType::UInt16});
    }
    if (!pointData.gpsTime.empty()) {
        chunk.attributes.push_back(
            {"gpsTime", usdpointcloud::PointAttributeType::Float64});
    }
    if (!pointData.nir.empty()) {
        chunk.attributes.push_back(
            {"nir", usdpointcloud::PointAttributeType::UInt16});
    }
    if (!pointData.waveformDescriptorIndex.empty()) {
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

    const auto usda = SdfFileFormat::FindByExtension("usda");
    const auto generated = SdfLayer::CreateAnonymous(
        "geo-las.generated.usda", usda);
    const auto stage = UsdStage::Open(generated);
    if (!stage) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::UsdLayerCreateFailed,
                                  "Unable to create a USD layer for LAS: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    if (!UsdGeomSetStageUpAxis(stage, TfToken("Y")) ||
        !UsdGeomSetStageMetersPerUnit(stage, 1.0)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::StageMetricsFailed,
                                  "Unable to set USD stage metrics for LAS: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
        if (!usdgeo::PointCloudLayer::AuthorPointCloud(
            stage, "/PointCloud", reference, bounds, chunk, pointData)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::PointCloudAuthorFailed,
                                  "Unable to author LAS point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    layer->TransferContent(generated);
    return true;
}

bool GeoLasFileFormat::WriteToString(const SdfLayer& layer,
                                     std::string* str,
                                     const std::string& comment) const {
    const auto usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

TF_REGISTRY_FUNCTION(TfType) {
    SDF_DEFINE_FILE_FORMAT(GeoLasFileFormat, SdfFileFormat);
}

PXR_NAMESPACE_CLOSE_SCOPE
