#include "usdpointcloud/FileFormatArguments.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace usdpointcloud {

namespace {

std::string Trim(std::string_view value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

void AddDiagnostic(usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, std::nullopt, std::nullopt});
}

bool ParseUnsigned(std::string_view value,
                   std::uint64_t& result,
                   std::string& error) {
    if (value.empty()) {
        error = "format argument value is empty";
        return false;
    }
    result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(),
                                        result);
    if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size()) {
        error = "format argument value must be an unsigned integer";
        return false;
    }
    return true;
}

bool ParsePositiveDouble(std::string_view value, double& result) {
    std::string text(value);
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' && std::isfinite(result) &&
           result > 0.0;
}

bool ParseFiniteDouble(std::string_view value, double& result) {
    std::string text(value);
    char* end = nullptr;
    result = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0' && std::isfinite(result);
}

bool ParseBounds(std::string_view value,
                 usdgeo::SpatialBounds& bounds) {
    std::array<double, 6> coordinates{};
    std::size_t start = 0;
    bool consumedAll = false;
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
        const auto end = value.find(',', start);
        const auto component = Trim(value.substr(
            start, end == std::string_view::npos ? value.size() - start
                                                   : end - start));
        if (component.empty() || !ParseFiniteDouble(component, coordinates[index])) {
            return false;
        }
        if (end == std::string_view::npos) {
            if (index + 1 != coordinates.size()) return false;
            consumedAll = true;
            break;
        }
        start = end + 1;
    }
    if (!consumedAll) return false;
    bounds = {{coordinates[0], coordinates[1], coordinates[2]},
              {coordinates[3], coordinates[4], coordinates[5]}};
    return bounds.IsValid();
}

bool ParseClassifications(std::string_view value,
                          std::vector<std::uint8_t>& classifications) {
    classifications.clear();
    std::set<std::uint8_t> unique;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(',', start);
        const auto component = Trim(value.substr(
            start, end == std::string_view::npos ? value.size() - start
                                                   : end - start));
        std::uint64_t parsed = 0;
        std::string error;
        if (component.empty() || !ParseUnsigned(component, parsed, error) ||
            parsed > std::numeric_limits<std::uint8_t>::max()) {
            return false;
        }
        unique.insert(static_cast<std::uint8_t>(parsed));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    classifications.assign(unique.begin(), unique.end());
    return !classifications.empty();
}

bool IsUnsupportedArgument(const std::string& key) {
    static const std::set<std::string> keys = {
        "lodLevels", "lodPointCounts", "lodRatios",
        "lodThresholds", "tileDepth", "tilePointLimit",
        "sampling", "originMode", "upAxis"};
    return keys.find(key) != keys.end();
}

bool IsAttributeName(const std::string& name) {
    static const std::set<std::string> names = {
        "xyz", "intensity", "returnNumber", "numberOfReturns",
        "classification", "classificationFlags", "scannerChannel",
        "scanDirectionFlag", "edgeOfFlightLine", "userData", "scanAngle",
        "pointSourceId", "rgb", "red", "green", "blue", "nir",
        "gpsTime", "waveformDescriptorIndex", "waveformDataOffset",
        "waveformPacketSize", "returnPointWaveformLocation", "waveformXt",
        "waveformYt", "waveformZt", "waveformDataExternal",
        "waveformDataFile"};
    return names.find(name) != names.end();
}

void AddAttributeGroup(const std::string& name, std::set<std::string>& selected) {
    if (name == "rgb" || name == "red" || name == "green" || name == "blue") {
        selected.insert("blue");
        selected.insert("green");
        selected.insert("red");
        return;
    }
    if (name == "returnNumber" || name == "numberOfReturns") {
        selected.insert("numberOfReturns");
        selected.insert("returnNumber");
        return;
    }
    if (name == "waveformDataFile" || name.find("waveform") == 0 ||
        name == "returnPointWaveformLocation") {
        selected.insert("returnPointWaveformLocation");
        selected.insert("waveformDataExternal");
        selected.insert("waveformDataFile");
        selected.insert("waveformDataOffset");
        selected.insert("waveformDescriptorIndex");
        selected.insert("waveformPacketSize");
        selected.insert("waveformXt");
        selected.insert("waveformYt");
        selected.insert("waveformZt");
        return;
    }
    selected.insert(name);
}

