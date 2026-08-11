#include "usdply/Ply.h"

#include "tinyply.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

namespace usdply {
namespace {

void AddDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                   usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::optional<std::uint64_t> byteOffset = std::nullopt) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, byteOffset, std::nullopt});
}

PlyScalarType ConvertType(tinyply::Type type) {
    switch (type) {
    case tinyply::Type::INT8: return PlyScalarType::Int8;
    case tinyply::Type::UINT8: return PlyScalarType::UInt8;
    case tinyply::Type::INT16: return PlyScalarType::Int16;
    case tinyply::Type::UINT16: return PlyScalarType::UInt16;
    case tinyply::Type::INT32: return PlyScalarType::Int32;
    case tinyply::Type::UINT32: return PlyScalarType::UInt32;
    case tinyply::Type::FLOAT32: return PlyScalarType::Float32;
    case tinyply::Type::FLOAT64: return PlyScalarType::Float64;
    default: return PlyScalarType::Float32;
    }
}

const tinyply::PlyElement* FindVertex(
    const std::vector<tinyply::PlyElement>& elements,
    std::string& error) {
    const tinyply::PlyElement* vertex = nullptr;
    for (const auto& element : elements) {
        if (element.name != "vertex") {
            continue;
        }
        if (vertex) {
            error = "PLY header contains duplicate vertex elements";
            return nullptr;
        }
        vertex = &element;
    }
    if (!vertex) {
        error = "PLY header does not contain a vertex element";
    }
    return vertex;
}

bool BuildHeader(const tinyply::PlyFile& file,
                 std::uint64_t dataOffset,
                 PlyHeader& header,
                 std::string& error) {
    header = {};
    header.format = file.is_binary_file() ? PlyFormat::BinaryLittleEndian
                                          : PlyFormat::Ascii;
    for (const auto& element : file.get_elements()) {
        PlyElement result;
        result.name = element.name;
        result.count = element.size;
        for (const auto& property : element.properties) {
            if (property.propertyType == tinyply::Type::INVALID ||
                (property.isList &&
                 property.listType == tinyply::Type::INVALID)) {
                error = "PLY header contains an unsupported property type";
                header = {};
                return false;
            }
            result.properties.push_back(
                {property.name, ConvertType(property.propertyType),
                 property.isList, ConvertType(property.listType)});
        }
        header.elements.push_back(std::move(result));
    }
    header.dataOffset = dataOffset;
    if (header.elements.empty()) {
        error = "PLY header does not contain any elements";
        header = {};
        return false;
    }
    return true;
}

template <typename T>
void AppendValues(const std::vector<T>& source, std::vector<T>& target) {
    target.insert(target.end(), source.begin(), source.end());
}

bool AppendPointData(const usdpointcloud::PointData& source,
                     usdpointcloud::PointData& target,
                     std::string& error) {
    if (target.positions.empty()) {
        target.colorBitDepth = source.colorBitDepth;
        target.extraByteNames = source.extraByteNames;
        target.extraByteComponentCounts = source.extraByteComponentCounts;
        target.extraBytes.resize(source.extraBytes.size());
    } else if (target.colorBitDepth != source.colorBitDepth ||
               target.extraByteNames != source.extraByteNames ||
               target.extraByteComponentCounts != source.extraByteComponentCounts ||
               target.extraBytes.size() != source.extraBytes.size()) {
        error = "PLY chunks contain inconsistent extra properties";
        return false;
    }
    AppendValues(source.positions, target.positions);
    AppendValues(source.intensity, target.intensity);
    AppendValues(source.classification, target.classification);
    AppendValues(source.red, target.red);
    AppendValues(source.green, target.green);
    AppendValues(source.blue, target.blue);
    for (std::size_t index = 0; index < source.extraBytes.size(); ++index) {
        AppendValues(source.extraBytes[index], target.extraBytes[index]);
    }
    return true;
}

void AddDecodeDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                         const std::string& message) {
    AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure, message);
}

