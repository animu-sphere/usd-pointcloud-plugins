#include "usdgeo/CacheKey.h"
#include "usdgeo/GeoReference.h"
#include "usdgeo/SpatialBounds.h"
#include "usdgeo/TileId.h"

#include <cstdlib>
#include <cmath>
#include <limits>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool NearlyEqual(double left, double right) {
    return std::abs(left - right) < 1e-12;
}

void TestBounds() {
    auto bounds = usdgeo::SpatialBounds::Empty();
    Check(!bounds.IsValid());

    bounds.Expand({1000000.25, -2.0, 4.0});
    bounds.Expand({1000002.25, 6.0, 10.0});
    Check(bounds.IsValid());

    bounds.Expand({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    Check(bounds.IsValid());
    bounds.Expand({std::numeric_limits<double>::infinity(), 0.0, 0.0});
    Check(bounds.IsValid());

    const auto center = bounds.Center();
    const auto size = bounds.Size();
    Check(NearlyEqual(center.x, 1000001.25));
    Check(NearlyEqual(center.y, 2.0));
    Check(NearlyEqual(center.z, 7.0));
    Check(NearlyEqual(size.x, 2.0));
    Check(NearlyEqual(size.y, 8.0));
    Check(NearlyEqual(size.z, 6.0));
}

void TestGeoReference() {
    usdgeo::GeoReference reference;
    Check(!reference.IsValid());

    reference.epsgCode = 6677;
    reference.localOrigin = {1000000.0, 2000000.0, 3000000.0};
    Check(reference.IsValid());

    const usdgeo::Vec3d source{1000000.25, 1999998.0, 3000010.0};
    usdgeo::Vec3d local;
    Check(reference.TryToLocal(source, local));
    Check(NearlyEqual(local.x, 0.25));
    Check(NearlyEqual(local.y, -2.0));
    Check(NearlyEqual(local.z, 10.0));

    usdgeo::Vec3d reconstructed;
    Check(reference.TryToSource(local, reconstructed));
    Check(NearlyEqual(reconstructed.x, source.x));
    Check(NearlyEqual(reconstructed.y, source.y));
    Check(NearlyEqual(reconstructed.z, source.z));

    reference.upAxis = "Y";
    const usdgeo::Vec3d yUpSource{1000001.0, 1999998.0, 3000004.0};
    Check(reference.TryToLocal(yUpSource, local));
    Check(NearlyEqual(local.x, 1.0));
    Check(NearlyEqual(local.y, 4.0));
    Check(NearlyEqual(local.z, 2.0));
    Check(reference.TryToSource(local, reconstructed));
    Check(NearlyEqual(reconstructed.x, yUpSource.x));
    Check(NearlyEqual(reconstructed.y, yUpSource.y));
    Check(NearlyEqual(reconstructed.z, yUpSource.z));

    usdgeo::SpatialBounds yUpBounds;
    Check(reference.TryToLocal(
        {{1000000.0, 1999990.0, 3000000.0},
         {1000002.0, 2000000.0, 3000005.0}},
        yUpBounds));
    Check(NearlyEqual(yUpBounds.minimum.y, 0.0));
    Check(NearlyEqual(yUpBounds.maximum.y, 5.0));
    Check(NearlyEqual(yUpBounds.minimum.z, 0.0));
    Check(NearlyEqual(yUpBounds.maximum.z, 10.0));

    reference.localOrigin.x = std::numeric_limits<double>::infinity();
    Check(!reference.IsValid());
    reference.localOrigin = {1000000.0, 2000000.0, 3000000.0};
    Check(!reference.TryToLocal(
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}, local));

    reference.upAxis = "invalid";
    Check(!reference.IsValid());
}

void TestTileAndCacheKeys() {
    const usdgeo::TileId tile{3, -4, 12, 0};
    Check(tile.IsValid());
    Check(tile.ToString() == "L3/-4/12/0");

    const usdgeo::CacheArguments first{{" path ", " /data/sample.las "},
                                       {"limit", "1000"}};
    const usdgeo::CacheArguments equivalent{{"LIMIT", "1000"},
                                             {"path", "/data/sample.las"}};
        Check(usdgeo::NormalizeCacheArguments(first) ==
            usdgeo::NormalizeCacheArguments(equivalent));
        Check(usdgeo::StableCacheKey(first) == usdgeo::StableCacheKey(equivalent));
        Check(usdgeo::StableCacheKey(first) !=
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
