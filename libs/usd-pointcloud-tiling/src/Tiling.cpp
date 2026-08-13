#include "usdpointcloud/Tiling.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>

namespace usdpointcloud {
namespace {

void AddError(std::vector<usdgeo::Diagnostic>& diagnostics,
              usdgeo::DiagnosticCode code,
              const char* message) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message});
}

bool ToTileCoordinate(double coordinate,
                      double tileSize,
                      std::int64_t& result) noexcept {
    const long double index = std::floor(static_cast<long double>(coordinate) /
                                         static_cast<long double>(tileSize));
    // Use the exclusive power-of-two upper bound because INT64_MAX can round
    // to 2^63 when long double has double precision, as it does on MSVC.
    constexpr long double minimum = -9223372036854775808.0L;
    constexpr long double maximumExclusive = 9223372036854775808.0L;
    if (index < minimum || index >= maximumExclusive) {
        return false;
    }

    result = static_cast<std::int64_t>(index);
    return true;
}

std::uint64_t EncodeMortonPrefix(std::int64_t x,
                                 std::int64_t y,
                                 std::int32_t depth) noexcept {
    std::uint64_t code = 0;
    for (std::int32_t bit = depth - 1; bit >= 0; --bit) {
        code = (code << 2) |
               static_cast<std::uint64_t>((x >> bit) & 1) |
               (static_cast<std::uint64_t>((y >> bit) & 1) << 1);
    }
    return code;
}

bool TileIdLess(const PointTileId& left, const PointTileId& right) noexcept {
    if (left.level != right.level) return left.level < right.level;
    if (left.x != right.x) return left.x < right.x;
    if (left.y != right.y) return left.y < right.y;
    return left.z < right.z;
}

bool TileIdEqual(const PointTileId& left, const PointTileId& right) noexcept {
    return left.level == right.level && left.x == right.x &&
           left.y == right.y && left.z == right.z;
}

bool IsSafeManifestPath(const std::string& value) {
    if (value.empty() || value.find('\\') != std::string::npos ||
        value.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos ||
        value.find('=') != std::string::npos) {
        return false;
    }
    if (value.front() == '/' ||
        (value.size() >= 2 &&
         ((value.front() >= 'A' && value.front() <= 'Z') ||
          (value.front() >= 'a' && value.front() <= 'z')) &&
         value[1] == ':')) {
        return false;
    }
    std::size_t componentStart = 0;
    while (componentStart < value.size()) {
        const auto separator = value.find('/', componentStart);
        const auto componentLength = separator == std::string::npos
                                         ? value.size() - componentStart
                                         : separator - componentStart;
        if ((componentLength == 1 && value[componentStart] == '.') ||
            (componentLength == 2 &&
             value.compare(componentStart, 2, "..") == 0)) {
            return false;
        }
        if (separator == std::string::npos) break;
        componentStart = separator + 1;
    }
    return true;
}

bool ManifestEntryLess(const PointTileManifestEntry& left,
                       const PointTileManifestEntry& right) noexcept {
    if (TileIdLess(left.id, right.id)) return true;
    if (TileIdLess(right.id, left.id)) return false;
    if (left.lod != right.lod) return left.lod < right.lod;
    return left.payloadPath < right.payloadPath;
}

} // namespace

bool TileGridConfig::IsValid() const noexcept {
    return level >= 0 && std::isfinite(tileSize) && tileSize > 0.0;
}

bool ValidateTileGridConfig(const TileGridConfig& config,
                            std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!std::isfinite(config.tileSize) || config.tileSize <= 0.0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "tile size must be finite and greater than zero");
    }
    if (config.level < 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTileId,
                 "tile level must not be negative");
    }
    return config.IsValid();
}

bool PointBudgetConfig::IsValid() const noexcept {
    return maxPointsPerTile > 0 && minPointsPerTile <= maxPointsPerTile &&
           maxDepth >= 0 && maxDepth <= kMaxPointBudgetDepth;
}

