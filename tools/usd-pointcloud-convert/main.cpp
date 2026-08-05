#include "usdgeo/PointCloudLayer.h"
#include "usdpointcloud/FileFormatArguments.h"
#include "usdlas/Las.h"
#include "usdlaz/Laz.h"

#include <pxr/usd/sdf/layer.h>

#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

volatile std::sig_atomic_t interrupted = 0;

void HandleInterrupt(int) {
    interrupted = 1;
}

void PrintUsage() {
    std::cerr
        << "Usage: usd-pointcloud-convert <input.las|input.laz> <output.usda> "
           "--tile-size <source-units> [options]\n"
        << "Options:\n"
        << "  --memory-limit <bytes>\n"
        << "  --chunk-points <count>\n"
        << "  --attributes <comma-separated names>\n"
        << "  --payload-directory <directory>\n";
}

bool ReadOptionValue(int& index, int argc, char** argv, std::string& value) {
    if (index + 1 >= argc) {
        std::cerr << "Missing value for " << argv[index] << "\n";
        return false;
    }
    value = argv[++index];
    return true;
}

void PrintDiagnostics(const std::vector<usdgeo::Diagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        std::cerr << "diagnostic[" << static_cast<int>(diagnostic.code) << "]: "
                  << diagnostic.message << "\n";
    }
}

bool IsExtension(const std::filesystem::path& path, const char* extension) {
    auto value = path.extension().string();
    for (auto& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value == extension;
}

std::filesystem::path ManifestPath(
    const std::filesystem::path& outputPath) {
    return std::filesystem::path(outputPath.string() + ".manifest");
}

std::string RelativeManifestPath(const std::filesystem::path& base,
                                 const std::filesystem::path& path) {
    return std::filesystem::relative(path, base).generic_string();
}

bool WriteManifest(const std::filesystem::path& manifestPath,
                   const std::filesystem::path& inputPath,
                   const std::filesystem::path& outputPath,
                   const std::filesystem::path& payloadDirectory,
                   const usdpointcloud::PointReadRequest& request,
                   std::string& errorMessage) {
    std::error_code error;
    const auto inputSize = std::filesystem::file_size(inputPath, error);
    if (error) {
        errorMessage = "unable to inspect input for manifest: " +
                       error.message();
        return false;
    }

    std::vector<std::string> payloads;
    for (std::filesystem::recursive_directory_iterator iterator(
             payloadDirectory, error), end;
         iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && !error) {
            payloads.push_back(RelativeManifestPath(
                outputPath.parent_path(), iterator->path()));
        }
    }
    if (error) {
        errorMessage = "unable to enumerate payloads for manifest: " +
                       error.message();
        return false;
    }
    std::sort(payloads.begin(), payloads.end());

    std::map<std::string, std::string> manifestArguments =
        request.canonicalArguments;
    manifestArguments["payloadDirectory"] = RelativeManifestPath(
        outputPath.parent_path(), payloadDirectory);

    std::ofstream output(manifestPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "unable to create manifest";
        return false;
    }
    output << "format=usd-pointcloud-manifest-v1\n"
           << "input.file=" << inputPath.filename().generic_string() << "\n"
           << "input.sizeBytes=" << inputSize << "\n"
           << "output.root=" << outputPath.filename().generic_string() << "\n"
           << "output.payloadDirectory="
           << RelativeManifestPath(outputPath.parent_path(), payloadDirectory)
           << "\n";
    for (const auto& [key, value] : manifestArguments) {
        output << "argument." << key << "=" << value << "\n";
    }
    output << "payload.count=" << payloads.size() << "\n";
    for (const auto& payload : payloads) {
        output << "payload.file=" << payload << "\n";
    }
    if (!output) {
        errorMessage = "unable to write manifest";
        return false;
    }
    return true;
}

