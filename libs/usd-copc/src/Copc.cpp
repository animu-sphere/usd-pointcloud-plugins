#include "usdcopc/Copc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::size_t kCopcInfoSize = 160;
constexpr std::size_t kHierarchyEntrySize = 32;

bool Has(const std::vector<std::uint8_t>& bytes,
         std::size_t offset,
         std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

template <typename T>
T ReadLittle(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(std::uint64_t));
    std::array<std::uint8_t, sizeof(T)> encoded{};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        encoded[index] = bytes[offset + index];
    }
    const std::uint16_t marker = 1;
    const bool nativeLittleEndian =
        *reinterpret_cast<const std::uint8_t*>(&marker) == 1;
    if (!nativeLittleEndian) {
        std::reverse(encoded.begin(), encoded.end());
    }
    T value{};
    std::memcpy(&value, encoded.data(), sizeof(T));
    return value;
}

void AddDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                   usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::optional<std::uint64_t> byteOffset = std::nullopt) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message, byteOffset,
                           std::nullopt});
}

bool GetFileSize(const std::string& filename, std::uint64_t& size) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const auto end = file.tellg();
    if (end < 0) {
        return false;
    }
    size = static_cast<std::uint64_t>(end);
    return true;
}

bool IsFileRangeValid(std::uint64_t fileSize,
                      std::uint64_t offset,
                      std::uint64_t size) {
    return offset <= fileSize && size <= fileSize - offset;
}

bool ReadFileRange(const std::string& filename,
                   std::uint64_t fileSize,
                   std::uint64_t offset,
                   std::uint64_t size,
                   std::vector<std::uint8_t>& bytes,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (!IsFileRangeValid(fileSize, offset, size) ||
        size > static_cast<std::uint64_t>(maxSize)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC file range is outside the file", offset);
        return false;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "cannot open COPC file");
        return false;
    }
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!file) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "cannot seek to COPC file range", offset);
        return false;
    }

    bytes.assign(static_cast<std::size_t>(size), 0);
    if (size != 0 &&
        !file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(size))) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::TruncatedRecord,
                      "COPC file range is truncated", offset);
        return false;
    }
    return true;
}

const usdlas::LasVariableLengthRecord* FindCopcInfo(
    const std::vector<usdlas::LasVariableLengthRecord>& records) {
    for (const auto& record : records) {
        if (record.userId == "copc" && record.recordId == 1) {
            return &record;
        }
    }
    return nullptr;
}

} // namespace

namespace usdcopc {

bool CopcInfo::IsValid() const noexcept {
    return std::isfinite(centerX) && std::isfinite(centerY) &&
           std::isfinite(centerZ) && std::isfinite(halfSize) &&
           std::isfinite(spacing) && halfSize > 0.0 && spacing > 0.0 &&
           rootHierarchyOffset > 0 && rootHierarchySize > 0 &&
           std::isfinite(gpsTimeMin) && std::isfinite(gpsTimeMax) &&
           gpsTimeMin <= gpsTimeMax;
}

bool CopcHierarchyEntry::IsValid() const noexcept {
    return level >= 0 && pointCount >= -1 &&
           ((pointCount == 0 && offset == 0 && byteSize == 0) ||
            (pointCount != 0 && offset > 0 && byteSize > 0));
}

bool CopcHierarchyEntry::IsHierarchyPage() const noexcept {
    return pointCount == -1;
}

bool CopcHierarchyEntry::IsPointData() const noexcept {
    return pointCount > 0;
}

bool CopcHeader::IsValid() const noexcept {
    return las.IsValid() && las.versionMajor == 1 && las.versionMinor == 4 &&
           las.pointFormat >= 6 && las.pointFormat <= 10 && info.IsValid() &&
           fileSize > 0 &&
           info.rootHierarchySize % kHierarchyEntrySize == 0 &&
           IsFileRangeValid(fileSize, info.rootHierarchyOffset,
                            info.rootHierarchySize);
}

CopcReader::CopcReader(std::string filename)
    : filename_(std::move(filename)) {}

bool CopcReader::ReadMetadata(
    CopcHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    failureKind_ = CopcReadFailure::None;
    header = {};

    usdlas::LasReader lasReader(filename_);
    if (!lasReader.ReadMetadata(header.las, diagnostics)) {
        failureKind_ = CopcReadFailure::LasMetadata;
        return false;
    }
    if (header.las.versionMajor != 1 || header.las.versionMinor != 4) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::UnsupportedVersion,
                      "COPC requires LAS version 1.4");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }
    if (header.las.pointFormat < 6 || header.las.pointFormat > 10) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::UnsupportedPointFormat,
                      "COPC requires LAS point format 6 through 10");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }

    const auto* infoRecord = FindCopcInfo(header.las.variableLengthRecords);
    if (infoRecord == nullptr) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "COPC info VLR is missing");
        failureKind_ = CopcReadFailure::MissingInfo;
        return false;
    }
    if (infoRecord->data.size() != kCopcInfoSize) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::TruncatedRecord,
                      "COPC info VLR must contain 160 bytes");
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }

    if (!GetFileSize(filename_, header.fileSize)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "cannot determine COPC file size");
        failureKind_ = CopcReadFailure::FileOpen;
        return false;
    }

    const auto& data = infoRecord->data;
    header.info.centerX = ReadLittle<double>(data, 0);
    header.info.centerY = ReadLittle<double>(data, 8);
    header.info.centerZ = ReadLittle<double>(data, 16);
    header.info.halfSize = ReadLittle<double>(data, 24);
    header.info.spacing = ReadLittle<double>(data, 32);
    header.info.rootHierarchyOffset = ReadLittle<std::uint64_t>(data, 40);
    header.info.rootHierarchySize = ReadLittle<std::uint64_t>(data, 48);
    header.info.gpsTimeMin = ReadLittle<double>(data, 56);
    header.info.gpsTimeMax = ReadLittle<double>(data, 64);

    if (!header.info.IsValid() ||
        header.info.rootHierarchySize % kHierarchyEntrySize != 0 ||
        !IsFileRangeValid(header.fileSize, header.info.rootHierarchyOffset,
                          header.info.rootHierarchySize)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC hierarchy range is invalid",
                      header.info.rootHierarchyOffset);
        failureKind_ = CopcReadFailure::InvalidInfo;
        return false;
    }

    return true;
}

