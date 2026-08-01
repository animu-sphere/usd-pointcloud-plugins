#include "usdgeo/PointCloudLayer.h"

#include <cstdint>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/points.h>

namespace usdgeo {

namespace {

bool IsValidPrimPath(const std::string& primPath) {
    return !primPath.empty() && primPath.front() == '/';
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

} // namespace usdgeo