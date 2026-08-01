#include "usdgeo/CacheKey.h"
#include "usdgeo/GeoReference.h"
#include "usdgeo/SpatialBounds.h"
#include "usdgeo/TileId.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool NearlyEqual(double left, double right) {
    return std::abs(left - right) < 1e-12;
}

void TestBounds() {
    auto bounds = usdgeo::SpatialBounds::Empty();
    assert(!bounds.IsValid());

    bounds.Expand({1000000.25, -2.0, 4.0});
    bounds.Expand({1000002.25, 6.0, 10.0});
    assert(bounds.IsValid());

    const auto center = bounds.Center();
    const auto size = bounds.Size();
    assert(NearlyEqual(center.x, 1000001.25));
    assert(NearlyEqual(center.y, 2.0));
    assert(NearlyEqual(center.z, 7.0));
    assert(NearlyEqual(size.x, 2.0));
    assert(NearlyEqual(size.y, 8.0));
    assert(NearlyEqual(size.z, 6.0));
}

void TestGeoReference() {
    usdgeo::GeoReference reference;
    assert(!reference.IsValid());

    reference.epsgCode = 6677;
    reference.localOrigin = {1000000.0, 2000000.0, 3000000.0};
    assert(reference.IsValid());

    const usdgeo::Vec3d source{1000000.25, 1999998.0, 3000010.0};
    usdgeo::Vec3d local;
    assert(reference.TryToLocal(source, local));
    assert(NearlyEqual(local.x, 0.25));
    assert(NearlyEqual(local.y, -2.0));
    assert(NearlyEqual(local.z, 10.0));

    usdgeo::Vec3d reconstructed;
    assert(reference.TryToSource(local, reconstructed));
    assert(NearlyEqual(reconstructed.x, source.x));
    assert(NearlyEqual(reconstructed.y, source.y));
    assert(NearlyEqual(reconstructed.z, source.z));

    reference.localOrigin.x = std::numeric_limits<double>::infinity();
    assert(!reference.IsValid());
    reference.localOrigin = {1000000.0, 2000000.0, 3000000.0};
    assert(!reference.TryToLocal(
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}, local));

    reference.upAxis = "invalid";
    assert(!reference.IsValid());
}

void TestTileAndCacheKeys() {
    const usdgeo::TileId tile{3, -4, 12, 0};
    assert(tile.IsValid());
    assert(tile.ToString() == "L3/-4/12/0");

    const usdgeo::CacheArguments first{{" path ", " /data/sample.las "},
                                       {"limit", "1000"}};
    const usdgeo::CacheArguments equivalent{{"LIMIT", "1000"},
                                             {"path", "/data/sample.las"}};
    assert(usdgeo::NormalizeCacheArguments(first) ==
           usdgeo::NormalizeCacheArguments(equivalent));
    assert(usdgeo::StableCacheKey(first) == usdgeo::StableCacheKey(equivalent));
    assert(usdgeo::StableCacheKey(first) !=
           usdgeo::StableCacheKey({{"limit", "1001"},
                                   {"path", "/data/sample.las"}}));
}

} // namespace

int main() {
    TestBounds();
    TestGeoReference();
    TestTileAndCacheKeys();
    return 0;
}
