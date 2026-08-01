#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace usdgeo {

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct SpatialBounds {
    Vec3d minimum;
    Vec3d maximum;

    static SpatialBounds Empty() noexcept;
    bool IsValid() const noexcept;
    Vec3d Center() const noexcept;
    Vec3d Size() const noexcept;
    void Expand(const Vec3d& point) noexcept;
};

} // namespace usdgeo
