#include "geolaz/GeoLazFileFormat.h"
#include "geolaz/GeoLazDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
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
    usdpointcloud::PointData pointData;
    const auto consumed = reader.Read(
        {},
        [&](const usdlas::LasHeader& chunkHeader,
            const std::vector<usdlas::LasPoint>& points,
            std::string& error) {
            return usdlas::AppendPointData(chunkHeader, points, resolvedPath,
                                           pointData, error);
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

    usdpointcloud::PointCloudAsset asset;
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(
            header, pointData, "LAZ CRS unavailable; inspect VLR metadata",
            asset, assetError)) {
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  geolaz::diagnostics::BoundsTransformFailed,
                                  "Unable to build LAZ point cloud asset: " +
                                      assetError)
                                  .c_str());
        return false;
    }
    usdgeo::PointCloudAuthorFailure authorFailure;
    if (!usdgeo::AuthorPointCloudAsset(layer, "/PointCloud", asset,
                                       authorFailure)) {
        const char* code = geolaz::diagnostics::PointCloudAuthorFailed;
        if (authorFailure == usdgeo::PointCloudAuthorFailure::InvalidLayer ||
            authorFailure == usdgeo::PointCloudAuthorFailure::StageCreation) {
            code = geolaz::diagnostics::UsdLayerCreateFailed;
        } else if (authorFailure ==
                   usdgeo::PointCloudAuthorFailure::StageMetrics) {
            code = geolaz::diagnostics::StageMetricsFailed;
        }
        TF_RUNTIME_ERROR("%s", geolaz::diagnostics::Message(
                                  code,
                                  "Unable to author LAZ point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
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
