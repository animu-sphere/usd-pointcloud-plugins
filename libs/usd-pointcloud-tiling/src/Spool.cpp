#include "usdpointcloud/Spool.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <type_traits>

namespace usdpointcloud {
namespace {

constexpr char kHeader[] = "USDGSP01";
constexpr char kFooter[] = "USDGEND1";
constexpr std::streamoff kFooterSize =
    static_cast<std::streamoff>(sizeof(kFooter) - 1 + sizeof(std::uint64_t));

void Error(std::vector<usdgeo::Diagnostic>& diagnostics,
           usdgeo::DiagnosticCode code,
           const char* message) {
    diagnostics.push_back({code, usdgeo::Severity::Error, message});
}

template <typename T>
bool Write(std::ofstream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return static_cast<bool>(stream);
}

template <typename T>
bool Read(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return static_cast<bool>(stream);
}

bool WriteString(std::ofstream& stream, const std::string& value) {
    if (value.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    const auto size = static_cast<std::uint32_t>(value.size());
    return Write(stream, size) &&
           static_cast<bool>(stream.write(value.data(), size));
}

bool ReadString(std::ifstream& stream, std::string& value) {
    std::uint32_t size = 0;
    if (!Read(stream, size) || size > 1024 * 1024) {
        return false;
    }
    value.resize(size);
    return static_cast<bool>(stream.read(value.data(), size));
}

template <typename T>
bool WriteAttribute(std::ofstream& stream, const SpoolAttributeValue& value) {
    if (!std::holds_alternative<T>(value)) {
        return false;
    }
    return Write(stream, std::get<T>(value));
}

bool WriteAttribute(std::ofstream& stream,
                    PointAttributeType type,
                    const SpoolAttributeValue& value) {
    switch (type) {
    case PointAttributeType::Int32:
        return WriteAttribute<std::int32_t>(stream, value);
    case PointAttributeType::Int16:
        return WriteAttribute<std::int16_t>(stream, value);
    case PointAttributeType::UInt8:
        return WriteAttribute<std::uint8_t>(stream, value);
    case PointAttributeType::UInt16:
        return WriteAttribute<std::uint16_t>(stream, value);
    case PointAttributeType::UInt32:
        return WriteAttribute<std::uint32_t>(stream, value);
    case PointAttributeType::UInt64:
        return WriteAttribute<std::uint64_t>(stream, value);
    case PointAttributeType::Float32:
        return WriteAttribute<float>(stream, value);
    case PointAttributeType::Float64:
        return WriteAttribute<double>(stream, value);
    case PointAttributeType::Float64Vec2:
        return WriteAttribute<std::array<double, 2>>(stream, value);
    case PointAttributeType::Float64Vec3:
        return WriteAttribute<std::array<double, 3>>(stream, value);
    }
    return false;
}

bool ReadAttribute(std::ifstream& stream,
                   PointAttributeType type,
                   SpoolAttributeValue& value) {
    switch (type) {
    case PointAttributeType::Int32: {
        std::int32_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::Int16: {
        std::int16_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::UInt8: {
        std::uint8_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::UInt16: {
        std::uint16_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::UInt32: {
        std::uint32_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::UInt64: {
        std::uint64_t item = 0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::Float32: {
        float item = 0.0F;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::Float64: {
        double item = 0.0;
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::Float64Vec2: {
        std::array<double, 2> item{};
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    case PointAttributeType::Float64Vec3: {
        std::array<double, 3> item{};
        if (!Read(stream, item)) return false;
        value = item;
        return true;
    }
    }
    return false;
}

std::size_t AttributeSize(PointAttributeType type) {
    switch (type) {
    case PointAttributeType::Int32: return sizeof(std::int32_t);
    case PointAttributeType::Int16: return sizeof(std::int16_t);
    case PointAttributeType::UInt8: return sizeof(std::uint8_t);
    case PointAttributeType::UInt16: return sizeof(std::uint16_t);
    case PointAttributeType::UInt32: return sizeof(std::uint32_t);
    case PointAttributeType::UInt64: return sizeof(std::uint64_t);
    case PointAttributeType::Float32: return sizeof(float);
    case PointAttributeType::Float64: return sizeof(double);
    case PointAttributeType::Float64Vec2: return sizeof(std::array<double, 2>);
    case PointAttributeType::Float64Vec3: return sizeof(std::array<double, 3>);
    }
    return 0;
}

} // namespace

class TileSpoolWriter::Impl {
public:
    std::ofstream stream;
    SpoolSchema schema;
    std::size_t memoryLimit = 0;
    std::size_t buffered = 0;
    std::uint64_t pointCount = 0;
    std::vector<SpoolPoint> points;
    bool open = false;
};

class TileSpoolReader::Impl {
public:
    std::ifstream stream;
    SpoolSchema schema;
    std::uint64_t pointCount = 0;
    std::uint64_t pointsRead = 0;
    std::streamoff dataEnd = 0;
    std::size_t recordSize = 0;
    bool complete = false;
};

bool SpoolSchema::IsValid() const noexcept {
    if (version != kPointSpoolSchemaVersion ||
        coordinateSpace != SpoolCoordinateSpace::SourceAndStage ||
        attributes.size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    for (const auto& attribute : attributes) {
        if (!attribute.IsValid() || AttributeSize(attribute.type) == 0) {
            return false;
        }
    }
    return true;
}

TileSpoolWriter::~TileSpoolWriter() {
    if (impl_ && impl_->open) {
        impl_->stream.close();
    }
}

TileSpoolWriter::TileSpoolWriter() = default;

bool TileSpoolWriter::Open(const std::filesystem::path& path,
                           const PointTileId& tile,
                           const SpoolSchema& schema,
                           std::size_t memoryLimitBytes,
                           std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!tile.IsValid() || !schema.IsValid() || memoryLimitBytes == 0) {
        Error(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
              "invalid tile spool configuration");
        return false;
    }
    impl_ = std::make_unique<Impl>();
    impl_->stream.open(path, std::ios::binary | std::ios::trunc);
    if (!impl_->stream) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to create tile spool");
        impl_.reset();
        return false;
    }
    impl_->schema = schema;
    impl_->memoryLimit = memoryLimitBytes;
    impl_->open = true;
    impl_->stream.write(kHeader, sizeof(kHeader) - 1);
    const auto version = schema.version;
    const auto coordinateSpace = static_cast<std::uint8_t>(schema.coordinateSpace);
    const auto attributeCount = static_cast<std::uint32_t>(schema.attributes.size());
    if (!Write(impl_->stream, version) || !Write(impl_->stream, coordinateSpace) ||
        !Write(impl_->stream, tile.level) || !Write(impl_->stream, tile.x) ||
        !Write(impl_->stream, tile.y) || !Write(impl_->stream, attributeCount)) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to write tile spool header");
        return false;
    }
    for (const auto& attribute : schema.attributes) {
        const auto type = static_cast<std::uint8_t>(attribute.type);
        if (!WriteString(impl_->stream, attribute.name) || !Write(impl_->stream, type)) {
            Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                  "unable to write tile spool schema");
            return false;
        }
    }
    return true;
}

bool TileSpoolWriter::Append(const SpoolPoint& point,
                             std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!impl_ || !impl_->open || point.attributes.size() != impl_->schema.attributes.size()) {
        Error(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
              "point does not match tile spool schema");
        return false;
    }
    for (std::size_t index = 0; index < point.attributes.size(); ++index) {
        if (static_cast<std::uint8_t>(point.attributes[index].index()) !=
            static_cast<std::uint8_t>(impl_->schema.attributes[index].type)) {
            Error(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
                  "point attribute type does not match tile spool schema");
            return false;
        }
    }
    impl_->buffered += sizeof(point.sourcePosition) + sizeof(point.stagePosition) +
                       [&]() {
                           std::size_t size = 0;
                           for (const auto& attribute : impl_->schema.attributes) {
                               size += AttributeSize(attribute.type);
                           }
                           return size;
                       }();
    impl_->points.push_back(point);
    return impl_->buffered >= impl_->memoryLimit ? Flush(diagnostics) : true;
}

bool TileSpoolWriter::Flush(std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!impl_ || !impl_->open) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to flush tile spool");
        return false;
    }
    for (const auto& point : impl_->points) {
        if (!impl_->stream.write(reinterpret_cast<const char*>(&point.sourcePosition),
                                 sizeof(point.sourcePosition)) ||
            !impl_->stream.write(reinterpret_cast<const char*>(&point.stagePosition),
                                 sizeof(point.stagePosition))) {
            Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                  "unable to write tile spool point");
            return false;
        }
        for (std::size_t index = 0; index < point.attributes.size(); ++index) {
            if (!WriteAttribute(impl_->stream, impl_->schema.attributes[index].type,
                                point.attributes[index])) {
                Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "unable to write tile spool attribute");
                return false;
            }
        }
        ++impl_->pointCount;
    }
    impl_->points.clear();
    impl_->buffered = 0;
    if (!impl_->stream.flush()) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to flush tile spool");
        return false;
    }
    return true;
}

bool TileSpoolWriter::Close(std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!impl_ || !impl_->open) {
        return true;
    }
    if (!Flush(diagnostics) || !impl_->stream.write(kFooter, sizeof(kFooter) - 1) ||
        !Write(impl_->stream, impl_->pointCount) || !impl_->stream.flush()) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to finalize tile spool");
        return false;
    }
    impl_->stream.close();
    impl_->open = false;
    return true;
}

