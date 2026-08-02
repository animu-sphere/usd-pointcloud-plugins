#include "usdgeo/PointCloudLayer.h"

#include <cstdint>
#include <filesystem>
#include <set>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/sdf/payload.h>
#include <pxr/usd/usd/payloads.h>
#include <pxr/usd/usdLod/rootAPI.h>
#include <pxr/usd/usdLod/screenSizeHeuristic.h>
#include <pxr/usd/usdLod/tokens.h>

namespace usdgeo {

namespace {

bool IsValidPrimPath(const std::string& primPath) {
    return !primPath.empty() && primPath.front() == '/';
}

bool SameBounds(const usdgeo::SpatialBounds& left,
                const usdgeo::SpatialBounds& right) {
    return left.minimum.x == right.minimum.x &&
           left.minimum.y == right.minimum.y &&
           left.minimum.z == right.minimum.z &&
           left.maximum.x == right.maximum.x &&
           left.maximum.y == right.maximum.y &&
           left.maximum.z == right.maximum.z;
}

bool IsValidMetadataChunk(const usdpointcloud::PointChunk& chunk) {
    if (chunk.pointCount != 0) {
        return chunk.IsValid();
    }
    if (!chunk.bounds.IsValid()) {
        return false;
    }
    std::set<std::string> names;
    for (const auto& attribute : chunk.attributes) {
        if (!attribute.IsValid() || !names.insert(attribute.name).second) {
            return false;
        }
    }
    return true;
}

pxr::VtIntArray ToIntArray(const std::vector<std::uint16_t>& values) {
    pxr::VtIntArray result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<int>(value));
    }
    return result;
}

pxr::VtIntArray ToIntArray(const std::vector<std::int16_t>& values) {
    pxr::VtIntArray result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<int>(value));
    }
    return result;
}

std::string TilePrimName(const usdpointcloud::PointTileId& id) {
    const auto axisName = [](std::int64_t value) {
        if (value >= 0) {
            return "p" + std::to_string(static_cast<std::uint64_t>(value));
        }
        const auto magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1;
        return "n" + std::to_string(magnitude);
    };
    return "Tile_L" + std::to_string(id.level) + "_" + axisName(id.x) +
           "_" + axisName(id.y) + "_" + axisName(id.z);
}

bool AuthorLodRoot(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<usdpointcloud::PointCloudAsset>& levels,
    const usdpointcloud::PointLodHierarchy& hierarchy,
    const pxr::SdfPath& heuristicPath,
    bool defineHeuristic,
    const std::filesystem::path* payloadDirectory = nullptr,
    const std::filesystem::path* rootLayerPath = nullptr) {
    if (!stage || !IsValidPrimPath(primPath) ||
        levels.size() != hierarchy.items.size()) {
        return false;
    }
    const auto root = pxr::UsdGeomXform::Define(stage, pxr::SdfPath(primPath));
    if (!root || !pxr::UsdLodRootAPI::Apply(root.GetPrim())) {
        return false;
    }
    const auto lodRoot = pxr::UsdLodRootAPI(root.GetPrim());
    if (!lodRoot.CreateLodDefaultIndexAttr().Set(
            static_cast<int>(hierarchy.defaultIndex))) {
        return false;
    }
    if (!hierarchy.screenSizeThresholds.empty()) {
        const auto heuristic =
            defineHeuristic
                ? pxr::UsdLodScreenSizeHeuristic::Define(stage, heuristicPath)
                : pxr::UsdLodScreenSizeHeuristic::Get(stage, heuristicPath);
        if (!heuristic ||
            (defineHeuristic &&
             (!heuristic.CreateLodDomainAttr().Set(pxr::UsdLodTokens->imaging) ||
              !heuristic.CreateThresholdsAttr().Set(pxr::VtArray<float>(
                  hierarchy.screenSizeThresholds.begin(),
                  hierarchy.screenSizeThresholds.end())))) ||
            !lodRoot.CreateLodHeuristicsRel().AddTarget(heuristicPath)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < levels.size(); ++index) {
        const auto& level = levels[index];
        const auto lodName = "LOD" + std::to_string(index);
        const auto lodPath = primPath + "/" + lodName;
        if (payloadDirectory == nullptr) {
            if (!AuthorPointCloudAsset(stage, lodPath, level.reference,
                                       level.bounds, level.chunk, level.data)) {
                return false;
            }
            continue;
        }

        const auto payloadPath = *payloadDirectory /
            (primPath.substr(primPath.find_last_of('/') + 1) + "_" +
             lodName + ".usdc");
        std::error_code relativeError;
        const auto payloadIdentifier = std::filesystem::relative(
            payloadPath, rootLayerPath->parent_path(), relativeError);
        if (relativeError || payloadIdentifier.empty() ||
            payloadIdentifier.is_absolute()) {
            return false;
        }
        const auto payloadStage = pxr::UsdStage::CreateNew(payloadPath.string());
        if (!payloadStage || !pxr::UsdGeomXform::Define(
                                  payloadStage, pxr::SdfPath("/" + lodName)) ||
            !AuthorPointCloudAsset(payloadStage, "/" + lodName + "/Points",
                                   level.reference, level.bounds, level.chunk,
                                   level.data) ||
            !payloadStage->GetRootLayer()->Save()) {
            return false;
        }
        const auto lodPrim = pxr::UsdGeomXform::Define(
            stage, pxr::SdfPath(lodPath));
        if (!lodPrim || !lodPrim.GetPrim().GetPayloads().AddPayload(
                pxr::SdfPayload(payloadIdentifier.generic_string(),
                                pxr::SdfPath("/" + lodName)))) {
            return false;
        }
    }
    return true;
}

} // namespace

