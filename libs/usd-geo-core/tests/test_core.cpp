#include "usdgeo/GeoReference.h"
#include "usdgeo/SpatialBounds.h"

#include <cassert>
#include <cmath>

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
    reference.localOrigin = {1000000.0, 0.0, 0.0};
    assert(reference.IsValid());

    reference.upAxis = "invalid";
    assert(!reference.IsValid());
}

} // namespace

int main() {
    TestBounds();
    TestGeoReference();
    return 0;
}
