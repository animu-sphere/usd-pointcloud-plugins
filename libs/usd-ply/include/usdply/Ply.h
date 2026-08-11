#pragma once

#include "usdgeo/Diagnostic.h"
#include "usdpointcloud/PointCloud.h"

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace usdply {

enum class PlyFormat {
    Ascii,
    BinaryLittleEndian,
    BinaryBigEndian,
};

enum class PlyScalarType {
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64,
};

struct PlyProperty {
    std::string name;
    PlyScalarType valueType = PlyScalarType::Float32;
    bool isList = false;
    PlyScalarType listCountType = PlyScalarType::UInt8;
};

struct PlyElement {
    std::string name;
    std::uint64_t count = 0;
    std::vector<PlyProperty> properties;
};

struct PlyHeader {
    PlyFormat format = PlyFormat::Ascii;
    std::vector<PlyElement> elements;
    std::uint64_t dataOffset = 0;
};

bool InspectHeader(std::istream& input,
                   PlyHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics);

class PlyPointStream final : public usdpointcloud::PointStream {
public:
    ~PlyPointStream() override;

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override;

private:
    friend std::unique_ptr<PlyPointStream> OpenPointStream(
        const std::string&,
        const usdpointcloud::PointReadOptions&,
        PlyHeader&,
        std::vector<usdgeo::Diagnostic>&);

    struct Impl;

    explicit PlyPointStream(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<PlyPointStream> OpenPointStream(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    PlyHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool ReadPointCloud(
    const std::string& filename,
    const usdpointcloud::PointReadOptions& options,
    const usdgeo::GeoReference& reference,
    usdpointcloud::PointCloudAsset& asset,
    std::vector<usdgeo::Diagnostic>& diagnostics);

} // namespace usdply