pxr::UsdStageRefPtr PointCloudLayer::CreateStage() {
    return pxr::UsdStage::CreateInMemory();
}

bool PointCloudLayer::AuthorPointCloud(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const std::vector<Vec3d>& positions) {
    return AuthorPointCloudAsset(
        stage, primPath, reference, bounds, chunk, positions);
}

bool PointCloudLayer::AuthorPointCloud(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const Data& data) {
    return AuthorPointCloudAsset(
        stage, primPath, reference, bounds, chunk, data);
}

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const usdpointcloud::PointCloudAsset& asset) {
    PointCloudAuthorFailure failure;
    return AuthorPointCloudAsset(layer, primPath, asset, failure);
}

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const usdpointcloud::PointCloudAsset& asset,
    PointCloudAuthorFailure& failure) {
    return AuthorPointCloudAsset(layer, primPath, asset.reference,
                                 asset.bounds, asset.chunk, asset.data, failure);
}

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data) {
    PointCloudAuthorFailure failure;
    return AuthorPointCloudAsset(layer, primPath, reference, bounds, chunk,
                                 data, failure);
}

bool AuthorPointCloudAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data,
    PointCloudAuthorFailure& failure) {
    failure = PointCloudAuthorFailure::None;
    if (!layer) {
        failure = PointCloudAuthorFailure::InvalidLayer;
        return false;
    }

    const auto stage = PointCloudLayer::CreateStage();
    if (!stage) {
        failure = PointCloudAuthorFailure::StageCreation;
        return false;
    }
    if (!pxr::UsdGeomSetStageUpAxis(
            stage, pxr::TfToken(reference.stageUpAxis)) ||
        !pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0)) {
        failure = PointCloudAuthorFailure::StageMetrics;
        return false;
    }
    if (!AuthorPointCloudAsset(stage, primPath, reference, bounds, chunk,
                               data)) {
        failure = PointCloudAuthorFailure::PointCloud;
        return false;
    }

    layer->TransferContent(stage->GetRootLayer());
    return true;
}

bool AuthorPointCloudAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const std::vector<Vec3d>& positions) {
    PointCloudLayer::Data data;
    data.positions = positions;
    return AuthorPointCloudAsset(stage, primPath, reference, bounds, chunk,
                                 data);
}