bool PointBudgetTile::IsValid() const noexcept {
    return id.IsValid() && std::isfinite(minX) && std::isfinite(maxX) &&
           std::isfinite(minY) && std::isfinite(maxY) && minX <= maxX &&
           minY <= maxY && pointCount > 0;
}

bool TilePlanSourceRange::IsValid() const noexcept {
    return length > 0 && offset <=
        (std::numeric_limits<std::uint64_t>::max)() - length;
}

bool TilePlanNode::IsValid() const noexcept {
    if (!id.IsValid() || !bounds.IsValid() || pointCount == 0) return false;
    if (isLeaf && !children.empty()) return false;
    return std::all_of(sourceRanges.begin(), sourceRanges.end(),
                       [](const TilePlanSourceRange& range) {
                           return range.IsValid();
                       });
}

bool TilePlan::IsValid() const noexcept {
    return !plannerId.empty() && plannerVersion > 0 &&
           std::all_of(nodes.begin(), nodes.end(),
                       [](const TilePlanNode& node) { return node.IsValid(); });
}

bool PointTileManifestEntry::IsValid() const noexcept {
    return id.IsValid() && bounds.IsValid() && pointCount > 0 &&
           IsSafeManifestPath(payloadPath);
}

bool ValidatePointTileManifest(
    const PointTileManifest& manifest,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    for (const auto& entry : manifest.entries) {
        if (!entry.IsValid()) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "tile manifest contains an invalid entry");
            return false;
        }
    }

    std::vector<PointTileManifestEntry> sorted = manifest.entries;
    std::sort(sorted.begin(), sorted.end(), ManifestEntryLess);
    for (std::size_t index = 1; index < sorted.size(); ++index) {
        const auto& previous = sorted[index - 1];
        const auto& current = sorted[index];
        if (TileIdEqual(previous.id, current.id) &&
            previous.lod == current.lod) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "tile manifest contains duplicate tile LOD entries");
            return false;
        }
    }
    return true;
}

bool SerializePointTileManifest(
    const PointTileManifest& manifest,
    std::string& serialized,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    serialized.clear();
    if (!ValidatePointTileManifest(manifest, diagnostics)) return false;

    std::vector<PointTileManifestEntry> sorted = manifest.entries;
    std::sort(sorted.begin(), sorted.end(), ManifestEntryLess);
    std::ostringstream output;
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "format=" << kPointTileManifestFormat << '\n'
           << "tile.count=" << sorted.size() << '\n';
    for (std::size_t index = 0; index < sorted.size(); ++index) {
        const auto& entry = sorted[index];
        const auto prefix = "tile." + std::to_string(index) + ".";
        output << prefix << "id=" << entry.id.ToString() << '\n'
               << prefix << "lod=" << entry.lod << '\n'
               << prefix << "pointCount=" << entry.pointCount << '\n'
               << prefix << "bounds.min=" << entry.bounds.minimum.x << ','
               << entry.bounds.minimum.y << ',' << entry.bounds.minimum.z
               << '\n'
               << prefix << "bounds.max=" << entry.bounds.maximum.x << ','
               << entry.bounds.maximum.y << ',' << entry.bounds.maximum.z
               << '\n'
               << prefix << "payload=" << entry.payloadPath << '\n';
    }
    if (!output) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "unable to serialize tile manifest");
        return false;
    }
    serialized = output.str();
    return true;
}

bool ValidatePointBudgetConfig(
    const PointBudgetConfig& config,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (config.maxPointsPerTile == 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "maximum points per tile must be greater than zero");
    }
    if (config.minPointsPerTile > config.maxPointsPerTile) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "minimum points per tile must not exceed the maximum");
    }
    if (config.maxDepth < 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTileId,
                 "maximum tile depth must not be negative");
    }
    if (config.maxDepth > kMaxPointBudgetDepth) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTileId,
                 "maximum tile depth exceeds the supported planner depth");
    }
    return config.IsValid();
}

