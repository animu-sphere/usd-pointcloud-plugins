#pragma once

#include <cstdint>
#include <string>

namespace usdgeo {

struct TileId {
    int level = 0;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool IsValid() const noexcept;
    std::string ToString() const;
};

} // namespace usdgeo