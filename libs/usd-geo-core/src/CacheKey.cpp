#include "usdgeo/CacheKey.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace usdgeo {
namespace {

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    return value;
}

std::string NormalizeName(std::string value) {
    value = Trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

} // namespace

std::string NormalizeCacheArguments(const CacheArguments& arguments) {
    CacheArguments normalized;
    normalized.reserve(arguments.size());
    for (const auto& [name, value] : arguments) {
        normalized.emplace_back(NormalizeName(name), Trim(value));
    }

    std::sort(normalized.begin(), normalized.end());

    std::ostringstream result;
    for (const auto& [name, value] : normalized) {
        result << name.size() << ':' << name << value.size() << ':' << value
               << ';';
    }
    return result.str();
}

std::string StableCacheKey(const CacheArguments& arguments) {
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;
    for (const unsigned char character : NormalizeCacheArguments(arguments)) {
        hash ^= character;
        hash *= prime;
    }

    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << hash;
    return result.str();
}

} // namespace usdgeo