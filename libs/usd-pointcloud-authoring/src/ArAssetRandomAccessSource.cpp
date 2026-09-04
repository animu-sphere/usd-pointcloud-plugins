#include "usdgeo/ArAssetRandomAccessSource.h"

#include <limits>
#include <utility>

namespace usdgeo {
namespace {

void AddDiagnostic(std::vector<Diagnostic>& diagnostics,
                   DiagnosticCode code,
                   const std::string& message,
                   std::optional<std::uint64_t> byteOffset = std::nullopt) {
    diagnostics.push_back(
        {code, Severity::Error, message, byteOffset, std::nullopt});
}

} // namespace

ArAssetRandomAccessSource::ArAssetRandomAccessSource(
    std::shared_ptr<pxr::ArAsset> asset,
    std::string sourceName)
    : asset_(std::move(asset)), sourceName_(std::move(sourceName)) {}

bool ArAssetRandomAccessSource::GetSize(
    std::uint64_t& size,
    std::vector<Diagnostic>& diagnostics) const {
    if (!asset_) {
        AddDiagnostic(diagnostics, DiagnosticCode::SourceOpenFailed,
                      "resolver returned no asset: " + sourceName_);
        return false;
    }
    size = static_cast<std::uint64_t>(asset_->GetSize());
    return true;
}

bool ArAssetRandomAccessSource::Read(
    std::uint64_t offset,
    std::size_t size,
    std::vector<std::uint8_t>& bytes,
    std::vector<Diagnostic>& diagnostics) {
    bytes.clear();
    std::uint64_t assetSize = 0;
    if (!GetSize(assetSize, diagnostics)) {
        return false;
    }
    if (offset > assetSize ||
        static_cast<std::uint64_t>(size) > assetSize - offset) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "asset byte range is outside the asset: " + sourceName_,
                      offset);
        return false;
    }
    if (offset > static_cast<std::uint64_t>(
                     (std::numeric_limits<std::size_t>::max)())) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "asset byte range offset is invalid: " + sourceName_,
                      offset);
        return false;
    }

    bytes.assign(size, 0);
    if (!bytes.empty() &&
        asset_->Read(bytes.data(), bytes.size(), static_cast<std::size_t>(offset)) !=
            bytes.size()) {
        bytes.clear();
        AddDiagnostic(diagnostics, DiagnosticCode::TruncatedRecord,
                      "asset byte range read was short: " + sourceName_,
                      offset);
        return false;
    }
    return true;
}

} // namespace usdgeo