bool AuthorPointCloudAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudLayer::Data& data) {
    if (!stage || !IsValidPrimPath(primPath) || !reference.IsValid() ||
        !bounds.IsValid() || !chunk.IsValid() || !data.IsValid() ||
        data.positions.size() != chunk.pointCount) {
        return false;
    }

    pxr::VtVec3fArray localPositions;
    localPositions.reserve(data.positions.size());
    for (const Vec3d& position : data.positions) {
        Vec3d local;
        if (!reference.TryToLocal(position, local)) {
            return false;
        }
        localPositions.push_back(pxr::GfVec3f(
            static_cast<float>(local.x), static_cast<float>(local.y),
            static_cast<float>(local.z)));
    }

    auto points = pxr::UsdGeomPoints::Define(stage, pxr::SdfPath(primPath));
    if (!points) {
        return false;
    }

    points.GetPointsAttr().Set(localPositions);
    if (!data.intensity.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:intensity"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.intensity));
    }
    if (!data.returnNumber.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:returnNumber"), pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.returnNumber.begin(),
                                              data.returnNumber.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:numberOfReturns"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.numberOfReturns.begin(),
                                              data.numberOfReturns.end()));
    }
    if (!data.classification.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:classification"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.classification.begin(),
                                              data.classification.end()));
    }
    if (!data.classificationFlags.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:classificationFlags"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.classificationFlags.begin(),
                                              data.classificationFlags.end()));
    }
    if (!data.scannerChannel.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scannerChannel"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.scannerChannel.begin(),
                                              data.scannerChannel.end()));
    }
    if (!data.scanDirectionFlag.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scanDirectionFlag"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.scanDirectionFlag.begin(),
                                              data.scanDirectionFlag.end()));
    }
    if (!data.edgeOfFlightLine.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:edgeOfFlightLine"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.edgeOfFlightLine.begin(),
                                              data.edgeOfFlightLine.end()));
    }
    if (!data.userData.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:userData"), pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.userData.begin(),
                                              data.userData.end()));
    }
    if (!data.scanAngle.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scanAngle"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.scanAngle));
    }
    if (!data.pointSourceId.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:pointSourceId"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.pointSourceId));
    }
    if (!data.red.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:red"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.red));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:green"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.green));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:blue"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.blue));
    }
    if (!data.nir.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:nir"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.nir));
    }
    if (!data.gpsTime.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:gpsTime"), pxr::SdfValueTypeNames->DoubleArray)
            .Set(pxr::VtArray<double>(data.gpsTime.begin(), data.gpsTime.end()));
    }
    if (!data.waveformDescriptorIndex.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformDescriptorIndex"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.waveformDescriptorIndex.begin(),
                                              data.waveformDescriptorIndex.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformDataOffset"),
            pxr::SdfValueTypeNames->UInt64Array)
            .Set(pxr::VtArray<std::uint64_t>(data.waveformDataOffset.begin(),
                                              data.waveformDataOffset.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformPacketSize"),
            pxr::SdfValueTypeNames->UIntArray)
            .Set(pxr::VtArray<unsigned int>(data.waveformPacketSize.begin(),
                                             data.waveformPacketSize.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:returnPointWaveformLocation"),
            pxr::SdfValueTypeNames->FloatArray)
            .Set(pxr::VtArray<float>(data.returnPointWaveformLocation.begin(),
                                     data.returnPointWaveformLocation.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformXt"), pxr::SdfValueTypeNames->FloatArray)
            .Set(pxr::VtArray<float>(data.waveformXt.begin(), data.waveformXt.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformYt"), pxr::SdfValueTypeNames->FloatArray)
            .Set(pxr::VtArray<float>(data.waveformYt.begin(), data.waveformYt.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformZt"), pxr::SdfValueTypeNames->FloatArray)
            .Set(pxr::VtArray<float>(data.waveformZt.begin(), data.waveformZt.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformDataExternal"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.waveformDataExternal.begin(),
                                              data.waveformDataExternal.end()));
    }
    if (!data.waveformDataFile.empty()) {
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:waveformDataFile"),
            pxr::SdfValueTypeNames->String)
            .Set(data.waveformDataFile);
    }
    for (std::size_t index = 0; index < data.extraBytes.size(); ++index) {
        if (index >= data.extraByteNames.size() ||
            data.extraByteNames[index].empty() || data.extraBytes[index].empty()) {
            continue;
        }
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:" + data.extraByteNames[index]),
            pxr::SdfValueTypeNames->DoubleArray)
            .Set(pxr::VtArray<double>(data.extraBytes[index].begin(),
                                      data.extraBytes[index].end()));
    }

    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:epsgCode"), pxr::SdfValueTypeNames->Int)
        .Set(reference.epsgCode.value_or(0));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:wkt"), pxr::SdfValueTypeNames->String)
        .Set(reference.wkt);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:projJson"), pxr::SdfValueTypeNames->String)
        .Set(reference.projJson);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:linearUnit"), pxr::SdfValueTypeNames->String)
        .Set(reference.linearUnit);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:upAxis"), pxr::SdfValueTypeNames->String)
        .Set(reference.stageUpAxis);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:localOrigin"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(reference.localOrigin.x, reference.localOrigin.y,
                          reference.localOrigin.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:boundsMin"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(bounds.minimum.x, bounds.minimum.y,
                          bounds.minimum.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:boundsMax"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(bounds.maximum.x, bounds.maximum.y,
                          bounds.maximum.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:pointCount"), pxr::SdfValueTypeNames->UInt64)
        .Set(chunk.pointCount);

    return true;
}

bool AuthorPointCloudMetadata(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const PointCloudSourceMetadata& sourceMetadata) {
    if (!layer || !IsValidPrimPath(primPath) || !reference.IsValid() ||
        !bounds.IsValid() || !IsValidMetadataChunk(chunk)) {
        return false;
    }

    const auto stage = PointCloudLayer::CreateStage();
    if (!stage || !pxr::UsdGeomSetStageUpAxis(
                      stage, pxr::TfToken(reference.stageUpAxis)) ||
        !pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0)) {
        return false;
    }

    const auto points = pxr::UsdGeomPoints::Define(
        stage, pxr::SdfPath(primPath));
    if (!points) {
        return false;
    }
    points.GetPointsAttr().Set(pxr::VtVec3fArray());
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:epsgCode"), pxr::SdfValueTypeNames->Int)
        .Set(reference.epsgCode.value_or(0));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:wkt"), pxr::SdfValueTypeNames->String)
        .Set(reference.wkt);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:projJson"), pxr::SdfValueTypeNames->String)
        .Set(reference.projJson);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:linearUnit"), pxr::SdfValueTypeNames->String)
        .Set(reference.linearUnit);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:upAxis"), pxr::SdfValueTypeNames->String)
        .Set(reference.stageUpAxis);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:localOrigin"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(reference.localOrigin.x, reference.localOrigin.y,
                          reference.localOrigin.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:boundsMin"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(bounds.minimum.x, bounds.minimum.y,
                          bounds.minimum.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:boundsMax"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(bounds.maximum.x, bounds.maximum.y,
                          bounds.maximum.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:pointCount"), pxr::SdfValueTypeNames->UInt64)
        .Set(chunk.pointCount);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:pointFormat"), pxr::SdfValueTypeNames->UChar)
        .Set(sourceMetadata.pointFormat);
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:scale"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(sourceMetadata.scale.x, sourceMetadata.scale.y,
                          sourceMetadata.scale.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:offset"), pxr::SdfValueTypeNames->Double3)
        .Set(pxr::GfVec3d(sourceMetadata.offset.x, sourceMetadata.offset.y,
                          sourceMetadata.offset.z));
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:metadataOnly"), pxr::SdfValueTypeNames->Bool)
        .Set(true);
    pxr::VtStringArray availableAttributes;
    availableAttributes.reserve(chunk.attributes.size());
    for (const auto& attribute : chunk.attributes) {
        availableAttributes.push_back(attribute.name);
    }
    points.GetPrim().CreateAttribute(
        pxr::TfToken("geo:availableAttributes"),
        pxr::SdfValueTypeNames->StringArray)
        .Set(availableAttributes);

    layer->TransferContent(stage->GetRootLayer());
    return true;
}

bool AuthorPointCloudLodAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<usdpointcloud::PointCloudAsset>& levels,
    const usdpointcloud::PointLodHierarchy& hierarchy) {
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!stage || !IsValidPrimPath(primPath) ||
        levels.size() != hierarchy.items.size() ||
        !usdpointcloud::ValidatePointLodHierarchy(hierarchy, diagnostics)) {
        return false;
    }
    for (std::size_t index = 0; index < levels.size(); ++index) {
        const auto& level = levels[index];
        const auto& item = hierarchy.items[index];
        if (!level.IsValid() || level.chunk.pointCount != item.pointCount ||
            !SameBounds(level.bounds, item.bounds)) {
            return false;
        }
    }

    return AuthorLodRoot(
        stage, primPath, levels, hierarchy,
        pxr::SdfPath(primPath + "/LodHeuristics/ScreenSize"), true);
}

bool AuthorPointCloudLodAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const std::vector<usdpointcloud::PointCloudAsset>& levels,
    const usdpointcloud::PointLodHierarchy& hierarchy) {
    if (!layer || levels.empty() || !levels.front().reference.IsValid()) {
        return false;
    }
    const auto stage = PointCloudLayer::CreateStage();
    if (!stage || !pxr::UsdGeomSetStageUpAxis(
                      stage, pxr::TfToken(levels.front().reference.stageUpAxis)) ||
        !pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0) ||
        !AuthorPointCloudLodAsset(stage, primPath, levels, hierarchy)) {
        return false;
    }
    layer->TransferContent(stage->GetRootLayer());
    return true;
}

