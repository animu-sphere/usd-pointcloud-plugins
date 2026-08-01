#include "usdpointcloud/PointCloud.h"

#include <cstdlib>

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

} // namespace

int main() {
    TestAttributesAndChunks();
    TestInvalidChunk();
    return 0;
}