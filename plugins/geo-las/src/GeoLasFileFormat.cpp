#include "geolas/GeoLasFileFormat.h"

#include "usdgeo/PointCloudLayer.h"
#include "usdlas/Las.h"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/registryManager.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

bool ReadFile(const std::string& path, std::vector<std::uint8_t>& bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return false;
    }
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uintmax_t>(size) >
                        std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    return bytes.empty() || static_cast<bool>(input.read(
                              reinterpret_cast<char*>(bytes.data()),
                              static_cast<std::streamsize>(bytes.size())));
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
        TF_RUNTIME_ERROR("geoLas requires a writable layer and full point data");
        return false;
    }

    std::vector<std::uint8_t> bytes;
    usdlas::LasHeader header;
    std::string error;
    if (!ReadFile(resolvedPath, bytes) ||
        !usdlas::InspectHeader(bytes, header, error)) {
        TF_RUNTIME_ERROR("Unable to inspect LAS file {}: {}", resolvedPath,
                         error);
        return false;
    }

    const auto recordLength = static_cast<std::size_t>(header.pointRecordLength);
    const auto pointOffset = static_cast<std::size_t>(header.pointDataOffset);
    if (header.pointCount >
            (std::numeric_limits<std::size_t>::max() - pointOffset) /
                recordLength ||
        pointOffset + static_cast<std::size_t>(header.pointCount) *
                          recordLength >
            bytes.size()) {
        TF_RUNTIME_ERROR("LAS point data is truncated: {}", resolvedPath);
        return false;
    }

    std::vector<usdgeo::Vec3d> positions;
    positions.reserve(static_cast<std::size_t>(header.pointCount));
    for (std::uint64_t index = 0; index < header.pointCount; ++index) {
        const auto offset = pointOffset + static_cast<std::size_t>(index) *
                                            recordLength;
        std::vector<std::uint8_t> record(bytes.begin() + offset,
                                         bytes.begin() + offset + recordLength);
        usdlas::LasPoint point;
        if (!usdlas::DecodePoint(header, record, point, error)) {
            TF_RUNTIME_ERROR("Unable to decode LAS point {}: {}", index, error);
            return false;
        }
        positions.push_back(point.sourcePosition);
    }

    usdgeo::GeoReference reference;
    reference.wkt = "LAS CRS unavailable; inspect VLR metadata";
    reference.upAxis = "Y";
    reference.localOrigin = header.bounds.minimum;
    usdgeo::SpatialBounds bounds;
    if (!reference.TryToLocal(header.bounds, bounds)) {
        TF_RUNTIME_ERROR("Unable to transform LAS bounds to USD: {}",
                         resolvedPath);
        return false;
    }
    usdpointcloud::PointChunk chunk;
    chunk.pointCount = header.pointCount;
    chunk.bounds = bounds;
    chunk.attributes.push_back({"classification",
                                usdpointcloud::PointAttributeType::UInt8});

    const auto usda = SdfFileFormat::FindByExtension("usda");
    const auto generated = SdfLayer::CreateAnonymous(
        "geo-las.generated.usda", usda);
    const auto stage = UsdStage::Open(generated);
    if (!stage) {
        TF_RUNTIME_ERROR("Unable to create a USD layer for LAS: {}",
                         resolvedPath);
        return false;
    }
    if (!UsdGeomSetStageUpAxis(stage, TfToken("Y")) ||
        !UsdGeomSetStageMetersPerUnit(stage, 1.0)) {
        TF_RUNTIME_ERROR("Unable to set USD stage metrics for LAS: {}",
                         resolvedPath);
        return false;
    }
    if (!usdgeo::PointCloudLayer::AuthorPointCloud(
            stage, "/PointCloud", reference, bounds, chunk, positions)) {
        TF_RUNTIME_ERROR("Unable to author LAS point cloud to USD layer: {}",
                         resolvedPath);
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
