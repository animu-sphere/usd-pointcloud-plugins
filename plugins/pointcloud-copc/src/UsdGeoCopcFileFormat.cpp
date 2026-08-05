#include "usdgeocopc/UsdGeoCopcFileFormat.h"
#include "usdgeocopc/UsdGeoCopcDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdpointcloud/FileFormatArguments.h"
#include "usdpointcloud/Sampling.h"
#include "usdcopc/Copc.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>

#include <algorithm>
#include <cstdint>
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

bool MakeReadRequest(const SdfLayer* layer,
                     usdpointcloud::PointReadRequest& request,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    return usdpointcloud::NormalizeFileFormatArguments(
        layer->GetFileFormatArguments(), request, diagnostics);
}

const char* ReaderDiagnosticCode(
    const std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!diagnostics.empty() &&
        diagnostics.front().code == usdgeo::DiagnosticCode::InvalidOffset) {
        return usdgeocopc::diagnostics::FileOpenFailed;
    }
    return usdgeocopc::diagnostics::DecodeFailed;
}

bool IsSelectedPoint(const usdlas::LasPoint& point,
                     std::uint64_t pointIndex,
                     const usdpointcloud::PointReadOptions& options) {
    if (pointIndex < options.range.firstPoint) {
        return false;
    }
    if (options.range.pointCount != 0 &&
        pointIndex - options.range.firstPoint >= options.range.pointCount) {
        return false;
    }
    return usdlas::MatchesReadOptions(point, options);
}

} // namespace

TF_DEFINE_PUBLIC_TOKENS(UsdGeoCopcFileFormatTokens,
                        USDGEOCOPC_FILE_FORMAT_TOKENS);

UsdGeoCopcFileFormat::UsdGeoCopcFileFormat()
    : SdfFileFormat(UsdGeoCopcFileFormatTokens->Id,
                    UsdGeoCopcFileFormatTokens->Version,
                    UsdGeoCopcFileFormatTokens->Target,
                    UsdGeoCopcFileFormatTokens->Extension) {}

UsdGeoCopcFileFormat::~UsdGeoCopcFileFormat() = default;

bool UsdGeoCopcFileFormat::CanRead(const std::string& file) const {
    return SdfFileFormat::GetFileExtension(file) == "copc";
}

