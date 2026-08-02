#include "usdgeolaz/UsdGeoLazFileFormat.h"
#include "usdgeolaz/UsdGeoLazDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdpointcloud/FileFormatArguments.h"
#include "usdpointcloud/Sampling.h"
#include "usdlaz/Laz.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>
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

TF_DEFINE_PUBLIC_TOKENS(UsdGeoLazFileFormatTokens, USDGEOLAZ_FILE_FORMAT_TOKENS);

UsdGeoLazFileFormat::UsdGeoLazFileFormat()
    : SdfFileFormat(UsdGeoLazFileFormatTokens->Id,
                    UsdGeoLazFileFormatTokens->Version,
                    UsdGeoLazFileFormatTokens->Target,
                    UsdGeoLazFileFormatTokens->Extension) {}

UsdGeoLazFileFormat::~UsdGeoLazFileFormat() = default;

bool UsdGeoLazFileFormat::CanRead(const std::string& file) const {
    return SdfFileFormat::GetFileExtension(file) == "laz";
}

bool UsdGeoLazFileFormat::Read(SdfLayer* layer,
                            const std::string& resolvedPath,
                            bool metadataOnly) const {
    if (!layer) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::InvalidReadRequest,
                                  "geospatial-laz requires a writable layer")
                                  .c_str());
        return false;
    }

    std::string sourcePath;
    usdpointcloud::PointReadRequest request;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!MakeReadRequest(layer, resolvedPath, sourcePath, request,
                         diagnostics)) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::FormatArgumentInvalid,
                                  "Invalid LAZ file-format arguments: " +
                                      DiagnosticDetail(diagnostics,
                                                       "invalid arguments"))
                                  .c_str());
        return false;
    }

    if (metadataOnly) {
        auto decoder = usdlaz::CreateFileDecoder(sourcePath, diagnostics);
        if (!decoder) {
            TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                      usdgeolaz::diagnostics::FileOpenFailed,
                                      "Unable to open LAZ file " + sourcePath +
                                          ": " + DiagnosticDetail(
                                              diagnostics, "decoder could not be created"))
                                      .c_str());
            return false;
        }
        usdlaz::LazReader reader(std::move(decoder));
        usdlas::LasHeader header;
        if (!reader.ReadMetadata(header, diagnostics)) {
            TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                      usdgeolaz::diagnostics::DecodeFailed,
                                      "Unable to inspect LAZ file " + resolvedPath +
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
            TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::PointCloudAuthorFailed);
            return false;
        }
        return true;
    }

    auto decoder = usdlaz::CreateFileDecoder(sourcePath, diagnostics);
    if (!decoder) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::FileOpenFailed,
                                  "Unable to open LAZ file " + sourcePath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "decoder could not be created"))
                                  .c_str());
        return false;
    }

    usdlaz::LazReader reader(std::move(decoder));
    usdlas::LasHeader header;
    usdpointcloud::PointData pointData;
    const auto consumed = reader.Read(
        request.readOptions,
        [&](const usdlas::LasHeader& chunkHeader,
            const std::vector<usdlas::LasPoint>& points,
            std::string& error) {
            return usdlas::AppendPointData(chunkHeader, points, sourcePath,
                                           pointData, error);
        },
        header, diagnostics);
    if (!consumed) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::DecodeFailed,
                                  "Unable to decode LAZ file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "decode failed"))
                                  .c_str());
        return false;
    }

    std::string selectionError;
    if (!usdpointcloud::SelectPointDataAttributes(
            pointData, request.attributes, selectionError)) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::FormatArgumentInvalid,
                                  "Unable to select LAZ point attributes: " +
                                      selectionError)
                                  .c_str());
        return false;
    }

    usdpointcloud::PointCloudAsset asset;
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(
            header, pointData, "LAZ CRS unavailable; inspect VLR metadata",
            asset, assetError)) {
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  usdgeolaz::diagnostics::BoundsTransformFailed,
                                  "Unable to build LAZ point cloud asset: " +
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
        const char* code = usdgeolaz::diagnostics::PointCloudAuthorFailed;
        if (authorFailure == usdgeo::PointCloudAuthorFailure::InvalidLayer ||
            authorFailure == usdgeo::PointCloudAuthorFailure::StageCreation) {
            code = usdgeolaz::diagnostics::UsdLayerCreateFailed;
        } else if (authorFailure ==
                   usdgeo::PointCloudAuthorFailure::StageMetrics) {
            code = usdgeolaz::diagnostics::StageMetricsFailed;
        }
        TF_RUNTIME_ERROR("%s", usdgeolaz::diagnostics::Message(
                                  code,
                                  "Unable to author LAZ point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
    return true;
}

bool UsdGeoLazFileFormat::WriteToString(const SdfLayer& layer,
                                     std::string* str,
                                     const std::string& comment) const {
    const auto usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

TF_REGISTRY_FUNCTION(TfType) {
    SDF_DEFINE_FILE_FORMAT(UsdGeoLazFileFormat, SdfFileFormat);
}

PXR_NAMESPACE_CLOSE_SCOPE
