#include "geolaz/GeoLazFileFormat.h"
#include "geolaz/GeoLazDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdlaz/Laz.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
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

TF_DEFINE_PUBLIC_TOKENS(GeoLazFileFormatTokens, GEOLAZ_FILE_FORMAT_TOKENS);

GeoLazFileFormat::GeoLazFileFormat()
    : SdfFileFormat(GeoLazFileFormatTokens->Id,
                    GeoLazFileFormatTokens->Version,
                    GeoLazFileFormatTokens->Target,
                    GeoLazFileFormatTokens->Extension) {}

GeoLazFileFormat::~GeoLazFileFormat() = default;

bool GeoLazFileFormat::CanRead(const std::string& file) const {
    return SdfFileFormat::GetFileExtension(file) == "laz";
}

bool GeoLazFileFormat::Read(SdfLayer* layer,
                            const std::string& resolvedPath,
                            bool metadataOnly) const {
    if (!layer || metadataOnly) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::InvalidReadRequest,
                                  "geoLaz requires a writable layer and full point data")
                                  .c_str());
        return false;
    }

    std::vector<usdgeo::Diagnostic> diagnostics;
    auto decoder = usdlaz::CreateFileDecoder(resolvedPath, diagnostics);
    if (!decoder) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::FileOpenFailed,
                                  "Unable to open LAZ file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "decoder could not be created"))
                                  .c_str());
        return false;
    }

    usdlaz::LazReader reader(std::move(decoder));
    usdlas::LasHeader header;
    usdgeo::PointCloudLayer::Data pointData;
    const auto consumed = reader.Read(
        {},
        [&](const usdlas::LasHeader& chunkHeader,
            const std::vector<usdlas::LasPoint>& points) {
            if (pointData.positions.empty()) {
                const auto pointCount = static_cast<std::size_t>(
                    chunkHeader.pointCount);
                pointData.positions.reserve(pointCount);
                pointData.intensity.reserve(pointCount);
                pointData.returnNumber.reserve(pointCount);
                pointData.numberOfReturns.reserve(pointCount);
                pointData.classification.reserve(pointCount);
                if (chunkHeader.pointFormat == 2 ||
                    chunkHeader.pointFormat == 3 ||
                    chunkHeader.pointFormat == 7 ||
                    chunkHeader.pointFormat == 8) {
                    pointData.red.reserve(pointCount);
                    pointData.green.reserve(pointCount);
                    pointData.blue.reserve(pointCount);
                }
                if (chunkHeader.pointFormat == 1 ||
                    chunkHeader.pointFormat == 3 ||
                    chunkHeader.pointFormat >= 6) {
                    pointData.gpsTime.reserve(pointCount);
                }
            }
            for (const auto& point : points) {
                pointData.positions.push_back(point.sourcePosition);
                pointData.intensity.push_back(point.intensity);
                pointData.returnNumber.push_back(point.returnNumber);
                pointData.numberOfReturns.push_back(point.numberOfReturns);
                pointData.classification.push_back(point.classification);
                if (point.hasColor) {
                    pointData.red.push_back(point.red);
                    pointData.green.push_back(point.green);
                    pointData.blue.push_back(point.blue);
                }
                if (point.hasGpsTime) {
                    pointData.gpsTime.push_back(point.gpsTime);
                }
            }
            return true;
        },
        header, diagnostics);
    if (!consumed) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::DecodeFailed,
                                  "Unable to decode LAZ file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "decode failed"))
                                  .c_str());
        return false;
    }

    usdgeo::GeoReference reference;
    reference.wkt = header.crsWkt.empty()
                        ? "LAZ CRS unavailable; inspect VLR metadata"
                        : header.crsWkt;
    reference.sourceUpAxis = "Z";
    reference.stageUpAxis = "Y";
    reference.localOrigin = header.bounds.minimum;
    usdgeo::SpatialBounds bounds;
    if (!reference.TryToLocal(header.bounds, bounds)) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::BoundsTransformFailed,
                                  "Unable to transform LAZ bounds to USD: " +
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
        {"classification", usdpointcloud::PointAttributeType::UInt8}};
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

    const auto usda = SdfFileFormat::FindByExtension("usda");
    const auto generated = SdfLayer::CreateAnonymous(
        "geo-laz.generated.usda", usda);
    const auto stage = UsdStage::Open(generated);
    if (!stage) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::UsdLayerCreateFailed,
                                  "Unable to create a USD layer for LAZ: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    if (!UsdGeomSetStageUpAxis(stage, TfToken("Y")) ||
        !UsdGeomSetStageMetersPerUnit(stage, 1.0)) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::StageMetricsFailed,
                                  "Unable to set USD stage metrics for LAZ: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    if (!usdgeo::PointCloudLayer::AuthorPointCloud(
            stage, "/PointCloud", reference, bounds, chunk, pointData)) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::PointCloudAuthorFailed,
                                  "Unable to author LAZ point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    layer->TransferContent(generated);
    return true;
}

bool GeoLazFileFormat::WriteToString(const SdfLayer& layer,
                                     std::string* str,
                                     const std::string& comment) const {
    const auto usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

TF_REGISTRY_FUNCTION(TfType) {
    SDF_DEFINE_FILE_FORMAT(GeoLazFileFormat, SdfFileFormat);
}

PXR_NAMESPACE_CLOSE_SCOPE
