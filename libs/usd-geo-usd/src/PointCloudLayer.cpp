#include "usdgeo/PointCloudLayer.h"

#include <cstdint>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/points.h>

namespace usdgeo {

namespace {

bool IsValidPrimPath(const std::string& primPath) {
    return !primPath.empty() && primPath.front() == '/';
}

template <typename T>
bool HasPointCountOrIsEmpty(const std::vector<T>& values,
                            std::size_t pointCount) {
    return values.empty() || values.size() == pointCount;
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
    Data data;
    data.positions = positions;
    return AuthorPointCloud(stage, primPath, reference, bounds, chunk, data);
}

bool PointCloudLayer::AuthorPointCloud(
    const pxr::UsdStageRefPtr& stage,
    const std::string& primPath,
    const GeoReference& reference,
    const SpatialBounds& bounds,
    const usdpointcloud::PointChunk& chunk,
    const Data& data) {
    if (!stage || !IsValidPrimPath(primPath) || !reference.IsValid() ||
        !bounds.IsValid() || !chunk.IsValid() ||
        data.positions.size() != chunk.pointCount ||
        !HasPointCountOrIsEmpty(data.intensity, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.returnNumber, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.numberOfReturns, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.classification, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.classificationFlags, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.scannerChannel, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.scanDirectionFlag, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.edgeOfFlightLine, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.userData, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.scanAngle, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.pointSourceId, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.red, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.green, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.blue, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.nir, chunk.pointCount) ||
        !HasPointCountOrIsEmpty(data.gpsTime, chunk.pointCount) ||
        (data.returnNumber.empty() != data.numberOfReturns.empty()) ||
        (data.red.empty() != data.green.empty()) ||
        (data.red.empty() != data.blue.empty())) {
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
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scannerChannel"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.scannerChannel.begin(),
                                              data.scannerChannel.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scanDirectionFlag"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.scanDirectionFlag.begin(),
                                              data.scanDirectionFlag.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:edgeOfFlightLine"),
            pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.edgeOfFlightLine.begin(),
                                              data.edgeOfFlightLine.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:userData"), pxr::SdfValueTypeNames->UCharArray)
            .Set(pxr::VtArray<unsigned char>(data.userData.begin(),
                                              data.userData.end()));
        points.GetPrim().CreateAttribute(
            pxr::TfToken("geo:scanAngle"), pxr::SdfValueTypeNames->IntArray)
            .Set(ToIntArray(data.scanAngle));
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