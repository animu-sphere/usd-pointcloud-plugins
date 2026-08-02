#include "usdgeolas/UsdGeoLasFileFormat.h"
#include "usdgeolas/UsdGeoLasDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdpointcloud/FileFormatArguments.h"
#include "usdpointcloud/Sampling.h"
#include "usdlas/Las.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>

#include <cstdint>
#include <filesystem>
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

const char* ReaderDiagnosticCode(
    const std::vector<usdgeo::Diagnostic>& diagnostics,
    usdlas::LasReadFailure failure) {
    if (diagnostics.empty()) {
        return usdgeolas::diagnostics::PointDecodeFailed;
    }
    switch (failure) {
    case usdlas::LasReadFailure::FileOpen:
        return usdgeolas::diagnostics::FileOpenFailed;
    case usdlas::LasReadFailure::FileSize:
        return usdgeolas::diagnostics::FileSizeUnavailable;
    case usdlas::LasReadFailure::PointDataTruncated:
        return usdgeolas::diagnostics::PointDataTruncated;
    case usdlas::LasReadFailure::EvlrOffset:
        return usdgeolas::diagnostics::EvlrOffsetInvalid;
    case usdlas::LasReadFailure::Vlr:
        return usdgeolas::diagnostics::VlrInvalid;
    case usdlas::LasReadFailure::Evlr:
        return usdgeolas::diagnostics::EvlrInvalid;
    case usdlas::LasReadFailure::PointDataSeek:
        return usdgeolas::diagnostics::PointDataSeekFailed;
    case usdlas::LasReadFailure::PointDataRead:
        return usdgeolas::diagnostics::PointReadFailed;
    case usdlas::LasReadFailure::PointDecode:
        return usdgeolas::diagnostics::PointDecodeFailed;
    default:
        break;
    }

    switch (diagnostics.front().code) {
    case usdgeo::DiagnosticCode::NonFiniteCoordinate:
    case usdgeo::DiagnosticCode::DecodeFailure:
        return usdgeolas::diagnostics::PointDecodeFailed;
    case usdgeo::DiagnosticCode::InvalidCrs:
    case usdgeo::DiagnosticCode::UnsupportedExtraBytesType:
        return usdgeolas::diagnostics::VlrInvalid;
    default:
        return usdgeolas::diagnostics::HeaderInvalid;
    }
}

bool MakeReadRequest(const SdfLayer* layer,
                     const std::string& resolvedPath,
                     std::string& sourcePath,
                     usdpointcloud::PointReadRequest& request,
                     std::vector<usdgeo::Diagnostic>& diagnostics) {
    sourcePath = resolvedPath;
                    return usdpointcloud::NormalizeFileFormatArguments(
                        layer->GetFileFormatArguments(), request, diagnostics);
}

} // namespace

TF_DEFINE_PUBLIC_TOKENS(UsdGeoLasFileFormatTokens, USDGEOLAS_FILE_FORMAT_TOKENS);

UsdGeoLasFileFormat::UsdGeoLasFileFormat()
    : SdfFileFormat(UsdGeoLasFileFormatTokens->Id,
                    UsdGeoLasFileFormatTokens->Version,
                    UsdGeoLasFileFormatTokens->Target,
                    UsdGeoLasFileFormatTokens->Extension) {}

UsdGeoLasFileFormat::~UsdGeoLasFileFormat() = default;

bool UsdGeoLasFileFormat::CanRead(const std::string& file) const {
    return SdfFileFormat::GetFileExtension(file) == "las";
}

