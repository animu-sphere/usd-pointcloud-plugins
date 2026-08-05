#include "LasInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace usdlas {

bool MatchesReadOptions(const LasPoint& point,
                        const LasReadOptions& options) noexcept {
    if (options.bounds) {
        const auto& bounds = *options.bounds;
        const auto& position = point.sourcePosition;
        if (position.x < bounds.minimum.x || position.x > bounds.maximum.x ||
            position.y < bounds.minimum.y || position.y > bounds.maximum.y ||
            position.z < bounds.minimum.z || position.z > bounds.maximum.z) {
            return false;
        }
    }
    return options.classifications.empty() ||
           std::find(options.classifications.begin(),
                     options.classifications.end(), point.classification) !=
               options.classifications.end();
}

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::string& error) {
    point = {};
    error.clear();
    if (!header.IsValid() ||
        !detail::IsSupportedFormatForVersion(header.versionMinor,
                                             header.pointFormat) ||
        header.pointRecordLength < detail::MinimumRecordLength(header.pointFormat) ||
        record.size() < header.pointRecordLength) {
        error = "LAS point record is invalid or truncated";
        return false;
    }

    const bool modern = header.pointFormat >= 6;
    const auto x = detail::ReadLittle<std::int32_t>(record, 0);
    const auto y = detail::ReadLittle<std::int32_t>(record, 4);
    const auto z = detail::ReadLittle<std::int32_t>(record, 8);
    point.sourcePosition = {x * header.xScale + header.xOffset,
                             y * header.yScale + header.yOffset,
                             z * header.zScale + header.zOffset};
    point.intensity = detail::ReadLittle<std::uint16_t>(record, 12);
    const auto returnFlags = detail::ReadLittle<std::uint8_t>(record, 14);
    point.returnNumber = returnFlags & 0x0f;
    point.numberOfReturns = (returnFlags >> 4) & 0x0f;
    point.classification =
        detail::ReadLittle<std::uint8_t>(record, modern ? 16 : 15);

    if (modern) {
        const auto flags = detail::ReadLittle<std::uint8_t>(record, 15);
        point.classificationFlags = flags & 0x0f;
        point.scannerChannel = (flags >> 4) & 0x03;
        point.scanDirectionFlag = (flags >> 6) & 0x01;
        point.edgeOfFlightLine = (flags >> 7) & 0x01;
        point.userData = detail::ReadLittle<std::uint8_t>(record, 17);
        point.scanAngle = detail::ReadLittle<std::int16_t>(record, 18);
        point.pointSourceId = detail::ReadLittle<std::uint16_t>(record, 20);
        if (header.pointFormat >= 7) {
            point.red = detail::ReadLittle<std::uint16_t>(record, 30);
            point.green = detail::ReadLittle<std::uint16_t>(record, 32);
            point.blue = detail::ReadLittle<std::uint16_t>(record, 34);
            point.hasColor = true;
        }
        if (header.pointFormat == 8 || header.pointFormat == 10) {
            point.nir = detail::ReadLittle<std::uint16_t>(record, 36);
        }
        point.gpsTime = detail::ReadLittle<double>(record, 22);
        point.hasGpsTime = true;
    } else {
        point.scanAngle = static_cast<std::int8_t>(
            detail::ReadLittle<std::uint8_t>(record, 16));
        point.userData = detail::ReadLittle<std::uint8_t>(record, 17);
        point.pointSourceId = detail::ReadLittle<std::uint16_t>(record, 18);
        if (header.pointFormat == 1 || header.pointFormat == 3) {
            point.gpsTime = detail::ReadLittle<double>(record, 20);
            point.hasGpsTime = true;
        }
        if (header.pointFormat == 2 || header.pointFormat == 3) {
            const auto colorOffset = header.pointFormat == 2 ? 20 : 28;
            point.red = detail::ReadLittle<std::uint16_t>(record, colorOffset);
            point.green =
                detail::ReadLittle<std::uint16_t>(record, colorOffset + 2);
            point.blue =
                detail::ReadLittle<std::uint16_t>(record, colorOffset + 4);
            point.hasColor = true;
        }
    }
    if (header.pointFormat == 4 || header.pointFormat == 5 ||
        header.pointFormat == 9 || header.pointFormat == 10) {
        const auto waveformOffset =
            header.pointFormat == 4 ? std::size_t{28}
            : header.pointFormat == 5 ? std::size_t{34}
            : header.pointFormat == 9 ? std::size_t{30}
                                      : std::size_t{38};
        const auto descriptorIndex =
            detail::ReadLittle<std::uint8_t>(record, waveformOffset);
        const auto encodedOffset =
            detail::ReadLittle<std::uint64_t>(record, waveformOffset + 1);
        point.waveform.descriptorIndex = descriptorIndex & 0x7f;
        point.waveform.external = (descriptorIndex & 0x80) != 0;
        point.waveform.dataOffset = encodedOffset;
        point.waveform.packetSize =
            detail::ReadLittle<std::uint32_t>(record, waveformOffset + 9);
        point.waveform.returnPointLocation =
            detail::ReadLittle<float>(record, waveformOffset + 13);
        point.waveform.xt =
            detail::ReadLittle<float>(record, waveformOffset + 17);
        point.waveform.yt =
            detail::ReadLittle<float>(record, waveformOffset + 21);
        point.waveform.zt =
            detail::ReadLittle<float>(record, waveformOffset + 25);
        point.hasWaveform = true;
    }
    const auto extraOffset = detail::MinimumRecordLength(header.pointFormat);
    std::size_t extraBytesOffset = extraOffset;
    point.extraBytes.reserve(header.extraBytes.size());
    for (const auto& descriptor : header.extraBytes) {
        const auto scalarSize = detail::ExtraByteScalarSize(descriptor.dataType);
        const auto componentCount =
            detail::ExtraByteComponentCount(descriptor.dataType);
        if (scalarSize == 0 || componentCount == 0) {
            error = "unsupported LAS Extra Bytes data type";
            return false;
        }
        if (!detail::Has(record, extraBytesOffset,
                         scalarSize * componentCount)) {
            error = "LAS point record Extra Bytes are truncated";
            return false;
        }
        const auto scalarDataType = descriptor.dataType >= 11
                                        ? (descriptor.dataType - 1) % 10 + 1
                                        : descriptor.dataType;
        for (std::uint8_t component = 0; component < componentCount;
             ++component) {
            const auto componentOffset =
                extraBytesOffset + scalarSize * component;
            if (scalarDataType == 7 &&
                detail::ReadLittle<std::uint64_t>(record, componentOffset) >
                    (std::uint64_t{1} << 53)) {
                error =
                    "LAS Extra Bytes integer cannot be represented exactly as double";
                return false;
            }
            if (scalarDataType == 8) {
                const auto rawInteger =
                    detail::ReadLittle<std::int64_t>(record, componentOffset);
                if (rawInteger > (std::int64_t{1} << 53) ||
                    rawInteger < -(std::int64_t{1} << 53)) {
                    error =
                        "LAS Extra Bytes integer cannot be represented exactly as double";
                    return false;
                }
            }
            const auto raw = detail::ReadExtraByteScalar(
                record, componentOffset, descriptor.dataType);
            const auto scale = component == 0 ? descriptor.scale.x
                               : component == 1 ? descriptor.scale.y
                                                : descriptor.scale.z;
            const auto offset = component == 0 ? descriptor.offset.x
                                : component == 1 ? descriptor.offset.y
                                                 : descriptor.offset.z;
            const auto value = raw * scale + offset;
            if (!std::isfinite(value)) {
                error =
                    "decoded LAS point contains a non-finite Extra Bytes value";
                return false;
            }
            point.extraBytes.push_back(value);
        }
        extraBytesOffset += scalarSize * componentCount;
    }
    if (!std::isfinite(point.sourcePosition.x) ||
        !std::isfinite(point.sourcePosition.y) ||
        !std::isfinite(point.sourcePosition.z)) {
        error = "decoded LAS point contains a non-finite coordinate";
        return false;
    }
    return true;
}

bool DecodePoint(const LasHeader& header,
                 const std::vector<std::uint8_t>& record,
                 LasPoint& point,
                 std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    std::string error;
    if (DecodePoint(header, record, point, error)) {
        return true;
    }
    detail::AddErrorDiagnostic(error, diagnostics);
    return false;
}

} // namespace usdlas