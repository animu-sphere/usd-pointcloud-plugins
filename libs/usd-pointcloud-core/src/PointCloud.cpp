#include "usdpointcloud/PointCloud.h"

#include <algorithm>

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

} // namespace usdpointcloud