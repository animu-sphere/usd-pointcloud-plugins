#include "usdgeo/PointCloudLayer.h"

#include <pxr/usd/sdf/layer.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

struct BenchmarkOptions {
    std::size_t pointCount = 1'000'000;
    std::size_t chunkPointCount = 65'536;
    double tileSize = 128.0;
    std::size_t memoryLimitBytes = 1 * 1024 * 1024;
};

constexpr std::size_t kPointsPerTile = 4096;
constexpr std::size_t kTilesPerRow = 32;

bool ParseSize(const std::string& text, std::size_t& value) {
    try {
        const auto parsed = std::stoull(text);
        if (parsed == 0 || parsed > std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        value = static_cast<std::size_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseDouble(const std::string& text, double& value) {
    try {
        value = std::stod(text);
        return std::isfinite(value) && value > 0.0;
    } catch (...) {
        return false;
    }
}

bool ParseOptions(int argc, char** argv, BenchmarkOptions& options) {
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == "--help") {
            std::cout
                << "Usage: usdPointCloudAuthoring_stream_benchmark"
                   " [--points N] [--chunk-points N] [--tile-size N]"
                   " [--memory-limit BYTES]\n";
            return false;
        }
        if (index + 1 >= argc) return false;
        const std::string argument(argv[index]);
        const std::string value(argv[++index]);
        if (argument == "--points") {
            if (!ParseSize(value, options.pointCount)) return false;
        } else if (argument == "--chunk-points") {
            if (!ParseSize(value, options.chunkPointCount)) return false;
        } else if (argument == "--tile-size") {
            if (!ParseDouble(value, options.tileSize)) return false;
        } else if (argument == "--memory-limit") {
            if (!ParseSize(value, options.memoryLimitBytes)) return false;
        } else {
            return false;
        }
    }
    return true;
}

std::uint64_t ResidentBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        return 0;
    }
    return static_cast<std::uint64_t>(counters.WorkingSetSize);
#else
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
#ifdef __APPLE__
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
#endif
}

std::uint64_t ProcessWriteBytes() {
#ifdef _WIN32
    IO_COUNTERS counters{};
    if (!GetProcessIoCounters(GetCurrentProcess(), &counters)) return 0;
    return static_cast<std::uint64_t>(counters.WriteTransferCount);
#else
    return 0;
#endif
}

std::unordered_set<std::filesystem::path> ExistingSpoolDirectories() {
    std::unordered_set<std::filesystem::path> result;
    std::error_code error;
    const auto temporaryDirectory = std::filesystem::temp_directory_path(error);
    if (error) return result;
    for (const auto& entry : std::filesystem::directory_iterator(
             temporaryDirectory, error)) {
        if (error) break;
        if (entry.is_directory(error) &&
            entry.path().filename().string().rfind(
                "usdgeo_point_spool_", 0) == 0) {
            result.insert(entry.path());
        }
        error.clear();
    }
    return result;
}

std::uint64_t DirectoryBytes(const std::filesystem::path& directory) {
    std::uint64_t bytes = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(
             directory, error)) {
        if (error) break;
        if (entry.is_regular_file(error)) bytes += entry.file_size(error);
        error.clear();
    }
    return bytes;
}

std::uint64_t NewSpoolBytes(
    const std::unordered_set<std::filesystem::path>& existingDirectories) {
    std::uint64_t bytes = 0;
    for (const auto& directory : ExistingSpoolDirectories()) {
        if (existingDirectories.count(directory) == 0) {
            bytes += DirectoryBytes(directory);
        }
    }
    return bytes;
}

std::uint64_t DirectoryBytesRecursive(const std::filesystem::path& directory) {
    std::uint64_t bytes = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             directory, error)) {
        if (error) break;
        if (entry.is_regular_file(error)) bytes += entry.file_size(error);
        error.clear();
    }
    return bytes;
}

class GeneratedPointStream final : public usdpointcloud::PointStream {
public:
    GeneratedPointStream(std::size_t pointCount, std::size_t chunkPointCount,
                         double tileSize)
        : pointCount_(pointCount),
          chunkPointCount_(chunkPointCount),
          tileSize_(tileSize) {}

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override {
        diagnostic = {};
        if (index_ == pointCount_) return usdpointcloud::PointStreamStatus::End;

        const auto count = std::min(chunkPointCount_, pointCount_ - index_);
        data.positions.reserve(count);
        data.intensity.reserve(count);
        data.extraByteNames = {"temperature"};
        data.extraByteComponentCounts = {1};
        data.extraBytes.emplace_back();
        data.extraBytes.front().reserve(count);

        for (std::size_t offset = 0; offset < count; ++offset) {
            const auto pointIndex = index_ + offset;
            const auto tileIndex = pointIndex / kPointsPerTile;
            const auto tileX = tileIndex % kTilesPerRow;
            const auto tileY = tileIndex / kTilesPerRow;
            const auto pointInTile = pointIndex % kPointsPerTile;
            const auto localX = static_cast<double>(pointInTile % 64) * 0.25;
            const auto localY = static_cast<double>(pointInTile / 64) * 0.25;
            data.positions.push_back(
                {static_cast<double>(tileX) * tileSize_ + localX + 0.25,
                 static_cast<double>(tileY) * tileSize_ + localY + 0.25,
                 static_cast<double>(pointIndex % 1000)});
            data.intensity.push_back(static_cast<std::uint16_t>(pointIndex));
            data.extraBytes.front().push_back(
                static_cast<double>(pointIndex % 1000) * 0.1);
        }
        usdgeo::SpatialBounds bounds = usdgeo::SpatialBounds::Empty();
        for (const auto& position : data.positions) bounds.Expand(position);
        chunk = usdpointcloud::MakePointChunk(data, bounds);
        index_ += count;
        return usdpointcloud::PointStreamStatus::Chunk;
    }

private:
    std::size_t pointCount_;
    std::size_t chunkPointCount_;
    double tileSize_;
    std::size_t index_ = 0;
};

