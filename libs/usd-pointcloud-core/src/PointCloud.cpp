#include "usdpointcloud/PointCloud.h"

#include <algorithm>
#include <limits>
#include <set>

namespace usdpointcloud {

bool PointAttribute::IsValid() const noexcept {
    return !name.empty();
}

bool PointChunk::IsValid() const noexcept {
    if (pointCount == 0) {
        return !bounds.IsValid() && attributes.empty();
    }
    if (!bounds.IsValid()) {
        return false;
    }

    for (const auto& attribute : attributes) {
        if (!attribute.IsValid()) {
            return false;
        }
    }
    for (auto first = attributes.begin(); first != attributes.end(); ++first) {
        if (std::any_of(first + 1, attributes.end(),
                        [&](const PointAttribute& other) {
                            return other.name == first->name;
                        })) {
            return false;
        }
    }
    return true;
}

namespace {

template <typename T>
bool HasPointCountOrIsEmpty(const std::vector<T>& values,
                            std::size_t pointCount) {
    return values.empty() || values.size() == pointCount;
}

bool IsAsciiAlphaNumeric(unsigned char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

bool IsAsciiDigit(unsigned char character) {
    return character >= '0' && character <= '9';
}

std::string NormalizeExtraByteName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (const unsigned char character : name) {
        result += IsAsciiAlphaNumeric(character) || character == '_'
                      ? static_cast<char>(character)
                      : '_';
    }
    if (result.empty()) {
        return "extra";
    }
    if (IsAsciiDigit(static_cast<unsigned char>(result.front()))) {
        result.insert(result.begin(), '_');
    }
    return result;
}

std::set<std::string> ReservedPointAttributeNames() {
    return {"xyz", "intensity", "returnNumber", "numberOfReturns",
            "classification", "classificationFlags", "scannerChannel",
            "scanDirectionFlag", "edgeOfFlightLine", "userData",
            "scanAngle", "pointSourceId", "red", "green", "blue",
            "nir", "gpsTime", "waveformDescriptorIndex",
            "waveformDataOffset", "waveformPacketSize",
            "returnPointWaveformLocation", "waveformXt", "waveformYt",
            "waveformZt", "waveformDataExternal", "waveformDataFile"};
}

} // namespace

bool PointData::IsValid() const noexcept {
    const auto pointCount = positions.size();
    const auto hasValidExtraByteValues = [&](std::size_t index,
                                             const auto& values) {
        const auto componentCount = extraByteComponentCounts.empty()
                                        ? std::uint8_t{1}
                                        : extraByteComponentCounts[index];
        return componentCount >= 1 && componentCount <= 3 &&
               (values.empty() ||
                (pointCount <= (std::numeric_limits<std::size_t>::max)() /
                                   componentCount &&
                 values.size() == pointCount * componentCount));
    };
    const bool extraByteComponentCountsValid =
        extraByteComponentCounts.empty() ||
        extraByteComponentCounts.size() == extraBytes.size();
    bool extraByteValuesValid = extraByteComponentCountsValid;
    if (extraByteComponentCountsValid) {
        for (std::size_t index = 0; index < extraBytes.size(); ++index) {
            if (!hasValidExtraByteValues(index, extraBytes[index])) {
                extraByteValuesValid = false;
                break;
            }
        }
    }
        return (colorBitDepth == 8 || colorBitDepth == 16) &&
            HasPointCountOrIsEmpty(intensity, pointCount) &&
           HasPointCountOrIsEmpty(returnNumber, pointCount) &&
           HasPointCountOrIsEmpty(numberOfReturns, pointCount) &&
           HasPointCountOrIsEmpty(classification, pointCount) &&
           HasPointCountOrIsEmpty(classificationFlags, pointCount) &&
           HasPointCountOrIsEmpty(scannerChannel, pointCount) &&
           HasPointCountOrIsEmpty(scanDirectionFlag, pointCount) &&
           HasPointCountOrIsEmpty(edgeOfFlightLine, pointCount) &&
           HasPointCountOrIsEmpty(userData, pointCount) &&
           HasPointCountOrIsEmpty(scanAngle, pointCount) &&
           HasPointCountOrIsEmpty(pointSourceId, pointCount) &&
           HasPointCountOrIsEmpty(red, pointCount) &&
           HasPointCountOrIsEmpty(green, pointCount) &&
           HasPointCountOrIsEmpty(blue, pointCount) &&
           HasPointCountOrIsEmpty(nir, pointCount) &&
           HasPointCountOrIsEmpty(gpsTime, pointCount) &&
           HasPointCountOrIsEmpty(waveformDescriptorIndex, pointCount) &&
           HasPointCountOrIsEmpty(waveformDataOffset, pointCount) &&
           HasPointCountOrIsEmpty(waveformPacketSize, pointCount) &&
           HasPointCountOrIsEmpty(returnPointWaveformLocation, pointCount) &&
           HasPointCountOrIsEmpty(waveformXt, pointCount) &&
           HasPointCountOrIsEmpty(waveformYt, pointCount) &&
           HasPointCountOrIsEmpty(waveformZt, pointCount) &&
           HasPointCountOrIsEmpty(waveformDataExternal, pointCount) &&
           extraByteNames.size() == extraBytes.size() &&
           extraByteComponentCountsValid &&
           extraByteValuesValid &&
           (returnNumber.empty() == numberOfReturns.empty()) &&
           (red.empty() == green.empty()) && (red.empty() == blue.empty()) &&
           (waveformDescriptorIndex.empty() == waveformDataOffset.empty()) &&
           (waveformDescriptorIndex.empty() == waveformPacketSize.empty()) &&
           (waveformDescriptorIndex.empty() ==
            returnPointWaveformLocation.empty()) &&
           (waveformDescriptorIndex.empty() == waveformXt.empty()) &&
           (waveformDescriptorIndex.empty() == waveformYt.empty()) &&
           (waveformDescriptorIndex.empty() == waveformZt.empty()) &&
           (waveformDescriptorIndex.empty() == waveformDataExternal.empty());
}

bool PointCloudAsset::IsValid() const noexcept {
    return reference.IsValid() && bounds.IsValid() && data.IsValid() &&
           chunk.IsValid() && chunk.pointCount == data.positions.size();
}

PointChunk MakePointChunk(const PointData& data,
                         const usdgeo::SpatialBounds& bounds) {
    PointChunk chunk;
    chunk.pointCount = data.positions.size();
    chunk.bounds = bounds;
    const auto add = [&](const char* name, PointAttributeType type,
                         std::size_t size) {
        if (size != 0) {
            chunk.attributes.push_back({name, type});
        }
    };
    add("intensity", PointAttributeType::UInt16, data.intensity.size());
    add("returnNumber", PointAttributeType::UInt8, data.returnNumber.size());
    add("numberOfReturns", PointAttributeType::UInt8,
        data.numberOfReturns.size());
    add("classification", PointAttributeType::UInt8,
        data.classification.size());
    add("classificationFlags", PointAttributeType::UInt8,
        data.classificationFlags.size());
    add("scannerChannel", PointAttributeType::UInt8,
        data.scannerChannel.size());
    add("scanDirectionFlag", PointAttributeType::UInt8,
        data.scanDirectionFlag.size());
    add("edgeOfFlightLine", PointAttributeType::UInt8,
        data.edgeOfFlightLine.size());
    add("userData", PointAttributeType::UInt8, data.userData.size());
    add("scanAngle", PointAttributeType::Int16, data.scanAngle.size());
    add("pointSourceId", PointAttributeType::UInt16,
        data.pointSourceId.size());
    add("red", PointAttributeType::UInt16, data.red.size());
    add("green", PointAttributeType::UInt16, data.green.size());
    add("blue", PointAttributeType::UInt16, data.blue.size());
    add("nir", PointAttributeType::UInt16, data.nir.size());
    add("gpsTime", PointAttributeType::Float64, data.gpsTime.size());
    add("waveformDescriptorIndex", PointAttributeType::UInt8,
        data.waveformDescriptorIndex.size());
    add("waveformDataOffset", PointAttributeType::UInt64,
        data.waveformDataOffset.size());
    add("waveformPacketSize", PointAttributeType::UInt32,
        data.waveformPacketSize.size());
    add("returnPointWaveformLocation", PointAttributeType::Float32,
        data.returnPointWaveformLocation.size());
    add("waveformXt", PointAttributeType::Float32, data.waveformXt.size());
    add("waveformYt", PointAttributeType::Float32, data.waveformYt.size());
    add("waveformZt", PointAttributeType::Float32, data.waveformZt.size());
    add("waveformDataExternal", PointAttributeType::UInt8,
        data.waveformDataExternal.size());
    const auto extraByteNames = NormalizeExtraByteNames(data.extraByteNames);
    for (std::size_t index = 0; index < data.extraBytes.size(); ++index) {
        if (!data.extraBytes[index].empty() && index < extraByteNames.size()) {
            const auto componentCount =
                data.extraByteComponentCounts.empty()
                    ? std::uint8_t{1}
                    : index < data.extraByteComponentCounts.size()
                          ? data.extraByteComponentCounts[index]
                          : std::uint8_t{0};
            if (componentCount < 1 || componentCount > 3) {
                continue;
            }
            const auto type = componentCount == 1 ? PointAttributeType::Float64
                              : componentCount == 2 ? PointAttributeType::Float64Vec2
                                                    : PointAttributeType::Float64Vec3;
            chunk.attributes.push_back({extraByteNames[index], type});
        }
    }
    return chunk;
}

std::vector<std::string> NormalizeExtraByteNames(
    const std::vector<std::string>& names) {
    std::set<std::string> usedNames = ReservedPointAttributeNames();
    std::vector<std::string> result;
    result.reserve(names.size());
    for (const auto& name : names) {
        const auto baseName = NormalizeExtraByteName(name);
        auto normalizedName = baseName;
        std::size_t suffix = 2;
        while (!usedNames.insert(normalizedName).second) {
            normalizedName = baseName + "_" + std::to_string(suffix++);
        }
        result.push_back(std::move(normalizedName));
    }
    return result;
}

bool PointRange::IsValid() const noexcept {
    return pointCount == 0 ||
           firstPoint <= (std::numeric_limits<std::uint64_t>::max)() -
                              pointCount;
}

bool PointReadOptions::IsValid() const noexcept {
    return chunkPointLimit != 0 && memoryBudgetBytes != 0 &&
           range.IsValid() && (!bounds || bounds->IsValid());
}

} // namespace usdpointcloud