bool ValidateTilePlan(
    const TilePlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (plan.plannerId.empty() || plan.plannerId.find('\n') != std::string::npos ||
        plan.plannerId.find('\r') != std::string::npos) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "tile plan requires a non-empty single-line planner identity");
        return false;
    }
    if (plan.plannerVersion == 0) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "tile plan planner version must be greater than zero");
        return false;
    }
    if (!plan.IsValid()) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "tile plan contains an invalid node");
        return false;
    }

    for (std::size_t index = 0; index < plan.nodes.size(); ++index) {
        const auto& node = plan.nodes[index];
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (TileIdEqual(plan.nodes[previous].id, node.id)) {
                AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                         "tile plan contains duplicate node identities");
                return false;
            }
        }
        if (node.parent.level >= 0) {
            const auto parent = std::find_if(
                plan.nodes.begin(), plan.nodes.end(),
                [&](const TilePlanNode& candidate) {
                    return TileIdEqual(candidate.id, node.parent);
                });
            if (parent == plan.nodes.end() ||
                std::find_if(parent->children.begin(), parent->children.end(),
                             [&](const PointTileId& child) {
                                 return TileIdEqual(child, node.id);
                             }) == parent->children.end()) {
                AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                         "tile plan node has an inconsistent parent relationship");
                return false;
            }
        }
        for (const auto& child : node.children) {
            const auto childNode = std::find_if(
                plan.nodes.begin(), plan.nodes.end(),
                [&](const TilePlanNode& candidate) {
                    return TileIdEqual(candidate.id, child);
                });
            if (childNode == plan.nodes.end() ||
                !TileIdEqual(childNode->parent, node.id)) {
                AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                         "tile plan node has an inconsistent child relationship");
                return false;
            }
        }
        if (!node.isLeaf && node.children.empty()) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "tile plan contains an internal node without children");
            return false;
        }
    }
    return true;
}

bool BuildTilePlan(
    const PointBudgetPlan& budgetPlan,
    TilePlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    plan.plannerId = "adaptive-point-budget";
    plan.plannerVersion = 1;
    std::map<std::string, std::size_t> nodeIndices;

    const auto addNode = [&](const PointTileId& id) -> TilePlanNode& {
        const auto key = id.ToString();
        const auto found = nodeIndices.find(key);
        if (found != nodeIndices.end()) return plan.nodes[found->second];
        nodeIndices.emplace(key, plan.nodes.size());
        plan.nodes.push_back({id, usdgeo::SpatialBounds::Empty(), 0,
                              {-1, 0, 0, 0}, {}, {}, false});
        return plan.nodes.back();
    };
    const auto addChild = [](TilePlanNode& parent,
                             const PointTileId& child) {
        const auto found = std::find_if(
            parent.children.begin(), parent.children.end(),
            [&](const PointTileId& value) { return TileIdEqual(value, child); });
        if (found == parent.children.end()) parent.children.push_back(child);
    };

    for (const auto& leaf : budgetPlan.tiles) {
        if (!leaf.IsValid()) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "cannot build a tile plan from an invalid adaptive tile");
            plan = {};
            return false;
        }
        std::vector<PointTileId> path;
        auto current = leaf.id;
        for (;;) {
            path.push_back(current);
            if (current.level == 0) break;
            current = {current.level - 1, current.x / 2, current.y / 2,
                       current.z / 2};
        }
        std::reverse(path.begin(), path.end());
        for (std::size_t index = 0; index < path.size(); ++index) {
            auto& node = addNode(path[index]);
            node.pointCount += leaf.pointCount;
            node.bounds.Expand({leaf.minX, leaf.minY, 0.0});
            node.bounds.Expand({leaf.maxX, leaf.maxY, 0.0});
            if (index > 0) {
                auto& parent = addNode(path[index - 1]);
                node.parent = parent.id;
                addChild(parent, node.id);
            }
        }
        auto& leafNode = addNode(leaf.id);
        leafNode.isLeaf = true;
    }

    std::sort(plan.nodes.begin(), plan.nodes.end(),
              [](const TilePlanNode& left, const TilePlanNode& right) {
                  return TileIdLess(left.id, right.id);
              });
    if (!ValidateTilePlan(plan, diagnostics)) {
        plan = {};
        return false;
    }
    return true;
}

