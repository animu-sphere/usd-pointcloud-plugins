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

std::size_t ScalarSize(tinyply::Type type) {
    switch (type) {
    case tinyply::Type::INT8:
    case tinyply::Type::UINT8: return 1;
    case tinyply::Type::INT16:
    case tinyply::Type::UINT16: return 2;
    case tinyply::Type::INT32:
    case tinyply::Type::UINT32:
    case tinyply::Type::FLOAT32: return 4;
    case tinyply::Type::FLOAT64: return 8;
    default: return 0;
    }
}

bool FitsMemoryBudget(const tinyply::PlyElement& vertex,
                      std::size_t memoryBudgetBytes,
                      std::string& error) {
    std::uint64_t bytesPerPoint = 0;
    for (const auto& property : vertex.properties) {
        const auto scalarSize = ScalarSize(property.propertyType);
        if (property.isList || scalarSize == 0 ||
            bytesPerPoint > (std::numeric_limits<std::uint64_t>::max)() -
                                scalarSize - sizeof(double)) {
            error = "PLY vertex payload size cannot be represented";
            return false;
        }
        bytesPerPoint += scalarSize + sizeof(double);
    }
    if (vertex.size != 0 &&
        bytesPerPoint > (std::numeric_limits<std::uint64_t>::max)() /
                            vertex.size) {
        error = "PLY vertex payload size cannot be represented";
        return false;
    }
    const auto requiredBytes = bytesPerPoint * vertex.size;
    if (requiredBytes > memoryBudgetBytes) {
        error = "PLY vertex payload exceeds the configured memory budget";
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

bool HasTypedIntensity(const std::vector<std::string>& propertyNames,
                       const std::vector<std::vector<double>>& propertyValues) {
    const auto intensityIndex = FindPropertyIndex(propertyNames, "intensity");
    if (intensityIndex == propertyNames.size()) {
        return false;
    }
    std::uint16_t converted = 0;
    return std::all_of(propertyValues[intensityIndex].begin(),
                       propertyValues[intensityIndex].end(),
                       [&](double value) {
                           return AssignUnsigned16(value, converted);
                       });
}

std::uint8_t ColorBitDepth(const tinyply::PlyElement& vertex) {
    bool hasColor = false;
    for (const auto& property : vertex.properties) {
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

PlyPointStream::PlyPointStream(
    std::vector<std::string> propertyNames,
    std::vector<std::vector<double>> propertyValues,
    std::uint64_t firstPoint,
    std::uint64_t endPoint,
    bool typedIntensity,
    std::uint8_t colorBitDepth,
    usdpointcloud::PointReadOptions options)
    : propertyNames_(std::move(propertyNames)),
      propertyValues_(std::move(propertyValues)),
      nextPoint_(firstPoint),
      endPoint_(endPoint),
    typedIntensity_(typedIntensity),
    colorBitDepth_(colorBitDepth),
      options_(std::move(options)) {}

PlyPointStream::~PlyPointStream() = default;

usdpointcloud::PointStreamStatus PlyPointStream::ReadNext(
    usdpointcloud::PointChunk& chunk,
    usdpointcloud::PointData& data,
    usdgeo::Diagnostic& diagnostic) {
    chunk = {};
    data = {};
    diagnostic = {};
    const auto xIndex = FindPropertyIndex(propertyNames_, "x");
    const auto yIndex = FindPropertyIndex(propertyNames_, "y");
    const auto zIndex = FindPropertyIndex(propertyNames_, "z");
    const auto classificationIndex =
        FindPropertyIndex(propertyNames_, "classification");
    while (nextPoint_ < endPoint_) {
        if (options_.isCancelled && options_.isCancelled()) {
            diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                          usdgeo::Severity::Error, "PLY read cancelled",
                          std::nullopt, nextPoint_};
            return usdpointcloud::PointStreamStatus::Error;
        }

        const auto count = static_cast<std::size_t>(
            (std::min)(static_cast<std::uint64_t>(options_.chunkPointLimit),
                       endPoint_ - nextPoint_));
        const auto sourceStart = nextPoint_;
        nextPoint_ += count;
        data.positions.reserve(count);
        data.intensity.reserve(count);
        data.classification.reserve(count);
        data.red.reserve(count);
        data.green.reserve(count);
        data.blue.reserve(count);
        data.colorBitDepth = colorBitDepth_;
        for (const auto& name : propertyNames_) {
            if (name != "x" && name != "y" && name != "z" &&
                (name != "intensity" || !typedIntensity_) &&
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
            const usdgeo::Vec3d position{
                propertyValues_[xIndex][sourceIndex],
                propertyValues_[yIndex][sourceIndex],
                propertyValues_[zIndex][sourceIndex]};
            if (!position.IsFinite()) {
                diagnostic = {usdgeo::DiagnosticCode::NonFiniteCoordinate,
                              usdgeo::Severity::Error,
                              "PLY vertex coordinates must be finite",
                              std::nullopt, sourceIndex};
                return usdpointcloud::PointStreamStatus::Error;
            }
            std::uint8_t classification = 0;
            if (classificationIndex != propertyNames_.size() &&
                !AssignUnsigned8(propertyValues_[classificationIndex][sourceIndex],
                                 classification)) {
                diagnostic = {usdgeo::DiagnosticCode::DecodeFailure,
                              usdgeo::Severity::Error,
                              "PLY classification value is outside uint8 range",
                              std::nullopt, sourceIndex};
                return usdpointcloud::PointStreamStatus::Error;
            }
            if (!IsInside(position, options_.bounds) ||
                (!options_.classifications.empty() &&
                 std::find(options_.classifications.begin(),
                           options_.classifications.end(),
                           classification) == options_.classifications.end())) {
                continue;
            }

            data.positions.push_back(position);
            for (std::size_t propertyIndex = 0;
                 propertyIndex < propertyNames_.size(); ++propertyIndex) {
                const auto& name = propertyNames_[propertyIndex];
                const auto value = propertyValues_[propertyIndex][sourceIndex];
                if (name == "x" || name == "y" || name == "z") {
                    continue;
                }
                if (name == "intensity" && typedIntensity_) {
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
        if (!FitsMemoryBudget(*vertex, options.memoryBudgetBytes, error)) {
            AddDiagnostic(diagnostics,
                          usdgeo::DiagnosticCode::InvalidFormatArgument, error);
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
            property->buffer = tinyply::Buffer();
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
        const auto typedIntensity = HasTypedIntensity(propertyNames, propertyValues);
        const auto colorBitDepth = ColorBitDepth(*vertex);
        return std::unique_ptr<PlyPointStream>(new PlyPointStream(
            std::move(propertyNames), std::move(propertyValues), firstPoint,
            endPoint, typedIntensity, colorBitDepth, options));
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
