#include "usdpointcloud/Tiling.h"

#include <cstdlib>
#include <limits>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestConfigValidation() {
    usdpointcloud::TileGridConfig config{10.0, 2};
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(config.IsValid());
    Check(usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(diagnostics.empty());

    config.tileSize = 0.0;
    Check(!usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(!diagnostics.empty());

    diagnostics.clear();
    config = {10.0, -1};
    Check(!usdpointcloud::ValidateTileGridConfig(config, diagnostics));
    Check(diagnostics.size() == 1);
}

void TestFixedGridRouting() {
    const usdpointcloud::FixedGridTileRouter router({10.0, 0});
    Check(router.IsValid());
    Check(router.GetTileId({0.0, 0.0, 100.0}).x == 0);
    Check(router.GetTileId({9.999, 19.999, 0.0}).y == 1);
    Check(router.GetTileId({10.0, -10.0, 0.0}).x == 1);
    Check(router.GetTileId({-0.001, -10.0, 0.0}).x == -1);
    Check(router.GetTileId({-10.0, -20.0, 0.0}).y == -2);
}

void TestInvalidRouting() {
    const usdpointcloud::FixedGridTileRouter invalid({0.0, 0});
    Check(!invalid.IsValid());
    Check(!invalid.GetTileId({0.0, 0.0, 0.0}).IsValid());

    const usdpointcloud::FixedGridTileRouter valid({1.0, 0});
    Check(!valid.GetTileId({std::numeric_limits<double>::infinity(), 0.0, 0.0})
               .IsValid());
    const double int64UpperExclusive = std::ldexp(1.0, 63);
    Check(!valid.GetTileId({int64UpperExclusive, 0.0, 0.0}).IsValid());
    Check(valid.GetTileId({-int64UpperExclusive, 0.0, 0.0}).IsValid());
}

} // namespace

int main() {
    TestConfigValidation();
    TestFixedGridRouting();
    TestInvalidRouting();
    return 0;
}
