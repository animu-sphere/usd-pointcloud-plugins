#pragma once

#include "usdgeo/RandomAccessSource.h"

#include <pxr/usd/ar/asset.h>

#include <memory>
#include <string>

namespace usdgeocopc {

class ArAssetRandomAccessSource final : public usdgeo::RandomAccessSource {
public:
    ArAssetRandomAccessSource(std::shared_ptr<pxr::ArAsset> asset,
                              std::string sourceName);

    bool GetSize(std::uint64_t& size,
                 std::vector<usdgeo::Diagnostic>& diagnostics) const override;
    bool Read(std::uint64_t offset,
              std::size_t size,
              std::vector<std::uint8_t>& bytes,
              std::vector<usdgeo::Diagnostic>& diagnostics) override;

private:
    std::shared_ptr<pxr::ArAsset> asset_;
    std::string sourceName_;
};

} // namespace usdgeocopc