std::size_t TileSpoolWriter::BufferedBytes() const noexcept {
    return impl_ ? impl_->buffered : 0;
}

TileSpoolReader::TileSpoolReader() = default;

TileSpoolReader::~TileSpoolReader() = default;

bool TileSpoolReader::Open(const std::filesystem::path& path,
                           PointTileId& tile,
                           SpoolSchema& schema,
                           std::vector<usdgeo::Diagnostic>& diagnostics) {
    impl_ = std::make_unique<Impl>();
    impl_->stream.open(path, std::ios::binary);
    char header[sizeof(kHeader) - 1] = {};
    std::uint8_t coordinateSpace = 0;
    std::uint32_t attributeCount = 0;
    if (!impl_->stream.read(header, sizeof(header)) ||
        std::memcmp(header, kHeader, sizeof(header)) != 0 ||
        !Read(impl_->stream, schema.version) || !Read(impl_->stream, coordinateSpace) ||
        !Read(impl_->stream, tile.level) || !Read(impl_->stream, tile.x) ||
        !Read(impl_->stream, tile.y) || !Read(impl_->stream, attributeCount) ||
        attributeCount > 100000) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "invalid or truncated tile spool header");
        return false;
    }
    schema.coordinateSpace = static_cast<SpoolCoordinateSpace>(coordinateSpace);
    schema.attributes.clear();
    schema.attributes.reserve(attributeCount);
    for (std::uint32_t index = 0; index < attributeCount; ++index) {
        PointAttribute attribute;
        std::uint8_t type = 0;
        if (!ReadString(impl_->stream, attribute.name) || !Read(impl_->stream, type)) {
            Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                  "invalid or truncated tile spool schema");
            return false;
        }
        attribute.type = static_cast<PointAttributeType>(type);
        schema.attributes.push_back(std::move(attribute));
    }
    if (!schema.IsValid() || !tile.IsValid()) {
        Error(diagnostics, usdgeo::DiagnosticCode::InvalidPointTile,
              "tile spool schema or tile id is invalid");
        return false;
    }
    impl_->recordSize = sizeof(usdgeo::Vec3d) * 2;
    for (const auto& attribute : schema.attributes) {
        impl_->recordSize += AttributeSize(attribute.type);
    }
    const auto dataStart = impl_->stream.tellg();
    impl_->stream.seekg(0, std::ios::end);
    const auto fileEnd = impl_->stream.tellg();
    if (dataStart < 0 || fileEnd < dataStart ||
        fileEnd - dataStart < kFooterSize ||
        (fileEnd - dataStart - kFooterSize) %
                static_cast<std::streamoff>(impl_->recordSize) != 0) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "invalid tile spool record layout");
        return false;
    }
    impl_->dataEnd = fileEnd - kFooterSize;
    if (impl_->dataEnd < dataStart) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "invalid tile spool footer layout");
        return false;
    }
    impl_->stream.seekg(dataStart);
    impl_->schema = schema;
    return true;
}