bool UsdGeoLasFileFormat::Read(SdfLayer* layer,
                            const std::string& resolvedPath,
                            bool metadataOnly) const {
    if (!layer) {
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  usdgeolas::diagnostics::InvalidReadRequest,
                                  "geospatial-las requires a writable layer")
                                  .c_str());
        return false;
    }

    std::string sourcePath;
    usdpointcloud::PointReadRequest request;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!MakeReadRequest(layer, resolvedPath, sourcePath, request,
                         diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  usdgeolas::diagnostics::FormatArgumentInvalid,
                                  "Invalid LAS file-format arguments: " +
                                      DiagnosticDetail(diagnostics,
                                                       "invalid arguments"))
                                  .c_str());
        return false;
    }

    if (metadataOnly) {
        usdlas::LasHeader header;
        usdlas::LasReader reader(sourcePath);
        if (!reader.ReadMetadata(header, diagnostics)) {
            TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                      ReaderDiagnosticCode(diagnostics,
                                                           reader.FailureKind()),
                                      "Unable to inspect LAS file " + resolvedPath +
                                          ": " + DiagnosticDetail(
                                              diagnostics, "inspection failed"))
                                      .c_str());
            return false;
        }
        usdpointcloud::PointChunk chunk;
        usdgeo::GeoReference reference;
        usdgeo::SpatialBounds bounds;
        usdgeo::PointCloudSourceMetadata sourceMetadata{
            header.pointFormat,
            {header.xScale, header.yScale, header.zScale},
            {header.xOffset, header.yOffset, header.zOffset}};
        std::string metadataError;
        if (!usdlas::BuildPointCloudMetadata(
                header, chunk, reference, bounds, metadataError) ||
            !usdgeo::AuthorPointCloudMetadata(
                layer, "/PointCloud", reference, bounds, chunk,
                sourceMetadata)) {
            TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::PointCloudAuthorFailed);
            return false;
        }
        return true;
    }

    usdlas::LasHeader header;
    usdpointcloud::PointData pointData;
    usdlas::LasReader reader(sourcePath);
    const auto consume = [&](const usdlas::LasHeader& chunkHeader,
                             const std::vector<usdlas::LasPoint>& points,
                             std::string& error) {
        return usdlas::AppendPointData(chunkHeader, points, sourcePath,
                                        pointData, error);
    };
    if (!reader.Read(request.readOptions, consume, header, diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  ReaderDiagnosticCode(diagnostics,
                                                       reader.FailureKind()),
                                  "Unable to read LAS file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "read failed"))
                                  .c_str());
        return false;
    }

    std::string selectionError;
    if (!usdpointcloud::SelectPointDataAttributes(
            pointData, request.attributes, selectionError)) {
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  usdgeolas::diagnostics::FormatArgumentInvalid,
                                  "Unable to select LAS point attributes: " +
                                      selectionError)
                                  .c_str());
        return false;
    }

    usdpointcloud::PointCloudAsset asset;
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(
            header, pointData, "LAS CRS unavailable; inspect VLR metadata",
            asset, assetError)) {
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  usdgeolas::diagnostics::BoundsTransformFailed,
                                  "Unable to build LAS point cloud asset: " +
                                      assetError)
                                  .c_str());
        return false;
    }
    bool authored = false;
    usdgeo::PointCloudAuthorFailure authorFailure =
        usdgeo::PointCloudAuthorFailure::PointCloud;
    if (request.lodProfile != usdpointcloud::LodProfile::Off) {
        std::vector<usdpointcloud::PointCloudAsset> levels;
        usdpointcloud::PointLodHierarchy hierarchy;
        if (!usdpointcloud::BuildPointLodAssets(
                asset, request.lodProfile, levels, hierarchy, diagnostics)) {
            authored = false;
        } else {
            authored = usdgeo::AuthorPointCloudLodAsset(
                layer, "/PointCloud", levels, hierarchy);
        }
    } else {
        authored = usdgeo::AuthorPointCloudAsset(layer, "/PointCloud", asset,
                                                 authorFailure);
    }
    if (!authored) {
        const char* code = usdgeolas::diagnostics::PointCloudAuthorFailed;
        if (authorFailure == usdgeo::PointCloudAuthorFailure::InvalidLayer ||
            authorFailure == usdgeo::PointCloudAuthorFailure::StageCreation) {
            code = usdgeolas::diagnostics::UsdLayerCreateFailed;
        } else if (authorFailure ==
                   usdgeo::PointCloudAuthorFailure::StageMetrics) {
            code = usdgeolas::diagnostics::StageMetricsFailed;
        }
        TF_RUNTIME_ERROR("%s", usdgeolas::diagnostics::Message(
                                  code,
                                  "Unable to author LAS point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    return true;
}

bool UsdGeoLasFileFormat::WriteToString(const SdfLayer& layer,
                                     std::string* str,
                                     const std::string& comment) const {
    const auto usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

TF_REGISTRY_FUNCTION(TfType) {
    SDF_DEFINE_FILE_FORMAT(UsdGeoLasFileFormat, SdfFileFormat);
}

PXR_NAMESPACE_CLOSE_SCOPE