bool AuthorPointCloudTiledAsset(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles) {
    if (!stage || !IsValidPrimPath(primPath) || tiles.empty()) {
        return false;
    }
    std::set<std::string> tileNames;
    std::vector<float> sharedThresholds;
    bool thresholdsInitialized = false;
    for (const auto& tile : tiles) {
        std::vector<usdgeo::Diagnostic> diagnostics;
        if (!usdpointcloud::ValidatePointTile(tile.tile, diagnostics) ||
            tile.levels.size() != tile.tile.lod.items.size() ||
            !tileNames.insert(tile.tile.id.ToString()).second) {
            return false;
        }
        for (std::size_t index = 0; index < tile.levels.size(); ++index) {
            const auto& level = tile.levels[index];
            const auto& item = tile.tile.lod.items[index];
            if (!level.IsValid() || level.chunk.pointCount != item.pointCount ||
                !SameBounds(level.bounds, item.bounds)) {
                return false;
            }
        }
        if (!thresholdsInitialized) {
            sharedThresholds = tile.tile.lod.screenSizeThresholds;
            thresholdsInitialized = true;
        } else if (tile.tile.lod.screenSizeThresholds != sharedThresholds) {
            return false;
        }
    }

    if (!pxr::UsdGeomXform::Define(stage, pxr::SdfPath(primPath)) ||
        !pxr::UsdGeomScope::Define(
            stage, pxr::SdfPath(primPath + "/LodHeuristics")) ||
        !pxr::UsdGeomScope::Define(stage, pxr::SdfPath(primPath + "/Tiles"))) {
        return false;
    }
    const auto heuristicPath =
        pxr::SdfPath(primPath + "/LodHeuristics/ScreenSize");
    bool defineHeuristic = true;
    for (const auto& tile : tiles) {
        const auto tilePath = pxr::SdfPath(
            primPath + "/Tiles/" + TilePrimName(tile.tile.id));
        if (!AuthorLodRoot(stage, tilePath.GetString(), tile.levels,
                           tile.tile.lod, heuristicPath, defineHeuristic)) {
            return false;
        }
        defineHeuristic = false;
    }
    return true;
}

