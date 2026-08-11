#include "usdply/Ply.h"

#include <cstdlib>
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
    Check(diagnostics.front().byteOffset == 21);

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
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::UnsupportedVersion);

    std::istringstream truncated("ply\nformat ascii 1.0\n");
    Check(!usdply::InspectHeader(truncated, header, diagnostics));
    Check(diagnostics.front().code == usdgeo::DiagnosticCode::TruncatedHeader);
}

} // namespace

int main() {
    TestAsciiVertexProperties();
    TestBinaryListProperty();
    TestCrLfAndBigEndianHeader();
    TestMalformedHeaders();
    return 0;
}