#include "usdply/Ply.h"

#include "tinyply.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
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
std::vector<double> CopyValues(tinyply::PlyData& data) {
    std::vector<double> values(data.count);
    const auto* bytes = data.buffer.get_const();
    for (std::size_t index = 0; index < data.count; ++index) {
        T value{};
        std::memcpy(&value, bytes + index * sizeof(T), sizeof(T));
        values[index] = static_cast<double>(value);
    }
    return values;
}

bool CopyValues(tinyply::PlyData& data,
                std::vector<double>& values,
                std::string& error) {
    if (data.isList) {
        error = "PLY vertex list properties are unsupported";
        return false;
    }
    switch (data.t) {
    case tinyply::Type::INT8:
        values = CopyValues<std::int8_t>(data);
        break;
    case tinyply::Type::UINT8:
        values = CopyValues<std::uint8_t>(data);
        break;
    case tinyply::Type::INT16:
        values = CopyValues<std::int16_t>(data);
        break;
    case tinyply::Type::UINT16:
        values = CopyValues<std::uint16_t>(data);
        break;
    case tinyply::Type::INT32:
        values = CopyValues<std::int32_t>(data);
        break;
    case tinyply::Type::UINT32:
        values = CopyValues<std::uint32_t>(data);
        break;
    case tinyply::Type::FLOAT32:
        values = CopyValues<float>(data);
        break;
    case tinyply::Type::FLOAT64:
        values = CopyValues<double>(data);
        break;
    default:
        error = "PLY vertex property has an unsupported scalar type";
        return false;
    }
    return true;
}

void AddDecodeDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                         const std::string& message) {
    AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure, message);
}

bool DetectDeclaredFormat(std::istream& input,
                          PlyFormat& format,
                          std::vector<usdgeo::Diagnostic>& diagnostics) {
    const auto originalPosition = input.tellg();
    input.clear();
    input.seekg(0, std::ios::beg);
    std::string line;
    if (!std::getline(input, line)) {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "PLY header must begin with the ply signature", 0);
        return false;
    }
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
    input.clear();
    input.seekg(originalPosition, std::ios::beg);
    return true;
}

bool AssignUnsigned16(double value, std::uint16_t& target) {
    if (!std::isfinite(value) || value < 0.0 || value > 65535.0 ||
        static_cast<double>(static_cast<std::uint16_t>(value)) != value) {
        return false;
    }
    target = static_cast<std::uint16_t>(value);
    return true;
}

} // namespace