class SelectedPointStream final : public usdpointcloud::PointStream {
public:
    SelectedPointStream(std::unique_ptr<usdpointcloud::PointStream> stream,
                        std::vector<std::string> attributes)
        : stream_(std::move(stream)), attributes_(std::move(attributes)) {}

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        const auto status = stream_->ReadNext(chunk, data, diagnostic);
        if (status != usdpointcloud::PointStreamStatus::Chunk ||
            attributes_.empty()) {
            return status;
        }
        std::string error;
        if (!usdpointcloud::SelectPointDataAttributes(data, attributes_, error)) {
            diagnostic = {usdgeo::DiagnosticCode::InvalidFormatArgument,
                          usdgeo::Severity::Error, error, std::nullopt,
                          std::nullopt};
            return usdpointcloud::PointStreamStatus::Error;
        }
        chunk = usdpointcloud::MakePointChunk(data, chunk.bounds);
        return usdpointcloud::PointStreamStatus::Chunk;
    }

private:
    std::unique_ptr<usdpointcloud::PointStream> stream_;
    std::vector<std::string> attributes_;
};

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--help") {
        PrintUsage();
        return 0;
    }
    if (argc < 4) {
        PrintUsage();
        return 2;
    }

    const std::filesystem::path inputPath = std::filesystem::absolute(argv[1]);
    const std::filesystem::path outputPath = std::filesystem::absolute(argv[2]);
    const auto manifestPath = ManifestPath(outputPath);
    if (!IsExtension(inputPath, ".las") && !IsExtension(inputPath, ".laz")) {
        std::cerr << "Input must have a .las or .laz extension\n";
        return 2;
    }
    if (!IsExtension(outputPath, ".usda")) {
        std::cerr << "Output must have a .usda extension\n";
        return 2;
    }

    std::map<std::string, std::string> arguments;
    arguments.emplace("tile", "true");
    bool hasTileSize = false;
    std::filesystem::path payloadDirectory;
    for (int index = 3; index < argc; ++index) {
        const std::string option = argv[index];
        std::string value;
        if (option == "--tile-size") {
            if (!ReadOptionValue(index, argc, argv, value)) return 2;
            arguments["tileSize"] = value;
            hasTileSize = true;
        } else if (option == "--memory-limit") {
            if (!ReadOptionValue(index, argc, argv, value)) return 2;
            arguments["tileMemoryLimit"] = value;
        } else if (option == "--chunk-points") {
            if (!ReadOptionValue(index, argc, argv, value)) return 2;
            arguments["chunkPointLimit"] = value;
        } else if (option == "--attributes") {
            if (!ReadOptionValue(index, argc, argv, value)) return 2;
            arguments["attributes"] = value;
        } else if (option == "--payload-directory") {
            if (!ReadOptionValue(index, argc, argv, value)) return 2;
            payloadDirectory = std::filesystem::path(value);
        } else {
            std::cerr << "Unknown option: " << option << "\n";
            PrintUsage();
            return 2;
        }
    }

    if (!hasTileSize) {
        std::cerr << "--tile-size is required\n";
        return 2;
    }
    if (payloadDirectory.empty()) {
        payloadDirectory = outputPath.parent_path() /
            (outputPath.stem().string() + "_payloads");
    } else if (payloadDirectory.is_relative()) {
        payloadDirectory = outputPath.parent_path() / payloadDirectory;
    }
    payloadDirectory = std::filesystem::absolute(payloadDirectory);
    arguments["payloadDirectory"] = payloadDirectory.string();

    std::error_code error;
    if (std::filesystem::exists(outputPath, error) || error ||
        std::filesystem::exists(manifestPath, error) || error ||
        std::filesystem::exists(payloadDirectory, error) || error) {
        std::cerr << "Output or payload directory already exists\n";
        return 2;
    }
    std::filesystem::create_directories(outputPath.parent_path(), error);
    if (error) {
        std::cerr << "Unable to create output directory: " << error.message()
                  << "\n";
        return 1;
    }

    usdpointcloud::PointReadRequest request;
    std::vector<usdgeo::Diagnostic> diagnostics;
    if (!usdpointcloud::NormalizeFileFormatArguments(
            arguments, request, diagnostics)) {
        PrintDiagnostics(diagnostics);
        return 2;
    }
    request.readOptions.isCancelled = [] { return interrupted != 0; };

    usdlas::LasHeader header;
    std::unique_ptr<usdpointcloud::PointStream> stream;
    if (IsExtension(inputPath, ".las")) {
        stream = usdlas::OpenLasPointStream(
            inputPath.string(), request.readOptions, header, diagnostics);
    } else {
        stream = usdlaz::OpenLazPointStream(
            inputPath.string(), request.readOptions, header, diagnostics);
    }
    if (!stream) {
        PrintDiagnostics(diagnostics);
        return 1;
    }
    if (!request.attributes.empty()) {
        stream = std::make_unique<SelectedPointStream>(
            std::move(stream), request.attributes);
    }

    usdpointcloud::PointChunk metadataChunk;
    usdgeo::GeoReference reference;
    usdgeo::SpatialBounds bounds;
    std::string metadataError;
    if (!usdlas::BuildPointCloudMetadata(
            header, metadataChunk, reference, bounds, metadataError)) {
        std::cerr << "Unable to build source metadata: " << metadataError << "\n";
        return 1;
    }

    std::signal(SIGINT, HandleInterrupt);
    const auto temporaryRootPath = outputPath.parent_path() /
        (outputPath.stem().string() + ".tmp.usda");
    const auto temporaryManifestPath =
        std::filesystem::path(manifestPath.string() + ".tmp");
    if (std::filesystem::exists(temporaryRootPath, error) || error ||
        std::filesystem::exists(temporaryManifestPath, error) || error) {
        std::cerr << "Temporary output already exists: "
                  << temporaryRootPath.string() << "\n";
        return 2;
    }
    auto layer = pxr::SdfLayer::CreateAnonymous("PointCloud.tmp.usda");
    if (!layer) {
        std::cerr << "Unable to create temporary root layer\n";
        return 1;
    }

    const usdgeo::PointCloudPayloadOptions payloadOptions{
        payloadDirectory.string(), temporaryRootPath.string(),
        request.tileMemoryLimitBytes};
    const auto authored = usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", *stream, reference,
        {request.tileSize, 0}, payloadOptions, diagnostics);
    if (!authored || interrupted != 0) {
        if (interrupted != 0) {
            std::cerr << "Conversion cancelled\n";
        }
        PrintDiagnostics(diagnostics);
        layer = nullptr;
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }
    layer->SetIdentifier(temporaryRootPath.string());
    if (!layer->Save()) {
        std::cerr << "Unable to save generated root layer\n";
        layer = nullptr;
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }
    std::string manifestError;
    if (!WriteManifest(temporaryManifestPath, inputPath, outputPath,
                       payloadDirectory, request, manifestError)) {
        std::cerr << "Unable to write conversion manifest: " << manifestError
                  << "\n";
        layer = nullptr;
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove(temporaryManifestPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }
    if (interrupted != 0) {
        std::cerr << "Conversion cancelled\n";
        layer = nullptr;
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove(temporaryManifestPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }
    layer = nullptr;
    std::filesystem::rename(temporaryManifestPath, manifestPath, error);
    if (error) {
        std::cerr << "Unable to publish conversion manifest: "
                  << error.message() << "\n";
        std::filesystem::remove(temporaryManifestPath, error);
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }
    std::filesystem::rename(temporaryRootPath, outputPath, error);
    if (error) {
        std::cerr << "Unable to publish generated root layer: "
                  << error.message() << "\n";
        std::filesystem::remove(temporaryRootPath, error);
        std::filesystem::remove(manifestPath, error);
        std::filesystem::remove_all(payloadDirectory, error);
        return 1;
    }

    std::cout << "Generated " << outputPath.string() << "\n";
    std::cout << "Payloads: " << payloadDirectory.string() << "\n";
    return 0;
}