bool UsdGeoCopcFileFormat::Read(SdfLayer* layer,
                                const std::string& resolvedPath,
                                bool metadataOnly) const {
    if (!layer) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::InvalidReadRequest,
                                  "pointcloud-copc requires a writable layer")
                                  .c_str());
        return false;
    }

    usdpointcloud::PointReadRequest request;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!MakeReadRequest(layer, request, diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::FormatArgumentInvalid,
                                  "Invalid COPC file-format arguments: " +
                                      DiagnosticDetail(diagnostics,
                                                       "invalid arguments"))
                                  .c_str());
        return false;
    }
    if (request.tiled) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::FormatArgumentInvalid,
                                  "COPC tiled reads are not implemented yet")
                                  .c_str());
        return false;
    }

    usdcopc::CopcReader reader(resolvedPath);
    usdcopc::CopcHeader header;
    if (!reader.ReadMetadata(header, diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  ReaderDiagnosticCode(diagnostics),
                                  "Unable to inspect COPC file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics,
                                                       "inspection failed"))
                                  .c_str());
        return false;
    }

    if (metadataOnly) {
        usdpointcloud::PointChunk chunk;
        usdgeo::GeoReference reference;
        usdgeo::SpatialBounds bounds;
        usdgeo::PointCloudSourceMetadata sourceMetadata{
            header.las.pointFormat,
            {header.las.xScale, header.las.yScale, header.las.zScale},
            {header.las.xOffset, header.las.yOffset, header.las.zOffset}};
        std::string metadataError;
        if (!usdlas::BuildPointCloudMetadata(
                header.las, chunk, reference, bounds, metadataError) ||
            !usdgeo::AuthorPointCloudMetadata(
                layer, "/PointCloud", reference, bounds, chunk,
                sourceMetadata)) {
            TF_RUNTIME_ERROR("%s",
                             usdgeocopc::diagnostics::PointCloudAuthorFailed);
            return false;
        }
        return true;
    }

    std::vector<usdcopc::CopcHierarchyEntry> hierarchy;
    if (!reader.ReadHierarchy(header, hierarchy, diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::DecodeFailed,
                                  "Unable to inspect COPC hierarchy: " +
                                      DiagnosticDetail(diagnostics,
                                                       "hierarchy read failed"))
                                  .c_str());
        return false;
    }

    usdpointcloud::PointData pointData;
    std::uint64_t pointIndex = 0;
    for (const auto& entry : hierarchy) {
        if (!entry.IsPointData()) {
            continue;
        }
        std::vector<usdlas::LasPoint> points;
        if (!reader.ReadPoints(header, entry, points, diagnostics)) {
            TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                      usdgeocopc::diagnostics::DecodeFailed,
                                      "Unable to decode COPC point data: " +
                                          DiagnosticDetail(
                                              diagnostics, "decode failed"))
                                      .c_str());
            return false;
        }
        std::vector<usdlas::LasPoint> selected;
        selected.reserve(points.size());
        for (const auto& point : points) {
            if (IsSelectedPoint(point, pointIndex, request.readOptions)) {
                selected.push_back(point);
            }
            ++pointIndex;
        }
        std::string appendError;
        if (!usdlas::AppendPointData(header.las, selected, resolvedPath,
                                     pointData, appendError)) {
            TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                      usdgeocopc::diagnostics::DecodeFailed,
                                      "Unable to construct COPC point data: " +
                                          appendError)
                                      .c_str());
            return false;
        }
    }

    std::string selectionError;
    if (!usdpointcloud::SelectPointDataAttributes(
            pointData, request.attributes, selectionError)) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::FormatArgumentInvalid,
                                  "Unable to select COPC point attributes: " +
                                      selectionError)
                                  .c_str());
        return false;
    }

    usdpointcloud::PointCloudAsset asset;
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(
            header.las, pointData, "COPC CRS unavailable; inspect VLR metadata",
            asset, assetError)) {
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  usdgeocopc::diagnostics::BoundsTransformFailed,
                                  "Unable to build COPC point cloud asset: " +
                                      assetError)
                                  .c_str());
        return false;
    }

    bool authored = false;
    usdgeo::PointCloudAuthorFailure authorFailure =
        usdgeo::PointCloudAuthorFailure::PointCloud;
    if (request.lodProfile != usdpointcloud::LodProfile::Off) {
        std::vector<usdpointcloud::PointCloudAsset> levels;
        usdpointcloud::PointLodHierarchy lodHierarchy;
        if (usdpointcloud::BuildPointLodAssets(
                asset, request.lodProfile, levels, lodHierarchy, diagnostics)) {
            authored = usdgeo::AuthorPointCloudLodAsset(
                layer, "/PointCloud", levels, lodHierarchy);
        }
    } else {
        authored = usdgeo::AuthorPointCloudAsset(layer, "/PointCloud", asset,
                                                 authorFailure);
    }
    if (!authored) {
        const char* code = usdgeocopc::diagnostics::PointCloudAuthorFailed;
        if (authorFailure == usdgeo::PointCloudAuthorFailure::InvalidLayer ||
            authorFailure == usdgeo::PointCloudAuthorFailure::StageCreation) {
            code = usdgeocopc::diagnostics::UsdLayerCreateFailed;
        } else if (authorFailure == usdgeo::PointCloudAuthorFailure::StageMetrics) {
            code = usdgeocopc::diagnostics::StageMetricsFailed;
        }
        TF_RUNTIME_ERROR("%s", usdgeocopc::diagnostics::Message(
                                  code,
                                  "Unable to author COPC point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    return true;
}

bool UsdGeoCopcFileFormat::WriteToString(const SdfLayer& layer,
                                         std::string* str,
                                         const std::string& comment) const {
    const auto usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

TF_REGISTRY_FUNCTION(TfType) {
    SDF_DEFINE_FILE_FORMAT(UsdGeoCopcFileFormat, SdfFileFormat);
}

PXR_NAMESPACE_CLOSE_SCOPE
