#include "usdpointcloud/Lod.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace usdpointcloud {
namespace {

bool SameBounds(const usdgeo::SpatialBounds& left,
                const usdgeo::SpatialBounds& right) noexcept {
    return left.minimum.x == right.minimum.x &&
           left.minimum.y == right.minimum.y &&
           left.minimum.z == right.minimum.z &&
           left.maximum.x == right.maximum.x &&
           left.maximum.y == right.maximum.y &&
           left.maximum.z == right.maximum.z;
}

    bool ContainsBounds(const usdgeo::SpatialBounds& outer,
                  const usdgeo::SpatialBounds& inner) noexcept {
        return outer.minimum.x <= inner.minimum.x &&
            outer.minimum.y <= inner.minimum.y &&
            outer.minimum.z <= inner.minimum.z &&
            outer.maximum.x >= inner.maximum.x &&
            outer.maximum.y >= inner.maximum.y &&
            outer.maximum.z >= inner.maximum.z;
    }

bool SameTileId(const PointTileId& left,
                const PointTileId& right) noexcept {
    return left.level == right.level && left.x == right.x &&
           left.y == right.y && left.z == right.z;
}

void AddDiagnostic(usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, std::nullopt, std::nullopt});
}

bool ValidThresholds(const std::vector<float>& thresholds) noexcept {
    for (std::size_t index = 0; index < thresholds.size(); ++index) {
        const float threshold = thresholds[index];
        if (!std::isfinite(threshold) || threshold <= 0.0F ||
            (index != 0 && threshold >= thresholds[index - 1])) {
            return false;
        }
    }
    return true;
}

} // namespace

bool PointLodItem::IsValid() const noexcept {
    return pointCount != 0 && bounds.IsValid() && sourceRange.IsValid();
}

bool PointLodHierarchy::IsValid() const noexcept {
    if (!bounds.IsValid() || items.empty() || defaultIndex >= items.size() ||
        screenSizeThresholds.size() + 1 != items.size() ||
        !ValidThresholds(screenSizeThresholds)) {
        return false;
    }

    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        if (!item.IsValid() || item.index != index ||
            !ContainsBounds(bounds, item.bounds) ||
            (index != 0 && item.pointCount > items[index - 1].pointCount)) {
            return false;
        }
    }
    return true;
}

bool PointTile::IsValid() const noexcept {
    if (!id.IsValid() || !bounds.IsValid() || !lod.IsValid() ||
        !SameBounds(bounds, lod.bounds)) {
        return false;
    }

    for (std::size_t index = 0; index < children.size(); ++index) {
        const auto& child = children[index];
        if (!child.IsValid()) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (SameTileId(child, children[previous])) {
                return false;
            }
        }
    }
    return true;
}

bool ValidatePointLodHierarchy(
    const PointLodHierarchy& hierarchy,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    if (!hierarchy.bounds.IsValid()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                      "LOD hierarchy bounds are invalid", diagnostics);
    }
    if (hierarchy.items.empty()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                      "LOD hierarchy must contain at least one item",
                      diagnostics);
    }
    if (hierarchy.defaultIndex >= hierarchy.items.size()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                      "LOD hierarchy default index is out of range",
                      diagnostics);
    }
    if (hierarchy.screenSizeThresholds.size() + 1 != hierarchy.items.size()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                      "LOD threshold count must equal item count minus one",
                      diagnostics);
    }
    if (!ValidThresholds(hierarchy.screenSizeThresholds)) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                      "LOD thresholds must be finite, positive, and descending",
                      diagnostics);
    }

    for (std::size_t index = 0; index < hierarchy.items.size(); ++index) {
        const auto& item = hierarchy.items[index];
        if (!item.sourceRange.IsValid()) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointSourceRange,
                          "LOD item source range is invalid", diagnostics);
        }
        if (!item.IsValid()) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodItem,
                          "LOD item is invalid at index " +
                              std::to_string(index),
                          diagnostics);
        } else if (item.index != index) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodItem,
                          "LOD item index does not match its position",
                          diagnostics);
        } else if (!ContainsBounds(hierarchy.bounds, item.bounds)) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodItem,
                          "LOD item bounds are outside the hierarchy bounds",
                          diagnostics);
        }
        if (index != 0 && item.pointCount > hierarchy.items[index - 1].pointCount) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidLodHierarchy,
                          "LOD point counts must not increase at lower detail",
                          diagnostics);
        }
    }
    return diagnostics.empty();
}

bool ValidatePointTile(const PointTile& tile,
                       std::vector<usdgeo::Diagnostic>& diagnostics) {
    const bool hierarchyValid =
        ValidatePointLodHierarchy(tile.lod, diagnostics);
    if (!tile.id.IsValid()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointTileId,
                      "point tile ID is invalid", diagnostics);
    }
    if (!tile.bounds.IsValid()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointTile,
                      "point tile bounds are invalid", diagnostics);
    }
    if (!SameBounds(tile.bounds, tile.lod.bounds)) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointTile,
                      "point tile bounds do not match the LOD hierarchy bounds",
                      diagnostics);
    }
    for (std::size_t index = 0; index < tile.children.size(); ++index) {
        const auto& child = tile.children[index];
        if (!child.IsValid()) {
            AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointTileId,
                          "point tile child ID is invalid", diagnostics);
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (SameTileId(child, tile.children[previous])) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidPointTile,
                              "point tile child IDs must be unique", diagnostics);
                break;
            }
        }
    }
    return hierarchyValid && diagnostics.empty();
}

} // namespace usdpointcloud