std::string Join(const std::set<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result += ',';
        }
        result += value;
    }
    return result;
}

} // namespace

bool ParseFileFormatArgumentString(
    std::string_view encoded,
    std::map<std::string, std::string>& arguments,
    std::string& error) {
    arguments.clear();
    if (encoded.empty()) {
        return true;
    }

    std::size_t start = 0;
    while (start <= encoded.size()) {
        const auto end = encoded.find('&', start);
        const auto part = encoded.substr(
            start, end == std::string_view::npos ? encoded.size() - start
                                                   : end - start);
        const auto separator = part.find('=');
        if (separator == std::string_view::npos) {
            error = "format argument must use key=value syntax";
            return false;
        }
        const auto key = Trim(part.substr(0, separator));
        const auto value = Trim(part.substr(separator + 1));
        if (key.empty() || value.empty()) {
            error = "format argument key and value must not be empty";
            return false;
        }
        if (!arguments.emplace(key, value).second) {
            error = "format argument key is repeated: " + key;
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

bool NormalizeFileFormatArguments(
    const std::map<std::string, std::string>& arguments,
    PointReadRequest& request,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    request = {};
    diagnostics.clear();
    std::set<std::string> selectedAttributes;
    bool attributesSpecified = false;

    for (const auto& [key, value] : arguments) {
        std::uint64_t parsed = 0;
        std::string error;
        if (key == "lod") {
            const auto normalizedProfile = Trim(value);
            if (normalizedProfile == "off") {
                request.lodProfile = LodProfile::Off;
            } else if (normalizedProfile == "preview") {
                request.lodProfile = LodProfile::Preview;
            } else if (normalizedProfile == "balanced") {
                request.lodProfile = LodProfile::Balanced;
            } else if (normalizedProfile == "quality") {
                request.lodProfile = LodProfile::Quality;
            } else {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid lod profile: " + value, diagnostics);
                return false;
            }
        } else if (key == "tile") {
            if (value == "true") {
                request.tiled = true;
            } else if (value == "false") {
                request.tiled = false;
            } else {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid tile value: " + value, diagnostics);
                return false;
            }
        } else if (key == "tileSize") {
            if (!ParsePositiveDouble(value, request.tileSize)) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid tileSize: " + value, diagnostics);
                return false;
            }
            request.tiled = true;
        } else if (key == "tileMemoryLimit") {
            if (!ParseUnsigned(value, parsed, error) ||
                parsed == 0 || parsed > (std::numeric_limits<std::size_t>::max)()) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid tileMemoryLimit: " + value, diagnostics);
                return false;
            }
            request.tileMemoryLimitBytes = static_cast<std::size_t>(parsed);
            request.tiled = true;
        } else if (key == "payloadDirectory") {
            request.payloadDirectory = value;
            request.tiled = true;
        } else if (key == "chunkPointLimit") {
            if (!ParseUnsigned(value, parsed, error) ||
                parsed > (std::numeric_limits<std::size_t>::max)()) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid chunkPointLimit: " + value, diagnostics);
                return false;
            }
            request.readOptions.chunkPointLimit = static_cast<std::size_t>(parsed);
        } else if (key == "memoryBudgetBytes") {
            if (!ParseUnsigned(value, parsed, error) ||
                parsed > (std::numeric_limits<std::size_t>::max)()) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid memoryBudgetBytes: " + value, diagnostics);
                return false;
            }
            request.readOptions.memoryBudgetBytes = static_cast<std::size_t>(parsed);
        } else if (key == "rangeFirstPoint") {
            if (!ParseUnsigned(value, parsed, error)) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid rangeFirstPoint: " + value, diagnostics);
                return false;
            }
            request.readOptions.range.firstPoint = parsed;
        } else if (key == "rangePointCount") {
            if (!ParseUnsigned(value, parsed, error)) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid rangePointCount: " + value, diagnostics);
                return false;
            }
            request.readOptions.range.pointCount = parsed;
        } else if (key == "bounds") {
            usdgeo::SpatialBounds bounds;
            if (!ParseBounds(value, bounds)) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid bounds: " + value, diagnostics);
                return false;
            }
            request.readOptions.bounds = bounds;
        } else if (key == "classification") {
            if (!ParseClassifications(value, request.readOptions.classifications)) {
                AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                              "invalid classification filter: " + value,
                              diagnostics);
                return false;
            }
        } else if (key == "attributes") {
            attributesSpecified = true;
            std::size_t start = 0;
            while (start <= value.size()) {
                const auto end = value.find(',', start);
                const auto name = Trim(value.substr(
                    start, end == std::string::npos ? value.size() - start
                                                     : end - start));
                if (name.empty() || !IsAttributeName(name)) {
                    AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                  "unknown point attribute: " + name,
                                  diagnostics);
                    return false;
                }
                AddAttributeGroup(name, selectedAttributes);
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
        } else if (IsUnsupportedArgument(key)) {
            AddDiagnostic(usdgeo::DiagnosticCode::UnsupportedFormatArgument,
                          "format argument is not implemented: " + key,
                          diagnostics);
            return false;
        } else {
            AddDiagnostic(usdgeo::DiagnosticCode::UnknownFormatArgument,
                          "unknown format argument: " + key, diagnostics);
            return false;
        }
    }

    if (!request.readOptions.IsValid()) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                      "point read arguments are outside their valid range",
                      diagnostics);
        return false;
    }
    if (request.tiled && (request.tileSize <= 0.0 ||
                          request.payloadDirectory.empty())) {
        AddDiagnostic(usdgeo::DiagnosticCode::InvalidFormatArgument,
                      "tiled reads require tileSize and payloadDirectory",
                      diagnostics);
        return false;
    }
    if (request.tiled && request.lodProfile != LodProfile::Off) {
        AddDiagnostic(usdgeo::DiagnosticCode::ConflictingFormatArguments,
                      "tile cannot be combined with lod", diagnostics);
        return false;
    }
    if (attributesSpecified) {
        selectedAttributes.insert("xyz");
        request.attributes.assign(selectedAttributes.begin(),
                                  selectedAttributes.end());
    }

    std::vector<std::pair<std::string, std::string>> normalized;
    if (request.readOptions.chunkPointLimit != 65536) {
        normalized.emplace_back("chunkPointLimit",
                                std::to_string(request.readOptions.chunkPointLimit));
    }
    if (request.readOptions.memoryBudgetBytes != 64 * 1024 * 1024) {
        normalized.emplace_back(
            "memoryBudgetBytes",
            std::to_string(request.readOptions.memoryBudgetBytes));
    }
    if (request.readOptions.range.firstPoint != 0) {
        normalized.emplace_back("rangeFirstPoint",
                                std::to_string(request.readOptions.range.firstPoint));
    }
    if (request.readOptions.range.pointCount != 0) {
        normalized.emplace_back("rangePointCount",
                                std::to_string(request.readOptions.range.pointCount));
    }
    if (request.readOptions.bounds) {
        const auto& bounds = *request.readOptions.bounds;
        normalized.emplace_back(
            "bounds", std::to_string(bounds.minimum.x) + "," +
                           std::to_string(bounds.minimum.y) + "," +
                           std::to_string(bounds.minimum.z) + "," +
                           std::to_string(bounds.maximum.x) + "," +
                           std::to_string(bounds.maximum.y) + "," +
                           std::to_string(bounds.maximum.z));
    }
    if (!request.readOptions.classifications.empty()) {
        std::string value;
        for (const auto classification : request.readOptions.classifications) {
            if (!value.empty()) value += ',';
            value += std::to_string(classification);
        }
        normalized.emplace_back("classification", value);
    }
    if (request.lodProfile != LodProfile::Off) {
        const char* profile = request.lodProfile == LodProfile::Preview
                                  ? "preview"
                                  : request.lodProfile == LodProfile::Balanced
                                        ? "balanced"
                                        : "quality";
        normalized.emplace_back("lod", profile);
    }
    if (request.tiled) {
        normalized.emplace_back("tile", "true");
        normalized.emplace_back("tileSize", std::to_string(request.tileSize));
        if (request.tileMemoryLimitBytes != 64 * 1024 * 1024) {
            normalized.emplace_back("tileMemoryLimit",
                                    std::to_string(request.tileMemoryLimitBytes));
        }
        normalized.emplace_back("payloadDirectory", request.payloadDirectory);
    }
    if (attributesSpecified) {
        normalized.emplace_back("attributes", Join(selectedAttributes));
    }
    for (const auto& [key, value] : normalized) {
        request.canonicalArguments.emplace(key, value);
        if (!request.normalizedArguments.empty()) {
            request.normalizedArguments += '&';
        }
        request.normalizedArguments += key + '=' + value;
    }
    return true;
}