bool InspectHeader(std::istream& input,
                   PlyHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    header = {};
    diagnostics.clear();
    try {
        PlyFormat declaredFormat = PlyFormat::Ascii;
        if (!DetectDeclaredFormat(input, declaredFormat, diagnostics)) {
            return false;
        }
        tinyply::PlyFile file;
        if (!file.parse_header(input)) {
            AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                          "PLY header could not be parsed");
            return false;
        }
        const auto position = input.tellg();
        const auto dataOffset = position < 0
                                    ? std::uint64_t{0}
                                    : static_cast<std::uint64_t>(position);
        std::string error;
        if (!BuildHeader(file, dataOffset, header, error)) {
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

PlyPointStream::PlyPointStream(
    std::vector<std::string> propertyNames,
    std::vector<std::vector<double>> propertyValues,
    std::uint64_t firstPoint,
    std::uint64_t endPoint,
    usdpointcloud::PointReadOptions options)
    : propertyNames_(std::move(propertyNames)),
      propertyValues_(std::move(propertyValues)),
      nextPoint_(firstPoint),
      endPoint_(endPoint),
      options_(std::move(options)) {}

PlyPointStream::~PlyPointStream() = default;

usdpointcloud::PointStreamStatus PlyPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    chunk = {};
    data = {};
    diagnostic = {};
    if (nextPoint_ >= endPoint_) {
        return usdpointcloud::PointStreamStatus::End;
    }

    const auto count = static_cast<std::size_t>(
        (std::min)(static_cast<std::uint64_t>(options_.chunkPointLimit),
                   endPoint_ - nextPoint_));
    data.positions.resize(count);
    const auto addStorage = [&](const std::string& name) {
        if (name == "intensity") data.intensity.resize(count);
        else if (name == "red") data.red.resize(count);
        else if (name == "green") data.green.resize(count);
        else if (name == "blue") data.blue.resize(count);
        else if (name != "x" && name != "y" && name != "z") {
            data.extraByteNames.push_back(name);
            data.extraByteComponentCounts.push_back(1);
            data.extraBytes.emplace_back(count);
        }
    };
    for (const auto& name : propertyNames_) addStorage(name);

    usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
    for (std::size_t index = 0; index < count; ++index) {
        const auto sourceIndex = nextPoint_ + index;
        for (std::size_t propertyIndex = 0;
             propertyIndex < propertyNames_.size(); ++propertyIndex) {
            const auto& name = propertyNames_[propertyIndex];
            const auto value = propertyValues_[propertyIndex][sourceIndex];
            if (name == "x") data.positions[index].x = value;
            else if (name == "y") data.positions[index].y = value;
            else if (name == "z") data.positions[index].z = value;
            else if (name == "intensity" &&
                     !AssignUnsigned16(value, data.intensity[index])) {
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error,
                              "PLY intensity value is outside uint16 range",
                              std::nullopt, sourceIndex};
                return usdpointcloud::PointStreamStatus::Error;
            } else if (name == "red" &&
                       !AssignUnsigned16(value, data.red[index])) {
                return usdpointcloud::PointStreamStatus::Error;
            } else if (name == "green" &&
                       !AssignUnsigned16(value, data.green[index])) {
                return usdpointcloud::PointStreamStatus::Error;
            } else if (name == "blue" &&
                       !AssignUnsigned16(value, data.blue[index])) {
                return usdpointcloud::PointStreamStatus::Error;
            } else if (name != "x" && name != "y" && name != "z" &&
                       name != "intensity" && name != "red" &&
                       name != "green" && name != "blue") {
                const auto extraIndex = static_cast<std::size_t>(
                    std::find(data.extraByteNames.begin(),
                              data.extraByteNames.end(), name) -
                    data.extraByteNames.begin());
                data.extraBytes[extraIndex][index] = value;
            }
        }
        if (!data.positions[index].IsFinite()) {
            diagnostic = {usdgeo::DiagnosticCode::NonFiniteCoordinate,
                          usdgeo::Severity::Error,
                          "PLY vertex coordinates must be finite", std::nullopt,
                          sourceIndex};
            return usdpointcloud::PointStreamStatus::Error;
        }
        bounds.Expand(data.positions[index]);
    }
    nextPoint_ += count;
    chunk = usdpointcloud::MakePointChunk(data, bounds);
    return usdpointcloud::PointStreamStatus::Chunk;
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
        PlyFormat declaredFormat = PlyFormat::Ascii;
        if (!DetectDeclaredFormat(stream, declaredFormat, diagnostics)) {
            return nullptr;
        }
        tinyply::PlyFile file;
        if (!file.parse_header(stream)) {
            AddDecodeDiagnostic(diagnostics, "PLY header could not be parsed");
            return nullptr;
        }
        const auto position = stream.tellg();
        const auto dataOffset = position < 0
                                    ? std::uint64_t{0}
                                    : static_cast<std::uint64_t>(position);
        std::string error;
        if (!BuildHeader(file, dataOffset, header, error)) {
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
        if (names.find("x") == names.end() || names.find("y") == names.end() ||
            names.find("z") == names.end()) {
            AddDecodeDiagnostic(diagnostics,
                                "PLY vertex element must contain x, y, and z");
            return nullptr;
        }

        std::map<std::string, std::shared_ptr<tinyply::PlyData>> requested;
        for (const auto& name : propertyNames) {
            requested.emplace(
                name, file.request_properties_from_element("vertex", {name}));
        }
        file.read(stream);
        if (stream.fail() || stream.bad()) {
            AddDecodeDiagnostic(diagnostics,
                                "PLY payload is truncated or unreadable");
            return nullptr;
        }
        std::vector<std::vector<double>> propertyValues;
        propertyValues.reserve(propertyNames.size());
        for (const auto& name : propertyNames) {
            const auto& property = requested.at(name);
            std::vector<double> values;
            if (!property || !CopyValues(*property, values, error) ||
                values.size() != vertex->size) {
                AddDecodeDiagnostic(diagnostics,
                                    error.empty() ? "PLY property count mismatch"
                                                  : error);
                return nullptr;
            }
            propertyValues.push_back(std::move(values));
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
        return std::unique_ptr<PlyPointStream>(new PlyPointStream(
            std::move(propertyNames), std::move(propertyValues), firstPoint,
            endPoint, options));
    } catch (const std::exception& exception) {
        AddDecodeDiagnostic(diagnostics,
                            std::string("PLY payload read failed: ") +
                                exception.what());
        return nullptr;
    }
}

} // namespace usdply