bool CopcReader::ReadHierarchy(
    const CopcHeader& header,
    std::vector<CopcHierarchyEntry>& entries,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    failureKind_ = CopcReadFailure::None;
    entries.clear();
    if (!header.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                      "COPC header is not valid");
        failureKind_ = CopcReadFailure::Hierarchy;
        return false;
    }

    std::unordered_set<std::uint64_t> visitedPages;
    std::function<bool(std::uint64_t, std::uint64_t)> readPage;
    readPage = [&](std::uint64_t offset, std::uint64_t size) {
        if (size == 0 || size % kHierarchyEntrySize != 0 ||
            !IsFileRangeValid(header.fileSize, offset, size)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                          "COPC hierarchy page range is invalid", offset);
            return false;
        }
        if (!visitedPages.insert(offset).second) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "COPC hierarchy contains a repeated page", offset);
            return false;
        }

        std::vector<std::uint8_t> page;
        if (!ReadFileRange(filename_, header.fileSize, offset, size, page,
                           diagnostics)) {
            return false;
        }
        for (std::size_t pageOffset = 0; pageOffset < page.size();
             pageOffset += kHierarchyEntrySize) {
            CopcHierarchyEntry entry;
            entry.level = ReadLittle<std::int32_t>(page, pageOffset);
            entry.x = ReadLittle<std::int32_t>(page, pageOffset + 4);
            entry.y = ReadLittle<std::int32_t>(page, pageOffset + 8);
            entry.z = ReadLittle<std::int32_t>(page, pageOffset + 12);
            entry.pointCount = ReadLittle<std::int32_t>(page, pageOffset + 16);
            entry.offset = ReadLittle<std::uint64_t>(page, pageOffset + 20);
            entry.byteSize = ReadLittle<std::uint32_t>(page, pageOffset + 28);

            const auto entryFileOffset = offset + pageOffset;
            if (!entry.IsValid()) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                              "COPC hierarchy entry is invalid",
                              entryFileOffset);
                return false;
            }
            if (entry.IsHierarchyPage()) {
                if (entry.byteSize % kHierarchyEntrySize != 0 ||
                    !IsFileRangeValid(header.fileSize, entry.offset,
                                      entry.byteSize)) {
                    AddDiagnostic(diagnostics,
                                  usdgeo::DiagnosticCode::InvalidOffset,
                                  "COPC child hierarchy page range is invalid",
                                  entryFileOffset);
                    return false;
                }
                entries.push_back(entry);
                if (!readPage(entry.offset, entry.byteSize)) {
                    return false;
                }
                continue;
            }
            if (entry.IsPointData() &&
                !IsFileRangeValid(header.fileSize, entry.offset,
                                  entry.byteSize)) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidOffset,
                              "COPC point data range is invalid", entryFileOffset);
                return false;
            }
            entries.push_back(entry);
        }
        return true;
    };

    if (!readPage(header.info.rootHierarchyOffset,
                  header.info.rootHierarchySize)) {
        failureKind_ = CopcReadFailure::Hierarchy;
        entries.clear();
        return false;
    }
    return true;
}

CopcReadFailure CopcReader::FailureKind() const noexcept {
    return failureKind_;
}

} // namespace usdcopc
