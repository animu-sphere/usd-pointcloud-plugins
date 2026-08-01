#include "usdpointcloud/PointCloud.h"

#include <algorithm>
#include <limits>

namespace usdpointcloud {

bool PointAttribute::IsValid() const noexcept {
    return !name.empty();
}

bool PointChunk::IsValid() const noexcept {
    if (pointCount == 0) {
        return !bounds.IsValid() && attributes.empty();
    }
    if (!bounds.IsValid()) {
        return false;
    }

    for (const auto& attribute : attributes) {
        if (!attribute.IsValid()) {
            return false;
        }
    }
    for (auto first = attributes.begin(); first != attributes.end(); ++first) {
        if (std::any_of(first + 1, attributes.end(),
                        [&](const PointAttribute& other) {
                            return other.name == first->name;
                        })) {
            return false;
        }
    }
    return true;
}

bool PointRange::IsValid() const noexcept {
    return pointCount == 0 ||
           firstPoint <= (std::numeric_limits<std::uint64_t>::max)() -
                              pointCount;
}

bool PointReadOptions::IsValid() const noexcept {
    return chunkPointLimit != 0 && memoryBudgetBytes != 0 &&
           range.IsValid();
}

} // namespace usdpointcloud