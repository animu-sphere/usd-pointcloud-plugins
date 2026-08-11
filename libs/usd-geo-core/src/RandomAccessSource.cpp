#include "usdgeo/RandomAccessSource.h"

#include <fstream>
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

LocalRandomAccessSource::LocalRandomAccessSource(std::string filename)
    : filename_(std::move(filename)) {}

bool LocalRandomAccessSource::GetSize(
    std::uint64_t& size,
    std::vector<Diagnostic>& diagnostics) const {
    std::ifstream file(filename_, std::ios::binary | std::ios::ate);
    if (!file) {
        AddDiagnostic(diagnostics, DiagnosticCode::SourceOpenFailed,
                      "could not open source: " + filename_);
        return false;
    }
    const auto end = file.tellg();
    if (end < 0) {
        AddDiagnostic(diagnostics, DiagnosticCode::SourceSizeUnavailable,
                      "could not determine source size: " + filename_);
        return false;
    }
    size = static_cast<std::uint64_t>(end);
    return true;
}

bool LocalRandomAccessSource::Read(
    std::uint64_t offset,
    std::size_t size,
    std::vector<std::uint8_t>& bytes,
    std::vector<Diagnostic>& diagnostics) {
    bytes.clear();
    if (size > static_cast<std::size_t>(
                   (std::numeric_limits<std::streamsize>::max)())) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "source byte range is too large", offset);
        return false;
    }

    std::ifstream file(filename_, std::ios::binary | std::ios::ate);
    if (!file) {
        AddDiagnostic(diagnostics, DiagnosticCode::SourceOpenFailed,
                      "could not open source: " + filename_);
        return false;
    }
    const auto end = file.tellg();
    if (end < 0) {
        AddDiagnostic(diagnostics, DiagnosticCode::SourceSizeUnavailable,
                      "could not determine source size: " + filename_);
        return false;
    }
    const auto sourceSize = static_cast<std::uint64_t>(end);
    if (offset > sourceSize ||
        static_cast<std::uint64_t>(size) > sourceSize - offset) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "source byte range is outside the source", offset);
        return false;
    }
    if (offset > static_cast<std::uint64_t>(
                     (std::numeric_limits<std::streamoff>::max)())) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "source byte range offset is invalid", offset);
        return false;
    }
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) {
        AddDiagnostic(diagnostics, DiagnosticCode::InvalidOffset,
                      "source byte range seek failed", offset);
        return false;
    }
    bytes.assign(size, 0);
    if (!bytes.empty() &&
        !file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()))) {
        bytes.clear();
        AddDiagnostic(diagnostics, DiagnosticCode::TruncatedRecord,
                      "source byte range is truncated", offset);
        return false;
    }
    return true;
}

std::unique_ptr<RandomAccessSource> OpenLocalRandomAccessSource(
    const std::string& filename) {
    return std::make_unique<LocalRandomAccessSource>(filename);
}

} // namespace usdgeo