bool BuildPointBudgetPlan(
    const std::vector<usdgeo::Vec3d>& sourcePositions,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    if (!ValidatePointBudgetConfig(config, diagnostics)) return false;

    if (sourcePositions.empty()) return true;
    plan.pointCount = sourcePositions.size();
    for (const auto& position : sourcePositions) {
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
            AddError(diagnostics, usdgeo::DiagnosticCode::NonFiniteCoordinate,
                     "point-budget planning requires finite horizontal coordinates");
            return false;
        }
    }

    struct Bounds {
        double minX;
        double maxX;
        double minY;
        double maxY;
    };
    const auto makeBounds = [&](const std::vector<std::size_t>& indices) {
        Bounds bounds{sourcePositions[indices.front()].x,
                      sourcePositions[indices.front()].x,
                      sourcePositions[indices.front()].y,
                      sourcePositions[indices.front()].y};
        for (const auto index : indices) {
            const auto& position = sourcePositions[index];
            bounds.minX = std::min(bounds.minX, position.x);
            bounds.maxX = std::max(bounds.maxX, position.x);
            bounds.minY = std::min(bounds.minY, position.y);
            bounds.maxY = std::max(bounds.maxY, position.y);
        }
        return bounds;
    };

    bool budgetSatisfied = true;
    const auto recordLeaf = [&](const std::vector<std::size_t>& indices,
                                const Bounds& bounds,
                                const PointTileId& id) {
        ++plan.tileCount;
        if (plan.minimumPointsPerTile == 0) {
            plan.minimumPointsPerTile = indices.size();
        } else {
            plan.minimumPointsPerTile =
                std::min(plan.minimumPointsPerTile, indices.size());
        }
        plan.maximumPointsPerTile =
            std::max(plan.maximumPointsPerTile, indices.size());
        plan.tiles.push_back({id, bounds.minX, bounds.maxX, bounds.minY,
                              bounds.maxY, indices.size()});
    };
    const std::function<void(const std::vector<std::size_t>&,
                             std::int32_t, PointTileId&, Bounds)>
        visit = [&](const std::vector<std::size_t>& indices,
                    std::int32_t depth, PointTileId& tileId,
                    Bounds bounds) {
            plan.depth = std::max(plan.depth, depth);
            const bool canSplit = indices.size() > config.maxPointsPerTile &&
                                  depth < config.maxDepth &&
                                  bounds.minX != bounds.maxX &&
                                  bounds.minY != bounds.maxY;
            if (!canSplit) {
                recordLeaf(indices, bounds, tileId);
                if (indices.size() > config.maxPointsPerTile) {
                    budgetSatisfied = false;
                }
                return;
            }

            const double splitX = bounds.minX +
                                  (bounds.maxX - bounds.minX) * 0.5;
            const double splitY = bounds.minY +
                                  (bounds.maxY - bounds.minY) * 0.5;
            std::array<std::vector<std::size_t>, 4> children;
            for (const auto index : indices) {
                const auto& position = sourcePositions[index];
                const std::size_t child = (position.x >= splitX ? 1 : 0) +
                                          (position.y >= splitY ? 2 : 0);
                children[child].push_back(index);
            }
            std::size_t nonEmptyChildren = 0;
            bool childrenMeetMinimum = true;
            for (const auto& child : children) {
                if (!child.empty()) {
                    ++nonEmptyChildren;
                    childrenMeetMinimum =
                        childrenMeetMinimum &&
                        child.size() >= config.minPointsPerTile;
                }
            }
            if (nonEmptyChildren < 2 || !childrenMeetMinimum) {
                recordLeaf(indices, bounds, tileId);
                budgetSatisfied = false;
                return;
            }
            ++plan.splitCount;
            for (std::size_t childIndex = 0; childIndex < children.size();
                 ++childIndex) {
                if (children[childIndex].empty()) continue;
                Bounds childBounds = bounds;
                if ((childIndex & 1) != 0) {
                    childBounds.minX = splitX;
                } else {
                    childBounds.maxX = splitX;
                }
                if ((childIndex & 2) != 0) {
                    childBounds.minY = splitY;
                } else {
                    childBounds.maxY = splitY;
                }
                PointTileId childId{
                    depth + 1,
                    tileId.x * 2 + static_cast<std::int64_t>(childIndex & 1),
                    tileId.y * 2 + static_cast<std::int64_t>(childIndex >> 1),
                    0};
                visit(children[childIndex], depth + 1, childId,
                      childBounds);
            }
        };

    std::vector<std::size_t> indices(sourcePositions.size());
    std::iota(indices.begin(), indices.end(), 0);
    PointTileId rootId{0, 0, 0, 0};
    const auto rootBounds = makeBounds(indices);
    visit(indices, 0, rootId, rootBounds);
    if (plan.tileCount > 0) {
        plan.averagePointsPerTile =
            static_cast<double>(plan.pointCount) /
            static_cast<double>(plan.tileCount);
    }
    if (!budgetSatisfied) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "point budget cannot be satisfied for the source distribution");
    }
    return budgetSatisfied;
}

