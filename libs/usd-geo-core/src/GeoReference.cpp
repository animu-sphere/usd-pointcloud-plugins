#include "usdgeo/GeoReference.h"

namespace usdgeo {

bool GeoReference::IsValid() const noexcept {
    const bool hasCrs = epsgCode.has_value() || !wkt.empty() || !projJson.empty();
    const bool hasUnit = !linearUnit.empty();
    const bool hasAxis = upAxis == "X" || upAxis == "Y" || upAxis == "Z";
    return hasCrs && hasUnit && hasAxis && localOrigin.IsFinite();
}

bool GeoReference::TryToLocal(const Vec3d& source, Vec3d& local) const noexcept {
    if (!IsValid() || !source.IsFinite()) {
        return false;
    }

    local = {source.x - localOrigin.x,
             source.y - localOrigin.y,
             source.z - localOrigin.z};
    return local.IsFinite();
}

bool GeoReference::TryToSource(const Vec3d& local, Vec3d& source) const noexcept {
    if (!IsValid() || !local.IsFinite()) {
        return false;
    }

    source = {local.x + localOrigin.x,
              local.y + localOrigin.y,
              local.z + localOrigin.z};
    return source.IsFinite();
}

} // namespace usdgeo
