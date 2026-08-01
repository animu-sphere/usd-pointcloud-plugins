#include "geolas/GeoLasFileFormat.h"
#include "geolas/GeoLasDiagnostics.h"

#include "usdgeo/Diagnostic.h"
#include "usdgeo/PointCloudLayer.h"
#include "usdlas/Las.h"

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
    const std::vector<usdgeo::Diagnostic>& diagnostics,
    usdlas::LasReadFailure failure) {
    if (diagnostics.empty()) {
        return geolas::diagnostics::PointDecodeFailed;
    }
    switch (failure) {
    case usdlas::LasReadFailure::FileOpen:
        return geolas::diagnostics::FileOpenFailed;
    case usdlas::LasReadFailure::FileSize:
        return geolas::diagnostics::FileSizeUnavailable;
    case usdlas::LasReadFailure::PointDataTruncated:
        return geolas::diagnostics::PointDataTruncated;
    case usdlas::LasReadFailure::EvlrOffset:
        return geolas::diagnostics::EvlrOffsetInvalid;
    case usdlas::LasReadFailure::Vlr:
        return geolas::diagnostics::VlrInvalid;
    case usdlas::LasReadFailure::Evlr:
        return geolas::diagnostics::EvlrInvalid;
    case usdlas::LasReadFailure::PointDataSeek:
        return geolas::diagnostics::PointDataSeekFailed;
    case usdlas::LasReadFailure::PointDataRead:
        return geolas::diagnostics::PointReadFailed;
    case usdlas::LasReadFailure::PointDecode:
        return geolas::diagnostics::PointDecodeFailed;
    default:
        break;
    }

    switch (diagnostics.front().code) {
    case usdgeo::DiagnosticCode::NonFiniteCoordinate:
    case usdgeo::DiagnosticCode::DecodeFailure:
        return geolas::diagnostics::PointDecodeFailed;
    case usdgeo::DiagnosticCode::InvalidCrs:
    case usdgeo::DiagnosticCode::UnsupportedExtraBytesType:
        return geolas::diagnostics::VlrInvalid;
    default:
        return geolas::diagnostics::HeaderInvalid;
    }
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
    usdpointcloud::PointData pointData;
    usdlas::LasReader reader(resolvedPath);
    usdlas::LasReadOptions options;
    const auto consume = [&](const usdlas::LasHeader& chunkHeader,
                             const std::vector<usdlas::LasPoint>& points,
                             std::string& error) {
        return usdlas::AppendPointData(chunkHeader, points, resolvedPath,
                                        pointData, error);
    };
    if (!reader.Read(options, consume, header, diagnostics)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  ReaderDiagnosticCode(diagnostics,
                                                       reader.FailureKind()),
                                  "Unable to read LAS file " + resolvedPath +
                                      ": " +
                                      DiagnosticDetail(diagnostics, "read failed"))
                                  .c_str());
        return false;
    }

    usdpointcloud::PointCloudAsset asset;
    std::string assetError;
    if (!usdlas::BuildPointCloudAsset(
            header, pointData, "LAS CRS unavailable; inspect VLR metadata",
            asset, assetError)) {
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  geolas::diagnostics::BoundsTransformFailed,
                                  "Unable to build LAS point cloud asset: " +
                                      assetError)
                                  .c_str());
        return false;
    }
    usdgeo::PointCloudAuthorFailure authorFailure;
    if (!usdgeo::AuthorPointCloudAsset(layer, "/PointCloud", asset,
                                       authorFailure)) {
        const char* code = geolas::diagnostics::PointCloudAuthorFailed;
        if (authorFailure == usdgeo::PointCloudAuthorFailure::InvalidLayer ||
            authorFailure == usdgeo::PointCloudAuthorFailure::StageCreation) {
            code = geolas::diagnostics::UsdLayerCreateFailed;
        } else if (authorFailure ==
                   usdgeo::PointCloudAuthorFailure::StageMetrics) {
            code = geolas::diagnostics::StageMetricsFailed;
        }
        TF_RUNTIME_ERROR("%s", geolas::diagnostics::Message(
                                  code,
                                  "Unable to author LAS point cloud to USD layer: " +
                                      resolvedPath)
                                  .c_str());
        return false;
    }
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