bool BuildPointBudgetPlan(
    const PointStreamFactory& streamFactory,
    const PointBudgetConfig& config,
    PointBudgetPlan& plan,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    plan = {};
    if (!ValidatePointBudgetConfig(config, diagnostics)) return false;
    if (!streamFactory) {
        AddError(diagnostics, usdgeo::DiagnosticCode::SourceOpenFailed,
                 "point-budget planning requires a stream factory");
        return false;
    }
    struct Bounds {
        double minX = 0.0;
        double maxX = 0.0;
        double minY = 0.0;
        double maxY = 0.0;
    };
    Bounds rootBounds;
    bool hasBounds = false;
    std::size_t pointCount = 0;
    auto firstPass = streamFactory();
    if (!firstPass) {
        AddError(diagnostics, usdgeo::DiagnosticCode::SourceOpenFailed,
                 "unable to open point stream for budget planning");
        return false;
    }
    for (;;) {
        PointChunk chunk;
        PointData data;
        usdgeo::Diagnostic diagnostic;
        const auto status = firstPass->ReadNext(chunk, data, diagnostic);
        if (status == PointStreamStatus::End) break;
        if (status == PointStreamStatus::Error || !chunk.IsValid() ||
            !data.IsValid() || chunk.pointCount != data.positions.size()) {
            if (diagnostic.message.empty()) {
                AddError(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                         "point stream failed during budget planning");
            } else {
                diagnostics.push_back(diagnostic);
            }
            return false;
        }
        if (pointCount > (std::numeric_limits<std::size_t>::max)() -
                             data.positions.size()) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "point count exceeds the planner range");
            return false;
        }
        pointCount += data.positions.size();
        for (const auto& position : data.positions) {
            if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
                AddError(diagnostics, usdgeo::DiagnosticCode::NonFiniteCoordinate,
                         "point-budget planning requires finite horizontal coordinates");
                return false;
            }
            if (!hasBounds) {
                rootBounds = {position.x, position.x, position.y, position.y};
                hasBounds = true;
            } else {
                rootBounds.minX = std::min(rootBounds.minX, position.x);
                rootBounds.maxX = std::max(rootBounds.maxX, position.x);
                rootBounds.minY = std::min(rootBounds.minY, position.y);
                rootBounds.maxY = std::max(rootBounds.maxY, position.y);
            }
        }
    }
    if (!hasBounds) return true;

    std::map<std::uint64_t, std::size_t> terminalCounts;
    auto secondPass = streamFactory();
    if (!secondPass) {
        AddError(diagnostics, usdgeo::DiagnosticCode::SourceOpenFailed,
                 "unable to reopen point stream for budget planning");
        return false;
    }
    std::size_t secondPassPointCount = 0;
    for (;;) {
        PointChunk chunk;
        PointData data;
        usdgeo::Diagnostic diagnostic;
        const auto status = secondPass->ReadNext(chunk, data, diagnostic);
        if (status == PointStreamStatus::End) break;
        if (status == PointStreamStatus::Error || !chunk.IsValid() ||
            !data.IsValid() || chunk.pointCount != data.positions.size()) {
            if (diagnostic.message.empty()) {
                AddError(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                         "point stream failed during budget planning");
            } else {
                diagnostics.push_back(diagnostic);
            }
            return false;
        }
        if (secondPassPointCount >
            (std::numeric_limits<std::size_t>::max)() -
                data.positions.size()) {
            AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                     "point count exceeds the planner range");
            return false;
        }
        secondPassPointCount += data.positions.size();
        for (const auto& position : data.positions) {
            Bounds bounds = rootBounds;
            std::int64_t x = 0;
            std::int64_t y = 0;
            for (std::int32_t depth = 0; depth <= config.maxDepth;
                 ++depth) {
                if (depth == config.maxDepth) break;
                const double splitX = bounds.minX +
                                      (bounds.maxX - bounds.minX) * 0.5;
                const double splitY = bounds.minY +
                                      (bounds.maxY - bounds.minY) * 0.5;
                const auto xBit = position.x >= splitX ? 1 : 0;
                const auto yBit = position.y >= splitY ? 1 : 0;
                x = x * 2 + xBit;
                y = y * 2 + yBit;
                if (xBit != 0) {
                    bounds.minX = splitX;
                } else {
                    bounds.maxX = splitX;
                }
                if (yBit != 0) {
                    bounds.minY = splitY;
                } else {
                    bounds.maxY = splitY;
                }
            }
            ++terminalCounts[
                EncodeMortonPrefix(x, y, config.maxDepth)];
        }
    }
    if (secondPassPointCount != pointCount) {
        AddError(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                 "point stream changed between budget planning passes");
        return false;
    }

    std::vector<std::uint64_t> terminalCodes;
    std::vector<std::size_t> terminalPrefixCounts(1, 0);
    terminalCodes.reserve(terminalCounts.size());
    for (const auto& [code, count] : terminalCounts) {
        terminalCodes.push_back(code);
        terminalPrefixCounts.push_back(terminalPrefixCounts.back() + count);
    }

    plan.pointCount = pointCount;
    bool budgetSatisfied = true;
    const auto countFor = [&](const PointTileId& id) {
        const auto remaining = config.maxDepth - id.level;
        const auto prefix = EncodeMortonPrefix(id.x, id.y, id.level);
        const auto firstCode = prefix << (remaining * 2);
        const auto lastCode = (prefix + 1) << (remaining * 2);
        const auto first = std::lower_bound(
            terminalCodes.begin(), terminalCodes.end(), firstCode);
        const auto last = std::lower_bound(
            terminalCodes.begin(), terminalCodes.end(), lastCode);
        const auto firstIndex =
            static_cast<std::size_t>(first - terminalCodes.begin());
        const auto lastIndex =
            static_cast<std::size_t>(last - terminalCodes.begin());
        return terminalPrefixCounts[lastIndex] -
               terminalPrefixCounts[firstIndex];
    };
    const auto recordLeaf = [&](std::size_t count,
                                const Bounds& bounds,
                                const PointTileId& id) {
        ++plan.tileCount;
        if (plan.minimumPointsPerTile == 0) {
            plan.minimumPointsPerTile = count;
        } else {
            plan.minimumPointsPerTile =
                std::min(plan.minimumPointsPerTile, count);
        }
        plan.maximumPointsPerTile = std::max(plan.maximumPointsPerTile, count);
        plan.tiles.push_back({id, bounds.minX, bounds.maxX, bounds.minY,
                              bounds.maxY, count});
    };
    const std::function<void(PointTileId, Bounds)> visit =
        [&](PointTileId id, Bounds bounds) {
            const auto count = countFor(id);
            plan.depth = std::max(plan.depth, id.level);
            const bool canSplit = count > config.maxPointsPerTile &&
                                  id.level < config.maxDepth &&
                                  bounds.minX != bounds.maxX &&
                                  bounds.minY != bounds.maxY;
            if (!canSplit) {
                recordLeaf(count, bounds, id);
                if (count > config.maxPointsPerTile) budgetSatisfied = false;
                return;
            }

            const double splitX = bounds.minX +
                                  (bounds.maxX - bounds.minX) * 0.5;
            const double splitY = bounds.minY +
                                  (bounds.maxY - bounds.minY) * 0.5;
            std::array<std::size_t, 4> childCounts{};
            for (std::size_t childIndex = 0; childIndex < childCounts.size();
                 ++childIndex) {
                const PointTileId childId{
                    id.level + 1,
                    id.x * 2 + static_cast<std::int64_t>(childIndex & 1),
                    id.y * 2 + static_cast<std::int64_t>(childIndex >> 1),
                    0};
                childCounts[childIndex] = countFor(childId);
            }
            std::size_t nonEmptyChildren = 0;
            bool childrenMeetMinimum = true;
            for (const auto childCount : childCounts) {
                if (childCount == 0) continue;
                ++nonEmptyChildren;
                childrenMeetMinimum =
                    childrenMeetMinimum && childCount >= config.minPointsPerTile;
            }
            if (nonEmptyChildren < 2 || !childrenMeetMinimum) {
                recordLeaf(count, bounds, id);
                budgetSatisfied = false;
                return;
            }
            ++plan.splitCount;
            for (std::size_t childIndex = 0; childIndex < childCounts.size();
                 ++childIndex) {
                if (childCounts[childIndex] == 0) continue;
                Bounds childBounds = bounds;
                if ((childIndex & 1) != 0) {
                    childBounds.minX = splitX;
                } else {
                    childBounds.maxX = splitX;
                }
                if ((childIndex & 2) != 0) {
                    childBounds.minY = splitY;
                } else {
                    childBounds.maxY = splitY;
                }
                visit({id.level + 1,
                       id.x * 2 + static_cast<std::int64_t>(childIndex & 1),
                       id.y * 2 + static_cast<std::int64_t>(childIndex >> 1),
                       0},
                     childBounds);
            }
        };
    visit({0, 0, 0, 0}, rootBounds);
    if (plan.tileCount > 0) {
        plan.averagePointsPerTile =
            static_cast<double>(plan.pointCount) /
            static_cast<double>(plan.tileCount);
    }
    if (!budgetSatisfied) {
        AddError(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                 "point budget cannot be satisfied for the source distribution");
    }
    return budgetSatisfied;
}

