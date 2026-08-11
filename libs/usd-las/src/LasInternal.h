#pragma once

#include "usdlas/Las.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace usdlas::detail {

enum class RangeReadFailure {
    None,
    InvalidOffset,
    Seek,
    Read,
};

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

bool Has(const std::vector<std::uint8_t>& bytes,
         std::size_t offset,
         std::size_t size);

std::string ReadText(const std::vector<std::uint8_t>& bytes,
                     std::size_t offset,
                     std::size_t size);

bool IsSupportedFormat(std::uint8_t format);
bool IsSupportedFormatForVersion(std::uint8_t versionMinor,
                                 std::uint8_t format);

std::size_t MinimumHeaderSize(std::uint8_t versionMinor);
std::size_t MinimumRecordLength(std::uint8_t format);
std::size_t ExtraByteScalarSize(std::uint8_t dataType);
std::uint8_t ExtraByteComponentCount(std::uint8_t dataType);

bool ValidateExtraBytesLayout(const LasHeader& header, std::string& error);

double ReadExtraByteScalar(const std::vector<std::uint8_t>& record,
                           std::size_t offset,
                           std::uint8_t dataType);

usdgeo::DiagnosticCode CodeForError(const std::string& error);
void AddErrorDiagnostic(const std::string& error,
                        std::vector<usdgeo::Diagnostic>& diagnostics);

bool ReadFileRange(usdgeo::RandomAccessSource& source,
                   std::uint64_t offset,
                   std::size_t size,
                   std::vector<std::uint8_t>& bytes,
                   std::string& error,
                   RangeReadFailure& failure,
                   std::vector<usdgeo::Diagnostic>* diagnostics = nullptr);

bool ReadRecords(const std::vector<std::uint8_t>& bytes,
                 std::size_t offset,
                 std::uint32_t count,
                 bool extended,
                 std::vector<LasVariableLengthRecord>& records,
                 std::string& error);

} // namespace usdlas::detail