#pragma once

#include "usdgeo/SpatialBounds.h"

#include <optional>
#include <string>

namespace usdgeo {

// Dataset-level coordinate metadata and source-to-stage axis conversion.
struct GeoReference {
    std::optional<int> epsgCode;
    std::string wkt;
    std::string projJson;
    std::string linearUnit = "metre";
    std::string sourceUpAxis = "Z";
    std::string stageUpAxis = "Z";
    Vec3d localOrigin;

    bool IsValid() const noexcept;
    bool TryToLocal(const Vec3d& source, Vec3d& local) const noexcept;
    bool TryToLocal(const SpatialBounds& source,
                    SpatialBounds& local) const noexcept;
    bool TryToSource(const Vec3d& local, Vec3d& source) const noexcept;
};

} // namespace usdgeo