bool TileSpoolReader::ReadNext(SpoolPoint& point,
                               std::vector<usdgeo::Diagnostic>& diagnostics) {
    if (!impl_ || !impl_->stream || impl_->complete) {
        return false;
    }
    const auto position = impl_->stream.tellg();
    if (position < 0 || position > impl_->dataEnd) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "incomplete tile spool");
        return false;
    }
    if (position == impl_->dataEnd) {
        char footer[sizeof(kFooter) - 1] = {};
        std::uint64_t count = 0;
        if (!impl_->stream.read(footer, sizeof(footer)) ||
            std::memcmp(footer, kFooter, sizeof(footer)) != 0 ||
            !Read(impl_->stream, count) || count != impl_->pointsRead) {
            Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                  "incomplete tile spool");
            return false;
        }
        impl_->pointCount = count;
        impl_->complete = true;
        return false;
    }
    if (impl_->dataEnd - position < static_cast<std::streamoff>(impl_->recordSize) ||
        !impl_->stream.read(reinterpret_cast<char*>(&point.sourcePosition),
                            sizeof(point.sourcePosition)) ||
        !impl_->stream.read(reinterpret_cast<char*>(&point.stagePosition), sizeof(point.stagePosition))) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "truncated tile spool point");
        return false;
    }
    point.attributes.resize(impl_->schema.attributes.size());
    for (std::size_t index = 0; index < point.attributes.size(); ++index) {
        if (!ReadAttribute(impl_->stream, impl_->schema.attributes[index].type,
                           point.attributes[index])) {
            Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                  "truncated tile spool point");
            return false;
        }
    }
    ++impl_->pointsRead;
    return true;
}

bool TileSpoolReader::IsComplete() const noexcept {
    return impl_ && impl_->complete;
}

bool RemoveSpoolDirectory(const std::filesystem::path& directory,
                          std::vector<usdgeo::Diagnostic>& diagnostics) {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) {
        Error(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
              "unable to remove tile spool directory");
        return false;
    }
    return true;
}

} // namespace usdpointcloud