FixedGridTileRouter::FixedGridTileRouter(TileGridConfig config) noexcept
    : config_(config) {}

PointTileId FixedGridTileRouter::GetTileId(
    const usdgeo::Vec3d& sourcePosition) const noexcept {
    PointTileId tile;
    tile.level = config_.level;
    if (!IsValid() || !std::isfinite(sourcePosition.x) ||
        !std::isfinite(sourcePosition.y) ||
        !ToTileCoordinate(sourcePosition.x, config_.tileSize, tile.x) ||
        !ToTileCoordinate(sourcePosition.y, config_.tileSize, tile.y)) {
        tile.level = -1;
    }
    return tile;
}

bool FixedGridTileRouter::IsValid() const noexcept {
    return config_.IsValid();
}

PointBudgetTileRouter::PointBudgetTileRouter(const PointBudgetPlan& plan)
    : tiles_(plan.tiles) {
    std::sort(tiles_.begin(), tiles_.end(),
              [](const PointBudgetTile& left, const PointBudgetTile& right) {
                  return TileIdLess(left.id, right.id);
              });
    if (!tiles_.empty()) {
        rootMinX_ = tiles_.front().minX;
        rootMaxX_ = tiles_.front().maxX;
        rootMinY_ = tiles_.front().minY;
        rootMaxY_ = tiles_.front().maxY;
        for (const auto& tile : tiles_) {
            rootMinX_ = std::min(rootMinX_, tile.minX);
            rootMaxX_ = std::max(rootMaxX_, tile.maxX);
            rootMinY_ = std::min(rootMinY_, tile.minY);
            rootMaxY_ = std::max(rootMaxY_, tile.maxY);
        }
    }
}