bool ReadHeaderText(std::istream& input,
                    std::string& headerText,
                    PlyFormat& format,
                    std::vector<usdgeo::Diagnostic>& diagnostics) {
    headerText.clear();
    std::string line;
    if (!std::getline(input, line)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "PLY header must begin with the ply signature", 0);
        return false;
    }
    headerText = line;
    headerText.push_back('\n');
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line != "ply") {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "PLY header must begin with the ply signature", 0);
        return false;
    }
    bool sawFormat = false;
    bool sawEndHeader = false;
    while (std::getline(input, line)) {
        headerText += line;
        headerText.push_back('\n');
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream fields(line);
        std::string keyword;
        fields >> keyword;
        if (keyword == "end_header") {
            sawEndHeader = true;
            break;
        }
        if (keyword == "format") {
            std::string encoding;
            std::string version;
            if (sawFormat || !(fields >> encoding >> version) ||
                version != "1.0") {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::UnsupportedVersion,
                              "PLY format declaration is invalid");
                return false;
            }
            if (encoding == "ascii") format = PlyFormat::Ascii;
            else if (encoding == "binary_little_endian") {
                format = PlyFormat::BinaryLittleEndian;
            } else if (encoding == "binary_big_endian") {
                format = PlyFormat::BinaryBigEndian;
            } else {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::UnsupportedVersion,
                              "PLY format encoding is unsupported");
                return false;
            }
            sawFormat = true;
        }
    }
    if (!sawEndHeader) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::TruncatedHeader,
                      "PLY header ended before end_header");
        return false;
    }
    return true;
}

std::size_t FindPropertyIndex(const std::vector<std::string>& names,
                              const std::string& name) {
    const auto found = std::find(names.begin(), names.end(), name);
    return found == names.end() ? names.size()
                                : static_cast<std::size_t>(found - names.begin());
}

bool AssignUnsigned8(double value, std::uint8_t& target) {
    if (!std::isfinite(value) || value < 0.0 || value > 255.0 ||
        static_cast<double>(static_cast<std::uint8_t>(value)) != value) {
        return false;
    }
    target = static_cast<std::uint8_t>(value);
    return true;
}

bool IsInside(const usdgeo::Vec3d& point,
              const std::optional<usdgeo::SpatialBounds>& bounds) {
    return !bounds ||
           (point.x >= bounds->minimum.x && point.x <= bounds->maximum.x &&
            point.y >= bounds->minimum.y && point.y <= bounds->maximum.y &&
            point.z >= bounds->minimum.z && point.z <= bounds->maximum.z);
}

bool AssignUnsigned16(double value, std::uint16_t& target) {
    if (!std::isfinite(value) || value < 0.0 || value > 65535.0 ||
        static_cast<double>(static_cast<std::uint16_t>(value)) != value) {
        return false;
    }
    target = static_cast<std::uint16_t>(value);
    return true;
}

std::uint8_t ColorBitDepth(const std::vector<tinyply::PlyProperty>& properties) {
    bool hasColor = false;
    for (const auto& property : properties) {
        if (property.name != "red" && property.name != "green" &&
            property.name != "blue") {
            continue;
        }
        hasColor = true;
        if (property.propertyType != tinyply::Type::UINT8) {
            return 16;
        }
    }
    return hasColor ? 8 : 16;
}

bool HasMixedColorBitDepth(
    const std::vector<tinyply::PlyProperty>& properties) {
    bool hasEightBit = false;
    bool hasSixteenBit = false;
    for (const auto& property : properties) {
        if (property.name != "red" && property.name != "green" &&
            property.name != "blue") {
            continue;
        }
        if (property.propertyType == tinyply::Type::UINT8) {
            hasEightBit = true;
        } else {
            hasSixteenBit = true;
        }
    }
    return hasEightBit && hasSixteenBit;
}

struct StreamProperty {
    std::string name;
    PlyScalarType type = PlyScalarType::Float32;
};

std::size_t ScalarSize(PlyScalarType type) {
    switch (type) {
    case PlyScalarType::Int8:
    case PlyScalarType::UInt8: return 1;
    case PlyScalarType::Int16:
    case PlyScalarType::UInt16: return 2;
    case PlyScalarType::Int32:
    case PlyScalarType::UInt32:
    case PlyScalarType::Float32: return 4;
    case PlyScalarType::Float64: return 8;
    }
    return 0;
}