bool AuthorPointCloudTiledAssetWithPayloads(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles,
    const PointCloudPayloadOptions& options) {
    if (!stage || !IsValidPrimPath(primPath) || tiles.empty() ||
        options.directory.empty() || options.rootLayerPath.empty()) {
        return false;
    }

    const std::filesystem::path payloadDirectory(options.directory);
    const std::filesystem::path rootLayerPath(options.rootLayerPath);
    std::error_code error;
    std::filesystem::create_directories(payloadDirectory, error);
    if (error) {
        return false;
    }

    std::vector<std::filesystem::path> payloadPaths;
    for (const auto& tile : tiles) {
        const auto tilePath = primPath + "/Tiles/" + TilePrimName(tile.tile.id);
        for (std::size_t index = 0; index < tile.levels.size(); ++index) {
            const auto payloadPath = payloadDirectory /
                (tilePath.substr(tilePath.find_last_of('/') + 1) + "_LOD" +
                 std::to_string(index) + ".usdc");
            if (std::filesystem::exists(payloadPath)) {
                return false;
            }
            payloadPaths.push_back(payloadPath);
        }
    }
    const auto cleanup = [&payloadPaths]() {
        std::error_code cleanupError;
        for (const auto& payloadPath : payloadPaths) {
            std::filesystem::remove(payloadPath, cleanupError);
            cleanupError.clear();
        }
    };

    std::set<std::string> tileNames;
    std::vector<float> sharedThresholds;
    bool thresholdsInitialized = false;
    for (const auto& tile : tiles) {
        std::vector<usdgeo::Diagnostic> diagnostics;
        if (!usdpointcloud::ValidatePointTile(tile.tile, diagnostics) ||
            tile.levels.size() != tile.tile.lod.items.size() ||
            !tileNames.insert(tile.tile.id.ToString()).second) {
            return false;
        }
        if (!thresholdsInitialized) {
            sharedThresholds = tile.tile.lod.screenSizeThresholds;
            thresholdsInitialized = true;
        } else if (tile.tile.lod.screenSizeThresholds != sharedThresholds) {
            return false;
        }
        for (std::size_t index = 0; index < tile.levels.size(); ++index) {
            const auto& level = tile.levels[index];
            const auto& item = tile.tile.lod.items[index];
            if (!level.IsValid() || level.chunk.pointCount != item.pointCount ||
                !SameBounds(level.bounds, item.bounds)) {
                return false;
            }
        }
    }

    if (!pxr::UsdGeomXform::Define(stage, pxr::SdfPath(primPath)) ||
        !pxr::UsdGeomScope::Define(
            stage, pxr::SdfPath(primPath + "/LodHeuristics")) ||
        !pxr::UsdGeomScope::Define(
            stage, pxr::SdfPath(primPath + "/Tiles"))) {
        return false;
    }

    const auto heuristicPath =
        pxr::SdfPath(primPath + "/LodHeuristics/ScreenSize");
    bool defineHeuristic = true;
    for (const auto& tile : tiles) {
        const auto tilePath = pxr::SdfPath(
            primPath + "/Tiles/" + TilePrimName(tile.tile.id));
        if (!AuthorLodRoot(stage, tilePath.GetString(), tile.levels,
                           tile.tile.lod, heuristicPath, defineHeuristic,
                           &payloadDirectory, &rootLayerPath)) {
            cleanup();
            return false;
        }
        defineHeuristic = false;
    }
    return true;
}

bool AuthorPointCloudTiledAsset(
    pxr::SdfLayer* layer,
    const std::string& primPath,
    const std::vector<PointCloudTileAsset>& tiles) {
    if (!layer || tiles.empty() || tiles.front().levels.empty() ||
        !tiles.front().levels.front().reference.IsValid()) {
        return false;
    }
    const auto stage = PointCloudLayer::CreateStage();
    if (!stage || !pxr::UsdGeomSetStageUpAxis(
                      stage, pxr::TfToken(
                          tiles.front().levels.front().reference.stageUpAxis)) ||
        !pxr::UsdGeomSetStageMetersPerUnit(stage, 1.0) ||
        !AuthorPointCloudTiledAsset(stage, primPath, tiles)) {
        return false;
    }
    layer->TransferContent(stage->GetRootLayer());
    return true;
}

} // namespace usdgeo