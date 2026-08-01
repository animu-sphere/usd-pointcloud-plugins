#include "usdpointcloud/PointCloud.h"

#include <cassert>

namespace {

void TestAttributesAndChunks() {
    usdpointcloud::PointAttribute position{"position",
                                           usdpointcloud::PointAttributeType::Float64};
    assert(position.IsValid());
    const usdpointcloud::PointAttribute invalidAttribute{"", position.type};
    assert(!invalidAttribute.IsValid());

    usdpointcloud::PointChunk chunk;
    assert(chunk.IsValid());

    chunk.pointCount = 2;
    chunk.bounds.Expand({1.0, 2.0, 3.0});
    chunk.bounds.Expand({4.0, 5.0, 6.0});
    chunk.attributes = {position, {"classification",
                                   usdpointcloud::PointAttributeType::UInt8}};
    assert(chunk.IsValid());

    chunk.attributes.push_back(position);
    assert(!chunk.IsValid());
}

void TestInvalidChunk() {
    usdpointcloud::PointChunk chunk;
    chunk.pointCount = 1;
    assert(!chunk.IsValid());

    chunk.bounds.Expand({0.0, 0.0, 0.0});
    chunk.attributes = {{"", usdpointcloud::PointAttributeType::Float32}};
    assert(!chunk.IsValid());
}

} // namespace

int main() {
    TestAttributesAndChunks();
    TestInvalidChunk();
    return 0;
}