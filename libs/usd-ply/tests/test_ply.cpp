#include "usdply/Ply.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAsciiVertexProperties() {
    const std::string contents =
        "ply\n"
        "format ascii 1.0\n"
        "comment source fixture\n"
        "element vertex 2\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "property uchar red\n"
        "end_header\n"
        "0 1 2 3\n";
    std::istringstream input(contents);
    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;

    Check(usdply::InspectHeader(input, header, diagnostics));
    Check(diagnostics.empty());
    Check(header.format == usdply::PlyFormat::Ascii);
    Check(header.elements.size() == 1);
    Check(header.elements.front().name == "vertex");
    Check(header.elements.front().count == 2);
    Check(header.elements.front().properties.size() == 4);
    Check(header.elements.front().properties.back().name == "red");
    Check(header.dataOffset == contents.find("0 1 2 3"));
}

void TestBinaryListProperty() {
    std::istringstream input(
        "ply\n"
        "format binary_little_endian 1.0\n"
        "element face 1\n"
        "property list uchar int vertex_indices\n"
        "end_header\n");
    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;

    Check(usdply::InspectHeader(input, header, diagnostics));
    Check(header.format == usdply::PlyFormat::BinaryLittleEndian);
    const auto& property = header.elements.front().properties.front();
    Check(property.isList);
    Check(property.listCountType == usdply::PlyScalarType::UInt8);
    Check(property.valueType == usdply::PlyScalarType::Int32);
}

void TestCrLfAndBigEndianHeader() {
    const std::string contents =
        "ply\r\n"
        "format binary_big_endian 1.0\r\n"
        "element vertex 1\r\n"
        "property float x\r\n"
        "end_header\r\n"
        "data";
    std::istringstream input(contents);
    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;

    Check(usdply::InspectHeader(input, header, diagnostics));
    Check(header.format == usdply::PlyFormat::BinaryBigEndian);
    Check(header.dataOffset == contents.find("data"));
}

void TestMalformedHeaders() {
    std::istringstream invalidSignature("not-ply\n");
    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    Check(!usdply::InspectHeader(invalidSignature, header, diagnostics));
    Check(diagnostics.size() == 1);
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::InvalidSignature);
    Check(diagnostics.front().byteOffset == 0);

    std::istringstream invalidProperty(
        "ply\nformat ascii 1.0\nproperty float x\nend_header\n");
    Check(!usdply::InspectHeader(invalidProperty, header, diagnostics));
    Check(!diagnostics.empty());

    std::istringstream partialHeader(
        "ply\nformat binary_little_endian 1.0\nelement vertex 1\n"
        "property unsupported x\nend_header\n");
    Check(!usdply::InspectHeader(partialHeader, header, diagnostics));
    Check(header.format == usdply::PlyFormat::Ascii);
    Check(header.elements.empty());
    Check(header.dataOffset == 0);

    std::istringstream duplicateFormat(
        "ply\nformat ascii 1.0\nformat ascii 1.0\nend_header\n");
    Check(!usdply::InspectHeader(duplicateFormat, header, diagnostics));
    Check(!diagnostics.empty());

    std::istringstream truncated("ply\nformat ascii 1.0\n");
    Check(!usdply::InspectHeader(truncated, header, diagnostics));
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::TruncatedHeader);
}

void TestAsciiPointStream() {
    const auto path = std::filesystem::temp_directory_path() /
                      "usdply-point-stream-test.ply";
    {
        std::ofstream output(path);
        output << "ply\n"
               << "format ascii 1.0\n"
               << "element vertex 2\n"
               << "property float x\n"
               << "property float y\n"
               << "property float z\n"
               << "property uchar red\n"
               << "property uchar green\n"
               << "property uchar blue\n"
               << "property float temperature\n"
               << "end_header\n"
               << "1 2 3 10 20 30 4.5\n"
               << "-1 0 2 40 50 60 5.5\n";
    }

    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdpointcloud::PointReadOptions options;
    options.chunkPointLimit = 1;
    const auto stream = usdply::OpenPointStream(
        path.string(), options, header, diagnostics);
    Check(stream != nullptr);
    Check(diagnostics.empty());

    usdpointcloud::PointChunk chunk;
    usdpointcloud::PointData data;
    usdgeo::Diagnostic diagnostic;
    Check(stream->ReadNext(chunk, data, diagnostic) ==
          usdpointcloud::PointStreamStatus::Chunk);
    Check(chunk.pointCount == 1 && data.positions[0].x == 1.0);
    Check(data.red[0] == 10 && data.green[0] == 20 && data.blue[0] == 30);
    Check(data.extraByteNames.size() == 1 &&
          data.extraBytes.front().front() == 4.5);
    Check(stream->ReadNext(chunk, data, diagnostic) ==
          usdpointcloud::PointStreamStatus::Chunk);
    Check(data.positions[0].x == -1.0 && data.extraBytes.front().front() == 5.5);
    Check(stream->ReadNext(chunk, data, diagnostic) ==
          usdpointcloud::PointStreamStatus::End);
    std::filesystem::remove(path);
}

void TestBinaryLittleEndianPointStream() {
    const auto path = std::filesystem::temp_directory_path() /
                      "usdply-binary-point-stream-test.ply";
    {
        std::ofstream output(path, std::ios::binary);
        output << "ply\n"
               << "format binary_little_endian 1.0\n"
               << "element vertex 1\n"
               << "property float x\n"
               << "property float y\n"
               << "property float z\n"
               << "end_header\n";
        const float coordinates[] = {1.25f, -2.5f, 3.75f};
        output.write(reinterpret_cast<const char*>(coordinates),
                     sizeof(coordinates));
    }
    usdply::PlyHeader header;
    std::vector<usdgeo::Diagnostic> diagnostics;
    usdpointcloud::PointReadOptions options;
    const auto stream = usdply::OpenPointStream(
        path.string(), options, header, diagnostics);
    Check(stream != nullptr);
    usdpointcloud::PointChunk chunk;
    usdpointcloud::PointData data;
    usdgeo::Diagnostic diagnostic;
    Check(stream->ReadNext(chunk, data, diagnostic) ==
          usdpointcloud::PointStreamStatus::Chunk);
    Check(data.positions.size() == 1 && data.positions[0].x == 1.25 &&
          data.positions[0].y == -2.5 && data.positions[0].z == 3.75);
    std::filesystem::remove(path);
}

} // namespace

int main() {
    TestAsciiVertexProperties();
    TestBinaryListProperty();
    TestCrLfAndBigEndianHeader();
    TestMalformedHeaders();
    TestAsciiPointStream();
    TestBinaryLittleEndianPointStream();
    return 0;
}