bool MakeReadRequest(
    const std::map<std::string, std::string>& arguments,
    PointReadRequest& request,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    return NormalizeFileFormatArguments(arguments, request, diagnostics);
}

bool SelectPointDataAttributes(PointData& data,
                               const std::vector<std::string>& attributes,
                               std::string& error) {
    if (attributes.empty()) {
        return data.IsValid();
    }
    if (!data.IsValid()) {
        error = "point attributes are inconsistent before selection";
        return false;
    }
    const std::set<std::string> selected(attributes.begin(), attributes.end());
    const auto keep = [&](const char* name) {
        return selected.find(name) != selected.end();
    };
    if (!keep("intensity")) data.intensity.clear();
    if (!keep("returnNumber")) {
        data.returnNumber.clear();
        data.numberOfReturns.clear();
    }
    if (!keep("classification")) data.classification.clear();
    if (!keep("classificationFlags")) data.classificationFlags.clear();
    if (!keep("scannerChannel")) data.scannerChannel.clear();
    if (!keep("scanDirectionFlag")) data.scanDirectionFlag.clear();
    if (!keep("edgeOfFlightLine")) data.edgeOfFlightLine.clear();
    if (!keep("userData")) data.userData.clear();
    if (!keep("scanAngle")) data.scanAngle.clear();
    if (!keep("pointSourceId")) data.pointSourceId.clear();
    if (!keep("red")) {
        data.red.clear();
        data.green.clear();
        data.blue.clear();
    }
    if (!keep("nir")) data.nir.clear();
    if (!keep("gpsTime")) data.gpsTime.clear();
    if (!keep("waveformDescriptorIndex")) {
        data.waveformDescriptorIndex.clear();
        data.waveformDataOffset.clear();
        data.waveformPacketSize.clear();
        data.returnPointWaveformLocation.clear();
        data.waveformXt.clear();
        data.waveformYt.clear();
        data.waveformZt.clear();
        data.waveformDataExternal.clear();
        data.waveformDataFile.clear();
    }
    for (std::size_t index = 0; index < data.extraBytes.size(); ++index) {
        if (index >= data.extraByteNames.size() ||
            !keep(data.extraByteNames[index].c_str())) {
            data.extraBytes[index].clear();
        }
    }
    if (!data.IsValid()) {
        error = "selected point attributes are inconsistent";
        return false;
    }
    return true;
}

} // namespace usdpointcloud