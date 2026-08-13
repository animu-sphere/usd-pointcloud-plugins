#pragma once

#include "usdpointcloud/PointCloud.h"
#include "usdlas/Las.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace usdlaz {

class LazDecoder {
public:
    virtual ~LazDecoder() = default;

    virtual bool ReadHeader(usdlas::LasHeader& header,
                            std::string& error) = 0;
    virtual bool ReadHeader(usdlas::LasHeader& header,
                            std::vector<usdgeo::Diagnostic>& diagnostics);
    virtual bool ReadChunk(std::size_t maximumPoints,
                           std::vector<usdlas::LasPoint>& points,
                           bool& complete,
                           std::string& error) = 0;
    virtual bool ReadChunk(std::size_t maximumPoints,
                           std::vector<usdlas::LasPoint>& points,
                           bool& complete,
                           std::vector<usdgeo::Diagnostic>& diagnostics);
    virtual std::uint64_t SourceBytesRead() const noexcept { return 0; }
};

std::unique_ptr<LazDecoder> CreateFileDecoder(const std::string& filename,
                                              std::string& error);
std::unique_ptr<LazDecoder> CreateFileDecoder(
    const std::string& filename,
    std::vector<usdgeo::Diagnostic>& diagnostics);

using LazPointConsumer = std::function<bool(const usdlas::LasPoint&,
                                            std::uint64_t)>;
using LazInput = std::function<void(unsigned char*, std::size_t)>;

class LazChunkDecoder {
public:
    virtual ~LazChunkDecoder() = default;

    virtual bool ReadChunk(
        std::size_t maximumPoints,
        std::vector<usdlas::LasPoint>& points,
        bool& complete,
        std::vector<usdgeo::Diagnostic>& diagnostics) = 0;
};

std::unique_ptr<LazChunkDecoder> CreateLazChunkDecoder(
    const usdlas::LasHeader& header,
    std::uint64_t pointCount,
    const LazInput& input,
    std::vector<usdgeo::Diagnostic>& diagnostics);

bool DecodeLazChunk(const usdlas::LasHeader& header,
                    const std::vector<std::uint8_t>& bytes,
                    std::uint64_t pointCount,
                    std::vector<usdlas::LasPoint>& points,
                    std::vector<usdgeo::Diagnostic>& diagnostics);
bool DecodeLazChunk(const usdlas::LasHeader& header,
                    const std::vector<std::uint8_t>& bytes,
                    std::uint64_t pointCount,
                    const LazPointConsumer& consume,
                    std::vector<usdgeo::Diagnostic>& diagnostics);
bool DecodeLazChunk(const usdlas::LasHeader& header,
                    std::uint64_t pointCount,
                    const LazInput& input,
                    const LazPointConsumer& consume,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

enum class LazReadFailure {
    None,
    FileOpen,
    Decode,
    InvalidRequest,
    Asset,
};

bool ReadPointCloud(const std::string& filename,
                    const usdpointcloud::PointReadOptions& options,
                    const std::vector<std::string>& attributes,
                    const std::string& missingCrsMessage,
                    usdpointcloud::PointCloudAsset& asset,
                    LazReadFailure& failure,
                    std::vector<usdgeo::Diagnostic>& diagnostics);

using LazReadOptions = usdpointcloud::PointReadOptions;
using LazPointChunkConsumer = std::function<bool(
    const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>&)>;
using LazPointChunkErrorConsumer = std::function<bool(
    const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>&,
    std::string&)>;

class LazReader {
public:
    explicit LazReader(std::unique_ptr<LazDecoder> decoder);

    bool Read(const LazReadOptions& options,
              const LazPointChunkConsumer& consume,
              usdlas::LasHeader& header,
              std::string& error);
    bool Read(const LazReadOptions& options,
              const LazPointChunkConsumer& consume,
              usdlas::LasHeader& header,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool Read(const LazReadOptions& options,
              const LazPointChunkErrorConsumer& consume,
              usdlas::LasHeader& header,
              std::vector<usdgeo::Diagnostic>& diagnostics);
    bool ReadMetadata(usdlas::LasHeader& header,
                      std::vector<usdgeo::Diagnostic>& diagnostics);

private:
    std::unique_ptr<LazDecoder> decoder_;
};

class LazPointStream final : public usdpointcloud::PointStream {
public:
    ~LazPointStream() override;

    usdpointcloud::PointStreamStatus ReadNext(
        usdpointcloud::PointChunk& chunk,
        usdpointcloud::PointData& data,
        usdgeo::Diagnostic& diagnostic) override;
    std::uint64_t SourceBytesRead() const noexcept override;

    const usdlas::LasHeader& Header() const noexcept;

private:
    friend std::unique_ptr<LazPointStream> OpenLazPointStream(
        const std::string&,
        const LazReadOptions&,
        usdlas::LasHeader&,
        std::vector<usdgeo::Diagnostic>&);

    LazPointStream(std::unique_ptr<LazDecoder> decoder,
                   LazReadOptions options,
                   usdlas::LasHeader header,
                   std::size_t maximumPoints);

    std::unique_ptr<LazDecoder> decoder_;
    LazReadOptions options_;
    usdlas::LasHeader header_;
    std::uint64_t pointsRead_ = 0;
    std::uint64_t selectedPointsRead_ = 0;
    std::uint64_t endPoint_ = 0;
    std::size_t maximumPoints_ = 0;
    bool complete_ = false;
    bool ended_ = false;
};

std::unique_ptr<LazPointStream> OpenLazPointStream(
    const std::string& filename,
    const LazReadOptions& options,
    usdlas::LasHeader& header,
    std::vector<usdgeo::Diagnostic>& diagnostics);

} // namespace usdlaz
