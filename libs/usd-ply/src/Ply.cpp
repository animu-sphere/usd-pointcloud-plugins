#include "usdply/Ply.h"

#include <charconv>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>

namespace usdply {
namespace {

void AddDiagnostic(std::vector<usdgeo::Diagnostic>& diagnostics,
                   usdgeo::DiagnosticCode code,
                   const std::string& message,
                   std::uint64_t byteOffset) {
    diagnostics.push_back(
        {code, usdgeo::Severity::Error, message, byteOffset, std::nullopt});
}

bool ParseScalarType(const std::string& token, PlyScalarType& type) {
    if (token == "char" || token == "int8") {
        type = PlyScalarType::Int8;
    } else if (token == "uchar" || token == "uint8") {
        type = PlyScalarType::UInt8;
    } else if (token == "short" || token == "int16") {
        type = PlyScalarType::Int16;
    } else if (token == "ushort" || token == "uint16") {
        type = PlyScalarType::UInt16;
    } else if (token == "int" || token == "int32") {
        type = PlyScalarType::Int32;
    } else if (token == "uint" || token == "uint32") {
        type = PlyScalarType::UInt32;
    } else if (token == "float" || token == "float32") {
        type = PlyScalarType::Float32;
    } else if (token == "double" || token == "float64") {
        type = PlyScalarType::Float64;
    } else {
        return false;
    }
    return true;
}

bool ParseCount(const std::string& token, std::uint64_t& count) {
    const auto result = std::from_chars(token.data(),
                                        token.data() + token.size(), count);
    return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

bool ParseFormat(const std::string& encoding, PlyFormat& format) {
    if (encoding == "ascii") {
        format = PlyFormat::Ascii;
    } else if (encoding == "binary_little_endian") {
        format = PlyFormat::BinaryLittleEndian;
    } else if (encoding == "binary_big_endian") {
        format = PlyFormat::BinaryBigEndian;
    } else {
        return false;
    }
    return true;
}

} // namespace

bool InspectHeader(std::istream& input,
                   PlyHeader& header,
                   std::vector<usdgeo::Diagnostic>& diagnostics) {
    header = {};
    diagnostics.clear();

    std::string line;
    std::uint64_t byteOffset = 0;
    if (!std::getline(input, line) || line != "ply") {
        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::InvalidSignature,
                      "PLY header must begin with the ply signature", 0);
        return false;
    }
    byteOffset += static_cast<std::uint64_t>(line.size()) + 1;

    bool sawFormat = false;
    PlyElement* currentElement = nullptr;
    while (std::getline(input, line)) {
        const auto lineOffset = byteOffset;
        byteOffset += static_cast<std::uint64_t>(line.size()) + 1;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "end_header") {
            if (!sawFormat) {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::UnsupportedVersion,
                              "PLY header is missing a format declaration",
                              lineOffset);
                return false;
            }
            header.dataOffset = byteOffset;
            return true;
        }

        std::istringstream fields(line);
        std::string keyword;
        fields >> keyword;
        if (keyword.empty() || keyword == "comment" || keyword == "obj_info") {
            continue;
        }
        if (keyword == "format") {
            std::string encoding;
            std::string version;
            if (sawFormat || !(fields >> encoding >> version) ||
                version != "1.0" || !ParseFormat(encoding, header.format)) {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::UnsupportedVersion,
                              "PLY format must be ascii, binary_little_endian, or binary_big_endian version 1.0",
                              lineOffset);
                return false;
            }
            sawFormat = true;
            continue;
        }
        if (keyword == "element") {
            std::string name;
            std::string countToken;
            std::uint64_t count = 0;
            if (!sawFormat || !(fields >> name >> countToken) || name.empty() ||
                !ParseCount(countToken, count)) {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::DecodeFailure,
                              "PLY element declaration is invalid", lineOffset);
                return false;
            }
            header.elements.push_back({std::move(name), count, {}});
            currentElement = &header.elements.back();
            continue;
        }
        if (keyword == "property") {
            std::string typeToken;
            std::string name;
            if (!currentElement || !(fields >> typeToken)) {
                AddDiagnostic(diagnostics,
                              usdgeo::DiagnosticCode::DecodeFailure,
                              "PLY property must follow an element declaration",
                              lineOffset);
                return false;
            }
            PlyProperty property;
            if (typeToken == "list") {
                std::string countTypeToken;
                std::string valueTypeToken;
                if (!(fields >> countTypeToken >> valueTypeToken >> name) ||
                    !ParseScalarType(countTypeToken, property.listCountType) ||
                    !ParseScalarType(valueTypeToken, property.valueType)) {
                    AddDiagnostic(diagnostics,
                                  usdgeo::DiagnosticCode::DecodeFailure,
                                  "PLY list property declaration is invalid",
                                  lineOffset);
                    return false;
                }
                property.isList = true;
            } else if (!(fields >> name) ||
                       !ParseScalarType(typeToken, property.valueType)) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                              "PLY scalar property declaration is invalid",
                              lineOffset);
                return false;
            }
            if (name.empty()) {
                AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                              "PLY property name is empty", lineOffset);
                return false;
            }
            property.name = std::move(name);
            currentElement->properties.push_back(std::move(property));
            continue;
        }

        AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::DecodeFailure,
                      "PLY header contains an unsupported declaration",
                      lineOffset);
        return false;
    }

    AddDiagnostic(diagnostics, usdgeo::DiagnosticCode::TruncatedHeader,
                  "PLY header ended before end_header", byteOffset);
    return false;
}

} // namespace usdply