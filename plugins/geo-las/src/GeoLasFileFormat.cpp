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
#include <algorithm>
#include <fstream>
#include <limits>
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

    std::ifstream input(resolvedPath, std::ios::binary | std::ios::ate);
    if (!input) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                      geolas::diagnostics::FileOpenFailed,
                      "Unable to open LAS file: " + resolvedPath)
                      .c_str());
        return false;
    }
    const auto fileSize = input.tellg();
    if (fileSize < 0 || static_cast<std::uintmax_t>(fileSize) >
                            std::numeric_limits<std::size_t>::max()) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                      geolas::diagnostics::FileSizeUnavailable,
                      "Unable to determine LAS file size: " + resolvedPath)
                      .c_str());
        return false;
    }
    input.seekg(0, std::ios::beg);
    const auto headerReadSize = std::min<std::size_t>(
        static_cast<std::size_t>(fileSize), 375);
    std::vector<std::uint8_t> headerBytes(headerReadSize);
    if (!input.read(reinterpret_cast<char*>(headerBytes.data()),
                    static_cast<std::streamsize>(headerBytes.size()))) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                      geolas::diagnostics::HeaderReadFailed,
                      "Unable to read LAS header: " + resolvedPath)
                      .c_str());
        return false;
    }

    usdlas::LasHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!usdlas::InspectHeader(headerBytes, header, diagnostics)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::HeaderInvalid,
                                  "Unable to inspect LAS file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "header is invalid"))
                                  .c_str());
        return false;
    }

    header.variableLengthRecords.clear();
    if (header.variableLengthRecordCount != 0) {
        const auto metadataSize = static_cast<std::size_t>(header.pointDataOffset);
        std::vector<std::uint8_t> metadataBytes(metadataSize);
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(metadataBytes.data()),
                        static_cast<std::streamsize>(metadataBytes.size())) ||
            !usdlas::InspectRecords(metadataBytes, header.headerSize,
                                    header.variableLengthRecordCount, false,
                                    header.variableLengthRecords, diagnostics)) {
            TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                      geolas::diagnostics::VlrInvalid,
                                      "Unable to inspect LAS VLR metadata " +
                                          resolvedPath + ": " +
                                          DiagnosticDetail(diagnostics, "metadata is invalid"))
                                      .c_str());
            return false;
        }
    }
    if (header.extendedVariableLengthRecordCount != 0) {
        const auto evlrOffset = static_cast<std::size_t>(
            header.firstExtendedVariableLengthRecordOffset);
        if (evlrOffset > static_cast<std::size_t>(fileSize)) {
            TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                      geolas::diagnostics::EvlrOffsetInvalid,
                                      "LAS EVLR offset is outside the file: " +
                                          resolvedPath)
                                      .c_str());
            return false;
        }
        std::vector<std::uint8_t> evlrBytes(
            static_cast<std::size_t>(fileSize) - evlrOffset);
        input.seekg(static_cast<std::streamoff>(evlrOffset), std::ios::beg);
        if (!input.read(reinterpret_cast<char*>(evlrBytes.data()),
                        static_cast<std::streamsize>(evlrBytes.size())) ||
            !usdlas::InspectRecords(evlrBytes, 0,
                                    header.extendedVariableLengthRecordCount,
                                    true, header.variableLengthRecords,
                                    diagnostics)) {
            TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                      geolas::diagnostics::EvlrInvalid,
                                      "Unable to inspect LAS EVLR metadata " +
                                          resolvedPath + ": " +
                                          DiagnosticDetail(diagnostics, "metadata is invalid"))
                                      .c_str());
            return false;
        }
    }
    if (!usdlas::ParseKnownMetadata(header.variableLengthRecords, header,
                                    diagnostics)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::VlrInvalid,
                                  "Unable to parse known LAS metadata " +
                                      resolvedPath + ": " +
                                      DiagnosticDetail(diagnostics, "metadata is invalid"))
                                  .c_str());
        return false;
    }

    const auto recordLength = static_cast<std::size_t>(header.pointRecordLength);
    const auto pointOffset = static_cast<std::size_t>(header.pointDataOffset);
    if (header.pointCount >
            (std::numeric_limits<std::size_t>::max() - pointOffset) /
                recordLength ||
        pointOffset + static_cast<std::size_t>(header.pointCount) *
                          recordLength >
            static_cast<std::size_t>(fileSize)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                      geolas::diagnostics::PointDataTruncated,
                      "LAS point data is truncated: " + resolvedPath)
                      .c_str());
        return false;
    }

    input.seekg(static_cast<std::streamoff>(pointOffset), std::ios::beg);
    if (!input) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                      geolas::diagnostics::PointDataSeekFailed,
                      "Unable to seek to LAS point data: " + resolvedPath)
                      .c_str());
        return false;
    }
    std::vector<std::uint8_t> record(recordLength);
    usdgeo::PointCloudLayer::Data pointData;
    pointData.positions.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.intensity.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.returnNumber.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.numberOfReturns.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.classification.reserve(static_cast<std::size_t>(header.pointCount));
    const bool modern = header.pointFormat >= 6;
    if (modern) {
        pointData.classificationFlags.reserve(
            static_cast<std::size_t>(header.pointCount));
        pointData.scannerChannel.reserve(
            static_cast<std::size_t>(header.pointCount));
    }
    pointData.scanDirectionFlag.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.edgeOfFlightLine.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.userData.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.scanAngle.reserve(static_cast<std::size_t>(header.pointCount));
    pointData.pointSourceId.reserve(static_cast<std::size_t>(header.pointCount));
    if (header.pointFormat == 2 || header.pointFormat == 3 ||
        header.pointFormat == 7 || header.pointFormat == 8) {
        pointData.red.reserve(static_cast<std::size_t>(header.pointCount));
        pointData.green.reserve(static_cast<std::size_t>(header.pointCount));
        pointData.blue.reserve(static_cast<std::size_t>(header.pointCount));
    }
    if (header.pointFormat == 1 || header.pointFormat == 3 ||
        header.pointFormat >= 6) {
        pointData.gpsTime.reserve(static_cast<std::size_t>(header.pointCount));
    }
    if (header.pointFormat == 8) {
        pointData.nir.reserve(static_cast<std::size_t>(header.pointCount));
    }
    for (std::uint64_t index = 0; index < header.pointCount; ++index) {
        if (!input.read(reinterpret_cast<char*>(record.data()),
                        static_cast<std::streamsize>(record.size()))) {
            TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                      geolas::diagnostics::PointReadFailed,
                                      "Unable to read LAS point " +
                                          std::to_string(index) + ": " + resolvedPath)
                                      .c_str());
            return false;
        }
        usdlas::LasPoint point;
        if (!usdlas::DecodePoint(header, record, point, diagnostics)) {
            TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                      geolas::diagnostics::PointDecodeFailed,
                                      "Unable to decode LAS point " +
                                          std::to_string(index) + ": " +
                                          DiagnosticDetail(diagnostics, "decode failed"))
                                      .c_str());
            return false;
        }
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
        if (header.pointFormat == 8) {
            pointData.nir.push_back(point.nir);
        }
    }

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