bool IsTypedIntensity(PlyScalarType type) {
    return type == PlyScalarType::Int8 || type == PlyScalarType::UInt8 ||
           type == PlyScalarType::Int16 || type == PlyScalarType::UInt16;
}

bool ParseAsciiScalar(std::istream& input,
                      PlyScalarType type,
                      double& value) {
    std::string token;
    if (!(input >> token)) {
        return false;
    }
    char* end = nullptr;
    if (type == PlyScalarType::Float32 || type == PlyScalarType::Float64) {
        value = std::strtod(token.c_str(), &end);
        return end != token.c_str() && *end == '\0';
    }
    if (type == PlyScalarType::Int8 || type == PlyScalarType::Int16 ||
        type == PlyScalarType::Int32) {
        const auto parsed = std::strtoll(token.c_str(), &end, 10);
        if (end == token.c_str() || *end != '\0') {
            return false;
        }
        value = static_cast<double>(parsed);
        return true;
    }
    const auto parsed = std::strtoull(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<double>(parsed);
    return true;
}

bool IsValidScalarValue(PlyScalarType type, double value) {
    if (!std::isfinite(value)) {
        return false;
    }
    switch (type) {
    case PlyScalarType::Int8:
        return value >= (std::numeric_limits<std::int8_t>::min)() &&
               value <= (std::numeric_limits<std::int8_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::UInt8:
        return value >= 0.0 &&
               value <= (std::numeric_limits<std::uint8_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::Int16:
        return value >= (std::numeric_limits<std::int16_t>::min)() &&
               value <= (std::numeric_limits<std::int16_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::UInt16:
        return value >= 0.0 &&
               value <= (std::numeric_limits<std::uint16_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::Int32:
        return value >= (std::numeric_limits<std::int32_t>::min)() &&
               value <= (std::numeric_limits<std::int32_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::UInt32:
        return value >= 0.0 &&
               value <= (std::numeric_limits<std::uint32_t>::max)() &&
               std::floor(value) == value;
    case PlyScalarType::Float32:
        return std::abs(value) <=
               static_cast<double>((std::numeric_limits<float>::max)());
    case PlyScalarType::Float64:
        return true;
    }
    return false;
}

bool ReadBinaryScalar(std::istream& input,
                      PlyScalarType type,
                      bool bigEndian,
                      double& value) {
    const auto size = ScalarSize(type);
    std::uint8_t bytes[sizeof(double)]{};
    input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size));
    if (!input) {
        return false;
    }
    std::uint64_t bits = 0;
    if (bigEndian) {
        for (std::size_t index = 0; index < size; ++index) {
            bits = (bits << 8) | bytes[index];
        }
    } else {
        for (std::size_t index = 0; index < size; ++index) {
            bits |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
        }
    }
    switch (type) {
    case PlyScalarType::Int8: value = static_cast<double>(static_cast<std::int8_t>(bits)); break;
    case PlyScalarType::UInt8: value = static_cast<double>(static_cast<std::uint8_t>(bits)); break;
    case PlyScalarType::Int16: value = static_cast<double>(static_cast<std::int16_t>(bits)); break;
    case PlyScalarType::UInt16: value = static_cast<double>(static_cast<std::uint16_t>(bits)); break;
    case PlyScalarType::Int32: value = static_cast<double>(static_cast<std::int32_t>(bits)); break;
    case PlyScalarType::UInt32: value = static_cast<double>(static_cast<std::uint32_t>(bits)); break;
    case PlyScalarType::Float32: {
        const auto narrowed = static_cast<std::uint32_t>(bits);
        float converted = 0.0f;
        std::memcpy(&converted, &narrowed, sizeof(converted));
        value = converted;
        break;
    }
    case PlyScalarType::Float64: {
        std::memcpy(&value, &bits, sizeof(value));
        break;
    }
    }
    return true;
}

bool ReadScalar(std::istream& input,
                PlyFormat format,
                PlyScalarType type,
                double& value) {
    return format == PlyFormat::Ascii
               ? ParseAsciiScalar(input, type, value)
               : ReadBinaryScalar(input, type,
                                  format == PlyFormat::BinaryBigEndian, value);
}

bool ReadListCount(std::istream& input,
                   PlyFormat format,
                   PlyScalarType type,
                   std::uint64_t& count) {
    double value = 0.0;
    if (!ReadScalar(input, format, type, value) ||
        !IsValidScalarValue(type, value) ||
        value < 0.0 || value > static_cast<double>((std::numeric_limits<std::uint64_t>::max)()) ||
        std::floor(value) != value) {
        return false;
    }
    count = static_cast<std::uint64_t>(value);
    return true;
}

bool SkipProperties(std::istream& input,
                    PlyFormat format,
                    const std::vector<tinyply::PlyProperty>& properties) {
    for (const auto& property : properties) {
        if (!property.isList) {
            double value = 0.0;
            const auto type = ConvertType(property.propertyType);
            if (!ReadScalar(input, format, type, value) ||
                !IsValidScalarValue(type, value)) {
                return false;
            }
            continue;
        }
        std::uint64_t count = 0;
        if (!ReadListCount(input, format, ConvertType(property.listType), count)) {
            return false;
        }
        for (std::uint64_t index = 0; index < count; ++index) {
            double value = 0.0;
                const auto type = ConvertType(property.propertyType);
                if (!ReadScalar(input, format, type, value) ||
                    !IsValidScalarValue(type, value)) {
                return false;
            }
        }
    }
    return true;
}

bool SkipElement(std::istream& input,
                 PlyFormat format,
                 const tinyply::PlyElement& element) {
    for (std::size_t row = 0; row < element.size; ++row) {
        if (!SkipProperties(input, format, element.properties)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool InspectHeader(std::istream& input,
                   PlyHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    header = {};
    diagnostics.clear();
    try {
        std::string headerText;
        PlyFormat declaredFormat = PlyFormat::Ascii;
        if (!ReadHeaderText(input, headerText, declaredFormat, diagnostics)) {
            return false;
        }
        std::istringstream headerInput(headerText);
        tinyply::PlyFile file;
        if (!file.parse_header(headerInput)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "PLY header could not be parsed");
            return false;
        }
        std::string error;
        if (!BuildHeader(file, headerText.size(), header, error)) {
            AddDecodeDiagnostic(diagnostics, error);
            return false;
        }
        header.format = declaredFormat;
        return true;
    } catch (const std::exception& exception) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      std::string("PLY header parse failed: ") + exception.what());
        return false;
    }
}

struct PlyPointStream::Impl {
        std::ifstream input;
        PlyFormat format = PlyFormat::Ascii;
        std::vector<StreamProperty> properties;
        std::uint64_t nextPoint = 0;
        std::uint64_t endPoint = 0;
        bool typedIntensity = false;
        std::uint8_t colorBitDepth = 16;
        usdpointcloud::PointReadOptions options;
};

PlyPointStream::PlyPointStream(std::unique_ptr<Impl> impl)
        : impl_(std::move(impl)) {}

PlyPointStream::~PlyPointStream() = default;

usdpointcloud::PointStreamStatus PlyPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    chunk = {};
    data = {};
    diagnostic = {};
    std::vector<std::string> propertyNames;
    propertyNames.reserve(impl_->properties.size());
    for (const auto& property : impl_->properties) {
        propertyNames.push_back(property.name);
    }
    const auto xIndex = FindPropertyIndex(propertyNames, "x");
    const auto yIndex = FindPropertyIndex(propertyNames, "y");
    const auto zIndex = FindPropertyIndex(propertyNames, "z");
    const auto classificationIndex =
        FindPropertyIndex(propertyNames, "classification");
    while (impl_->nextPoint < impl_->endPoint) {
        if (impl_->options.isCancelled && impl_->options.isCancelled()) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, "PLY read cancelled",
                          std::nullopt, impl_->nextPoint};
            return usdpointcloud::PointStreamStatus::Error;
        }

        const auto count = static_cast<std::size_t>(
            (std::min)(static_cast<std::uint64_t>(impl_->options.chunkPointLimit),
                       impl_->endPoint - impl_->nextPoint));
        const auto sourceStart = impl_->nextPoint;
        impl_->nextPoint += count;
        data.positions.reserve(count);
        data.intensity.reserve(count);
        data.classification.reserve(count);
        data.red.reserve(count);
        data.green.reserve(count);
        data.blue.reserve(count);
        data.colorBitDepth = impl_->colorBitDepth;
        for (const auto& name : propertyNames) {
            if (name != "x" && name != "y" && name != "z" &&
                (name != "intensity" || !impl_->typedIntensity) &&
                name != "classification" &&
                name != "red" && name != "green" && name != "blue") {
                data.extraByteNames.push_back(name);
                data.extraByteComponentCounts.push_back(1);
                data.extraBytes.emplace_back();
                data.extraBytes.back().reserve(count);
            }
        }

        usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
        for (std::size_t index = 0; index < count; ++index) {
            const auto sourceIndex = sourceStart + index;
            std::vector<double> values(impl_->properties.size());
            for (std::size_t propertyIndex = 0;
                 propertyIndex < impl_->properties.size(); ++propertyIndex) {
                if (!ReadScalar(impl_->input, impl_->format,
                                impl_->properties[propertyIndex].type,
                                values[propertyIndex])) {
                    diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                                  usdgeo::Severity::Error,
                                  "PLY payload is truncated or unreadable",
                                  std::nullopt, sourceIndex};
                    return usdpointcloud::PointStreamStatus::Error;
                }
            }
            for (std::size_t propertyIndex = 0;
                 propertyIndex < impl_->properties.size(); ++propertyIndex) {
                const auto& property = impl_->properties[propertyIndex];
                if (property.name == "x" || property.name == "y" ||
                    property.name == "z") {
                    if (!std::isfinite(values[propertyIndex])) {
                        continue;
                    }
                }
                if (!IsValidScalarValue(property.type, values[propertyIndex])) {
                    diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                                  usdgeo::Severity::Error,
                                  "PLY " + property.name +
                                      " value is outside its declared scalar range",
                                  std::nullopt, sourceIndex};
                    return usdpointcloud::PointStreamStatus::Error;
                }
            }
            const usdgeo::Vec3d position{
                values[xIndex], values[yIndex], values[zIndex]};
            if (!position.IsFinite()) {
                diagnostic = {usdgeo::DiagnosticCode::NonFiniteCoordinate,
                              usdgeo::Severity::Error,
                              "PLY vertex coordinates must be finite",
                              std::nullopt, sourceIndex};
                return usdpointcloud::PointStreamStatus::Error;
            }
            std::uint8_t classification = 0;
            if (classificationIndex != propertyNames.size() &&
                !AssignUnsigned8(values[classificationIndex], classification)) {
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error,
                              "PLY classification value is outside uint8 range",
                              std::nullopt, sourceIndex};
                return usdpointcloud::PointStreamStatus::Error;
            }
            if (!IsInside(position, impl_->options.bounds) ||
                (!impl_->options.classifications.empty() &&
                 std::find(impl_->options.classifications.begin(),
                           impl_->options.classifications.end(),
                           classification) ==
                       impl_->options.classifications.end())) {
                continue;
            }

            data.positions.push_back(position);
            for (std::size_t propertyIndex = 0;
                 propertyIndex < propertyNames.size(); ++propertyIndex) {
                const auto& name = propertyNames[propertyIndex];
                const auto value = values[propertyIndex];
                if (name == "x" || name == "y" || name == "z") {
                    continue;
                }
                if (name == "intensity" && impl_->typedIntensity) {
                    std::uint16_t converted = 0;
                    if (!AssignUnsigned16(value, converted)) {
                        diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                                      usdgeo::Severity::Error,
                                      "PLY intensity value is outside uint16 range",
                                      std::nullopt, sourceIndex};
                        return usdpointcloud::PointStreamStatus::Error;
                    }
                    data.intensity.push_back(converted);
                } else if (name == "classification") {
                    data.classification.push_back(classification);
                } else if (name == "red" || name == "green" ||
                           name == "blue") {
                    std::uint16_t converted = 0;
                    if (!AssignUnsigned16(value, converted)) {
                        diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                                      usdgeo::Severity::Error,
                                      "PLY " + name +
                                          " value is outside uint16 range",
                                      std::nullopt, sourceIndex};
                        return usdpointcloud::PointStreamStatus::Error;
                    }
                    if (name == "red") data.red.push_back(converted);
                    else if (name == "green") data.green.push_back(converted);
                    else data.blue.push_back(converted);
                } else {
                    const auto extraIndex = static_cast<std::size_t>(
                        std::find(data.extraByteNames.begin(),
                                  data.extraByteNames.end(), name) -
                        data.extraByteNames.begin());
                    data.extraBytes[extraIndex].push_back(value);
                }
            }
            bounds.Expand(position);
        }
        if (data.positions.empty()) {
            data = {};
            continue;
        }
        chunk = usdpointcloud::MakePointChunk(data, bounds);
        if (!chunk.IsValid()) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error,
                          "PLY stream produced an invalid point chunk",
                          std::nullopt, sourceStart};
            return usdpointcloud::PointStreamStatus::Error;
        }
        return usdpointcloud::PointStreamStatus::Chunk;
    }
    return usdpointcloud::PointStreamStatus::End;
}