PointBudgetTileRouter::PointBudgetTileRouter(const TilePlan& plan) {
    for (const auto& node : plan.nodes) {
        if (!node.isLeaf || !node.IsValid() ||
            node.pointCount > (std::numeric_limits<std::size_t>::max)()) {
            continue;
        }
        tiles_.push_back({node.id,
                          node.bounds.minimum.x,
                          node.bounds.maximum.x,
                          node.bounds.minimum.y,
                          node.bounds.maximum.y,
                          static_cast<std::size_t>(node.pointCount)});
    }
    std::sort(tiles_.begin(), tiles_.end(),
              [](const PointBudgetTile& left, const PointBudgetTile& right) {
                  return TileIdLess(left.id, right.id);
              });
    if (!tiles_.empty()) {
        rootMinX_ = tiles_.front().minX;
        rootMaxX_ = tiles_.front().maxX;
        rootMinY_ = tiles_.front().minY;
        rootMaxY_ = tiles_.front().maxY;
        for (const auto& tile : tiles_) {
            rootMinX_ = std::min(rootMinX_, tile.minX);
            rootMaxX_ = std::max(rootMaxX_, tile.maxX);
            rootMinY_ = std::min(rootMinY_, tile.minY);
            rootMaxY_ = std::max(rootMaxY_, tile.maxY);
        }
    }
}

