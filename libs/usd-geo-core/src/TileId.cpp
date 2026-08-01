#include "usdgeo/TileId.h"

namespace usdgeo {

bool TileId::IsValid() const noexcept {
    return level >= 0;
}

std::string TileId::ToString() const {
    if (!IsValid()) {
        return {};
    }

    return "L" + std::to_string(level) + "/" + std::to_string(x) + "/" +
           std::to_string(y) + "/" + std::to_string(z);
}

} // namespace usdgeo