std::uint64_t PayloadBytes(const std::filesystem::path& directory) {
    std::uint64_t bytes = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(
             directory, error)) {
        if (error) break;
        if (entry.is_regular_file(error) && entry.path().extension() == ".usdc") {
            bytes += entry.file_size(error);
        }
        error.clear();
    }
    return bytes;
}

} // namespace

int main(int argc, char** argv) {
    BenchmarkOptions options;
    if (!ParseOptions(argc, argv, options)) {
        return argc > 1 && std::string(argv[1]) == "--help" ? 0 : 2;
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto outputDirectory = std::filesystem::temp_directory_path() /
                                 ("usdgeo_stream_benchmark_" +
                                  std::to_string(stamp));
    const auto rootLayerPath = outputDirectory / "PointCloud.usda";
    std::error_code error;
    std::filesystem::create_directories(outputDirectory, error);
    if (error) {
        std::cerr << "unable to create benchmark output directory\n";
        return 1;
    }

    const auto existingSpoolDirectories = ExistingSpoolDirectories();
    std::atomic<bool> sampling{true};
    std::atomic<std::uint64_t> peakResident{ResidentBytes()};
    std::atomic<std::uint64_t> peakSpoolBytes{0};
    const auto sample = [&]() {
        while (sampling.load(std::memory_order_relaxed)) {
            peakResident.store(
                std::max(peakResident.load(std::memory_order_relaxed),
                         ResidentBytes()),
                std::memory_order_relaxed);
            peakSpoolBytes.store(
                std::max(peakSpoolBytes.load(std::memory_order_relaxed),
                         NewSpoolBytes(existingSpoolDirectories)),
                std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    const auto baselineResident = ResidentBytes();
    const auto writeBytesBefore = ProcessWriteBytes();
    std::thread sampler(sample);

    const auto start = std::chrono::steady_clock::now();
    const auto layer = pxr::SdfLayer::CreateNew(rootLayerPath.string());
    if (!layer) {
        sampling.store(false, std::memory_order_relaxed);
        sampler.join();
        std::filesystem::remove_all(outputDirectory, error);
        return 1;
    }
    usdgeo::GeoReference reference;
    reference.epsgCode = 26910;
    GeneratedPointStream stream(options.pointCount, options.chunkPointCount,
                                options.tileSize);
    std::vector<usdgeo::Diagnostic> diagnostics;
    const auto succeeded = usdgeo::AuthorPointCloudTiledAssetFromStream(
        layer.operator->(), "/PointCloud", stream, reference,
        {options.tileSize, 0},
        {outputDirectory.string(), rootLayerPath.string(),
         options.memoryLimitBytes},
        diagnostics);
    if (succeeded) layer->Export(rootLayerPath.string());
    const auto elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - start)
                             .count();

    sampling.store(false, std::memory_order_relaxed);
    sampler.join();
    peakResident.store(
        std::max(peakResident.load(std::memory_order_relaxed), ResidentBytes()),
        std::memory_order_relaxed);
    peakSpoolBytes.store(
        std::max(peakSpoolBytes.load(std::memory_order_relaxed),
                 NewSpoolBytes(existingSpoolDirectories)),
        std::memory_order_relaxed);

    const auto outputBytes = DirectoryBytesRecursive(outputDirectory);
    const auto payloadBytes = PayloadBytes(outputDirectory);
    const auto writeBytesAfter = ProcessWriteBytes();
    std::cout << "points=" << options.pointCount
              << " chunk_points=" << options.chunkPointCount
              << " tile_size=" << options.tileSize
              << " tile_count="
              << (options.pointCount + kPointsPerTile - 1) / kPointsPerTile
              << " memory_limit_bytes=" << options.memoryLimitBytes
              << " elapsed_seconds=" << elapsed
              << " baseline_rss_bytes=" << baselineResident
              << " peak_rss_bytes=" << peakResident.load()
              << " rss_delta_bytes="
              << (peakResident.load() > baselineResident
                      ? peakResident.load() - baselineResident
                      : 0)
              << " peak_spool_bytes=" << peakSpoolBytes.load()
              << " payload_bytes=" << payloadBytes
              << " output_bytes=" << outputBytes
              << " process_write_bytes="
              << (writeBytesAfter >= writeBytesBefore
                      ? writeBytesAfter - writeBytesBefore
                      : 0)
              << " success=" << (succeeded ? "true" : "false") << '\n';
    for (const auto& diagnostic : diagnostics) {
        std::cerr << diagnostic.message << '\n';
    }
    std::filesystem::remove_all(outputDirectory, error);
    return succeeded ? 0 : 1;
}