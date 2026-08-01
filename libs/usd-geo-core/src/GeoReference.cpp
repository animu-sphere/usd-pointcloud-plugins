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

    const Vec3d delta{source.x - localOrigin.x,
                      source.y - localOrigin.y,
                      source.z - localOrigin.z};
    local = upAxis == "Y" ? Vec3d{delta.x, delta.z, -delta.y} : delta;
    return local.IsFinite();
}

bool GeoReference::TryToLocal(const SpatialBounds& source,
                              SpatialBounds& local) const noexcept {
    if (!IsValid() || !source.IsValid()) {
        return false;
    }

    local = SpatialBounds::Empty();
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                const Vec3d corner{
                    x == 0 ? source.minimum.x : source.maximum.x,
                    y == 0 ? source.minimum.y : source.maximum.y,
                    z == 0 ? source.minimum.z : source.maximum.z};
                Vec3d transformed;
                if (!TryToLocal(corner, transformed)) {
                    return false;
                }
                local.Expand(transformed);
            }
        }
    }
    return local.IsValid();
}

bool GeoReference::TryToSource(const Vec3d& local, Vec3d& source) const noexcept {
    if (!IsValid() || !local.IsFinite()) {
        return false;
    }

    const Vec3d delta = upAxis == "Y"
                            ? Vec3d{local.x, -local.z, local.y}
                            : local;
    source = {delta.x + localOrigin.x,
              delta.y + localOrigin.y,
              delta.z + localOrigin.z};
    return source.IsFinite();
}

} // namespace usdgeo
