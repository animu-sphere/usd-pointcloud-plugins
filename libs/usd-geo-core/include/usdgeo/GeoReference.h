#pragma once

#include "usdgeo/SpatialBounds.h"

#include <optional>
#include <string>

namespace usdgeo {

// Dataset-level coordinate metadata. Coordinates remain in source units until
// a later transform explicitly maps them into stage-local space.
struct GeoReference {
    std::optional<int> epsgCode;
    std::string wkt;
    std::string projJson;
    std::string linearUnit = "metre";
    std::string upAxis = "Z";
    Vec3d localOrigin;

    bool IsValid() const noexcept;
    bool TryToLocal(const Vec3d& source, Vec3d& local) const noexcept;
    bool TryToLocal(const SpatialBounds& source,
                    SpatialBounds& local) const noexcept;
    bool TryToSource(const Vec3d& local, Vec3d& source) const noexcept;
};

} // namespace usdgeo