PointTileId PointBudgetTileRouter::GetTileId(
    const usdgeo::Vec3d& sourcePosition) const noexcept {
    if (!IsValid() || !std::isfinite(sourcePosition.x) ||
        !std::isfinite(sourcePosition.y)) {
        return {-1, 0, 0, 0};
    }

    if (sourcePosition.x < rootMinX_ || sourcePosition.x > rootMaxX_ ||
        sourcePosition.y < rootMinY_ || sourcePosition.y > rootMaxY_) {
        return {-1, 0, 0, 0};
    }

    const auto findTile = [&](const PointTileId& id) {
        const auto found = std::lower_bound(
            tiles_.begin(), tiles_.end(), id,
            [](const PointBudgetTile& tile, const PointTileId& value) {
                return TileIdLess(tile.id, value);
            });
        if (found == tiles_.end() || !TileIdEqual(found->id, id)) {
            return static_cast<const PointBudgetTile*>(nullptr);
        }
        return &*found;
    };

    double minX = rootMinX_;
    double maxX = rootMaxX_;
    double minY = rootMinY_;
    double maxY = rootMaxY_;
    PointTileId id{0, 0, 0, 0};
    for (;;) {
        if (findTile(id)) return id;
        if (id.level >= kMaxPointBudgetDepth || minX == maxX ||
            minY == maxY) {
            return {-1, 0, 0, 0};
        }
        const double splitX = minX + (maxX - minX) * 0.5;
        const double splitY = minY + (maxY - minY) * 0.5;
        const auto xBit = sourcePosition.x >= splitX ? 1 : 0;
        const auto yBit = sourcePosition.y >= splitY ? 1 : 0;
        id = {id.level + 1, id.x * 2 + xBit, id.y * 2 + yBit, 0};
        if (xBit != 0) {
            minX = splitX;
        } else {
            maxX = splitX;
        }
        if (yBit != 0) {
            minY = splitY;
        } else {
            maxY = splitY;
        }
    }
}

bool PointBudgetTileRouter::IsValid() const noexcept {
    return !tiles_.empty() && std::isfinite(rootMinX_) &&
           std::isfinite(rootMaxX_) && std::isfinite(rootMinY_) &&
           std::isfinite(rootMaxY_) && rootMinX_ <= rootMaxX_ &&
           rootMinY_ <= rootMaxY_ &&
           std::all_of(tiles_.begin(), tiles_.end(),
                       [](const PointBudgetTile& tile) {
                           return tile.IsValid();
                       });
}

} // namespace usdpointcloud