std::unique_ptr<PlyPointStream> OpenPointStream(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    PlyHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    diagnostics.clear();
    header = {};
    if (!options.IsValid()) {
        AddDecodeDiagnostic(diagnostics, "PLY read options are invalid");
        return nullptr;
    }

    std::ifstream stream(filename, std::ios::binary);
    if (!stream) {
        AddDecodeDiagnostic(diagnostics, "could not open PLY file: " + filename);
        return nullptr;
    }
    try {
        std::string headerText;
        PlyFormat declaredFormat = PlyFormat::Ascii;
        if (!ReadHeaderText(stream, headerText, declaredFormat, diagnostics)) {
            return nullptr;
        }
        std::istringstream headerInput(headerText);
        tinyply::PlyFile file;
        if (!file.parse_header(headerInput)) {
            AddDecodeDiagnostic(diagnostics, "PLY header could not be parsed");
            return nullptr;
        }
        std::string error;
        if (!BuildHeader(file, headerText.size(), header, error)) {
            AddDecodeDiagnostic(diagnostics, error);
            return nullptr;
        }
        header.format = declaredFormat;
        const auto elements = file.get_elements();
        const auto* vertex = FindVertex(elements, error);
        if (!vertex) {
            AddDecodeDiagnostic(diagnostics, error);
            return nullptr;
        }
        std::set<std::string> names;
        std::vector<std::string> propertyNames;
        for (const auto& property : vertex->properties) {
            if (property.isList) {
                AddDecodeDiagnostic(diagnostics,
                                    "PLY vertex list properties are unsupported");
                return nullptr;
            }
            if (!names.insert(property.name).second) {
                AddDecodeDiagnostic(diagnostics,
                                    "PLY vertex property is duplicated: " +
                                        property.name);
                return nullptr;
            }
            propertyNames.push_back(property.name);
        }
        if (HasMixedColorBitDepth(vertex->properties)) {
            AddDecodeDiagnostic(
                diagnostics,
                "PLY red, green, and blue properties must use a consistent bit depth");
            return nullptr;
        }
        if (names.find("x") == names.end() || names.find("y") == names.end() ||
            names.find("z") == names.end()) {
            AddDecodeDiagnostic(diagnostics,
                                "PLY vertex element must contain x, y, and z");
            return nullptr;
        }
        if (!options.classifications.empty() &&
            names.find("classification") == names.end()) {
            AddDiagnostic(
                diagnostics, usdgeo::DiagnosticCode::InvalidFormatArgument,
                "PLY classification filter requires a classification property");
            return nullptr;
        }
        const auto firstPoint = options.range.firstPoint;
        if (firstPoint > vertex->size ||
            options.range.pointCount > vertex->size - firstPoint) {
            AddDecodeDiagnostic(diagnostics, "PLY point range is outside vertex data");
            return nullptr;
        }
        const auto endPoint = options.range.pointCount == 0
                                  ? vertex->size
                                  : firstPoint + options.range.pointCount;
        std::uint64_t bytesPerPoint = sizeof(usdgeo::Vec3d) +
                                      propertyNames.size() * sizeof(double);
        if (bytesPerPoint == 0 ||
            bytesPerPoint > options.memoryBudgetBytes) {
            AddDiagnostic(diagnostics,
                          usdgeo::DiagnosticCode::InvalidFormatArgument,
                          "PLY chunk cannot fit in the configured memory budget");
            return nullptr;
        }
        auto impl = std::make_unique<PlyPointStream::Impl>();
        impl->format = declaredFormat;
        impl->input = std::move(stream);
        impl->nextPoint = firstPoint;
        impl->endPoint = endPoint;
        impl->typedIntensity = false;
        impl->colorBitDepth = ColorBitDepth(vertex->properties);
        impl->options = options;
        const auto budgetChunkLimit = static_cast<std::size_t>(
            options.memoryBudgetBytes / bytesPerPoint);
        impl->options.chunkPointLimit =
            (std::min)(impl->options.chunkPointLimit, budgetChunkLimit);
        for (const auto& property : vertex->properties) {
            impl->properties.push_back(
                {property.name, ConvertType(property.propertyType)});
            if (property.name == "intensity") {
                impl->typedIntensity = IsTypedIntensity(
                    ConvertType(property.propertyType));
            }
        }
        const auto vertexIndex = static_cast<std::size_t>(
            std::find_if(elements.begin(), elements.end(),
                         [](const tinyply::PlyElement& element) {
                             return element.name == "vertex";
                         }) - elements.begin());
        for (std::size_t index = 0; index < vertexIndex; ++index) {
            if (!SkipElement(impl->input, declaredFormat, elements[index])) {
                AddDecodeDiagnostic(diagnostics,
                                    "PLY payload is truncated or unreadable");
                return nullptr;
            }
        }
        for (std::uint64_t index = 0; index < firstPoint; ++index) {
            if (!SkipProperties(impl->input, declaredFormat,
                                vertex->properties)) {
                AddDecodeDiagnostic(diagnostics,
                                    "PLY payload is truncated or unreadable");
                return nullptr;
            }
        }
        return std::unique_ptr<PlyPointStream>(
            new PlyPointStream(std::move(impl)));
    } catch (const std::exception& exception) {
        AddDecodeDiagnostic(diagnostics,
                            std::string("PLY payload read failed: ") +
                                exception.what());
        return nullptr;
    }
}

