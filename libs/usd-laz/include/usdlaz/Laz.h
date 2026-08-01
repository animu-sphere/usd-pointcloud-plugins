#pragma once

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
};

std::unique_ptr<LazDecoder> CreateFileDecoder(const std::string& filename,
                                              std::string& error);
std::unique_ptr<LazDecoder> CreateFileDecoder(
    const std::string& filename,
    std::vector<usdgeo::Diagnostic>& diagnostics);

struct LazReadOptions {
    std::size_t chunkPointLimit = 65536;
};

class LazReader {
public:
    explicit LazReader(std::unique_ptr<LazDecoder> decoder);

    bool Read(const LazReadOptions& options,
              const std::function<bool(const usdlas::LasHeader&,
                                       const std::vector<usdlas::LasPoint>&)>&
                  consume,
              usdlas::LasHeader& header,
              std::string& error);
    bool Read(const LazReadOptions& options,
              const std::function<bool(const usdlas::LasHeader&,
                                       const std::vector<usdlas::LasPoint>&)>&
                  consume,
              usdlas::LasHeader& header,
              std::vector<usdgeo::Diagnostic>& diagnostics);

private:
    std::unique_ptr<LazDecoder> decoder_;
};

} // namespace usdlaz
