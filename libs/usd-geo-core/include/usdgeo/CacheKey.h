#pragma once

#include <string>
#include <utility>
#include <vector>

namespace usdgeo {

using CacheArguments = std::vector<std::pair<std::string, std::string>>;

std::string NormalizeCacheArguments(const CacheArguments& arguments);
std::string StableCacheKey(const CacheArguments& arguments);

} // namespace usdgeo