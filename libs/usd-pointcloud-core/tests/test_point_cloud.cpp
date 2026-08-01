#include "usdpointcloud/PointCloud.h"
#include "usdpointcloud/FileFormatArguments.h"

#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAttributesAndChunks() {
    usdpointcloud::PointAttribute position{"position",
                                           usdpointcloud::PointAttributeType::Float64};
    Check(position.IsValid());
    const usdpointcloud::PointAttribute invalidAttribute{"", position.type};
    Check(!invalidAttribute.IsValid());

    usdpointcloud::PointChunk chunk;
    Check(chunk.IsValid());

    chunk.pointCount = 2;
    chunk.bounds.Expand({1.0, 2.0, 3.0});
    chunk.bounds.Expand({4.0, 5.0, 6.0});
    chunk.attributes = {position, {"classification",
                                   usdpointcloud::PointAttributeType::UInt8}};
    Check(chunk.IsValid());

    chunk.attributes.push_back(position);
    Check(!chunk.IsValid());
}

void TestInvalidChunk() {
    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 1;
    Check(!chunk.IsValid());

    chunk.bounds.Expand({0.0, 0.0, 0.0});
    chunk.attributes = {{"", usdpointcloud::PointAttributeType::Float32}};
    Check(!chunk.IsValid());
}

void TestPointDataAndAssetChunk() {
    usdpointcloud::PointData data;
    data.positions = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    data.intensity = {10, 20};
    data.classification = {2, 5};
    Check(data.IsValid());

    usdgeo::SpatialBounds bounds;
    bounds.Expand(data.positions[0]);
    bounds.Expand(data.positions[1]);
    const auto chunk = usdpointcloud::MakePointChunk(data, bounds);
    Check(chunk.IsValid() && chunk.pointCount == 2);
    Check(chunk.attributes.size() == 2);
    Check(chunk.attributes[0].name == "intensity");
    Check(chunk.attributes[1].name == "classification");

    data.red = {1};
    Check(!data.IsValid());
}

void TestReadOptions() {
    usdpointcloud::PointReadOptions options;
    Check(options.IsValid());

    options.range = {4, 3};
    Check(options.IsValid());
    options.range = {(std::numeric_limits<std::uint64_t>::max)(), 2};
    Check(!options.IsValid());
    options.range = {};
    options.memoryBudgetBytes = 0;
    Check(!options.IsValid());
}

void TestFileFormatArgumentNormalization() {
    std::map<std::string, std::string> arguments = {
        {"attributes", "rgb, xyz"},
        {"chunkPointLimit", "2"},
        {"rangeFirstPoint", "4"},
        {"rangePointCount", "3"}};
    usdpointcloud::PointReadRequest request;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(usdpointcloud::NormalizeFileFormatArguments(
        arguments, request, diagnostics));
    Check(diagnostics.empty());
    Check(request.readOptions.chunkPointLimit == 2);
    Check(request.readOptions.range.firstPoint == 4);
    Check(request.readOptions.range.pointCount == 3);
    Check(request.normalizedArguments ==
          "chunkPointLimit=2&rangeFirstPoint=4&rangePointCount=3&attributes=blue,green,red,xyz");
    Check(request.canonicalArguments ==
          std::map<std::string, std::string>{
              {"attributes", "blue,green,red,xyz"},
              {"chunkPointLimit", "2"},
              {"rangeFirstPoint", "4"},
              {"rangePointCount", "3"}});

    arguments = {{"chunkPointLimit", "065536"},
                 {"memoryBudgetBytes", "67108864"},
                 {"rangePointCount", "0"}};
    Check(usdpointcloud::NormalizeFileFormatArguments(
        arguments, request, diagnostics));
    Check(request.canonicalArguments.empty());

    std::map<std::string, std::string> encoded;
    std::string error;
    Check(usdpointcloud::ParseFileFormatArgumentString(
        "rangePointCount=3&rangeFirstPoint=4", encoded, error));
    Check(encoded["rangeFirstPoint"] == "4");

    arguments = {{"unknown", "value"}};
    Check(!usdpointcloud::NormalizeFileFormatArguments(
        arguments, request, diagnostics));
    Check(diagnostics.size() == 1 &&
          diagnostics.front().code == usdgeo::DiagnosticCode::UnknownFormatArgument);
}

void TestAttributeSelection() {
    usdpointcloud::PointData data;
    data.positions = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    data.intensity = {10, 20};
    data.red = {1, 2};
    data.green = {3, 4};
    data.blue = {5, 6};
    std::string error;
    Check(usdpointcloud::SelectPointDataAttributes(
        data, {"blue", "green", "red", "xyz"}, error));
    Check(data.positions.size() == 2 && data.intensity.empty() &&
          data.red.size() == 2 && data.green.size() == 2 &&
          data.blue.size() == 2);
}

} // namespace

int main() {
    TestAttributesAndChunks();
    TestInvalidChunk();
    TestPointDataAndAssetChunk();
    TestReadOptions();
    TestFileFormatArgumentNormalization();
    TestAttributeSelection();
    return 0;
}