bool ReadPointCloud(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    const usdgeo::GeoReference& reference,
    usdpointcloud::PointCloudAsset& asset,
    std::vector<usdgeo::Diagnostic>& diagnostics) {
    asset = {};
    diagnostics.clear();
    if (!reference.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidCrs,
                      "PLY point cloud requires a valid CRS argument");
        return false;
    }

    PlyHeader header;
    auto stream = OpenPointStream(filename, options, header, diagnostics);
    if (!stream) {
        return false;
    }

    usdpointcloud::PointData data;
    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    usdpointcloud::PointChunk chunk;
    usdgeo::Diagnostic diagnostic;
    while (true) {
        const auto status = stream->ReadNext(chunk, data, diagnostic);
        if (status == usdpointcloud::PointStreamStatus::End) {
            break;
        }
        if (status == usdpointcloud::PointStreamStatus::Error) {
            diagnostics.push_back(diagnostic);
            return false;
        }

        usdpointcloud::PointData localData = data;
        for (auto& position : localData.positions) {
            usdgeo::Vec3d local;
            if (!reference.TryToLocal(position, local)) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidCrs,
                              "PLY coordinate could not be transformed");
                return false;
            }
            position = local;
            bounds.Expand(local);
        }
        std::string appendError;
        if (!AppendPointData(localData, asset.data, appendError)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          appendError);
            return false;
        }
    }
    if (asset.data.positions.empty() || !bounds.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "PLY point cloud contains no selected points");
        return false;
    }
    asset.reference = reference;
    asset.bounds = bounds;
    asset.chunk = usdpointcloud::MakePointChunk(asset.data, bounds);
    if (!asset.IsValid()) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "PLY point cloud asset is invalid");
        asset = {};
        return false;
    }
    return true;
}

} // namespace usdply
