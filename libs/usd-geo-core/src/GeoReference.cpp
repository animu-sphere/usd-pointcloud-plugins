#include "usdgeo/GeoReference.h"

namespace usdgeo {

bool GeoReference::IsValid() const noexcept {
    const bool hasCrs = epsgCode.has_value() || !wkt.empty() || !projJson.empty();
    const bool hasUnit = !linearUnit.empty();
    const bool hasAxis = upAxis == "X" || upAxis == "Y" || upAxis == "Z";
    return hasCrs && hasUnit && hasAxis;
}

} // namespace usdgeo
