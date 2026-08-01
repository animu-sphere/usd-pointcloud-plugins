#include "usdgeo/SpatialBounds.h"

namespace usdgeo {

bool Vec3d::IsFinite() const noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

SpatialBounds SpatialBounds::Empty() noexcept {
    const double infinity = std::numeric_limits<double>::infinity();
    return {{infinity, infinity, infinity},
            {-infinity, -infinity, -infinity}};
}

bool SpatialBounds::IsValid() const noexcept {
    return std::isfinite(minimum.x) && std::isfinite(minimum.y) &&
           std::isfinite(minimum.z) && std::isfinite(maximum.x) &&
           std::isfinite(maximum.y) && std::isfinite(maximum.z) &&
           minimum.x <= maximum.x && minimum.y <= maximum.y &&
           minimum.z <= maximum.z;
}

Vec3d SpatialBounds::Center() const noexcept {
    return {(minimum.x + maximum.x) * 0.5,
            (minimum.y + maximum.y) * 0.5,
            (minimum.z + maximum.z) * 0.5};
}

Vec3d SpatialBounds::Size() const noexcept {
    return {maximum.x - minimum.x,
            maximum.y - minimum.y,
            maximum.z - minimum.z};
}

void SpatialBounds::Expand(const Vec3d& point) noexcept {
    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
    maximum.z = std::max(maximum.z, point.z);
}

} // namespace usdgeo
