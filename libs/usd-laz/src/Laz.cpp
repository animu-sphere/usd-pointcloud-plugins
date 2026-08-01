#include "usdlaz/Laz.h"

#include <limits>
#include <utility>

namespace usdlaz {

LazReader::LazReader(std::unique_ptr<LazDecoder> decoder)
    : decoder_(std::move(decoder)) {}

bool LazReader::Read(
    const LazReadOptions& options,
    const std::function<bool(const usdlas::LasHeader&,
                             const std::vector<usdlas::LasPoint>&)>& consume,
    usdlas::LasHeader& header,
    std::string& error) {
    if (!decoder_) {
        error = "LAZ decoder is not configured";
        return false;
    }
    if (options.chunkPointLimit == 0 || !consume) {
        error = "LAZ read options or consumer are invalid";
        return false;
    }
    if (!decoder_->ReadHeader(header, error)) {
        return false;
    }

    std::uint64_t pointsRead = 0;
    bool complete = false;
    while (!complete) {
        std::vector<usdlas::LasPoint> points;
        if (!decoder_->ReadChunk(options.chunkPointLimit, points, complete,
                                 error)) {
            return false;
        }
        if (points.empty() && !complete) {
            error = "LAZ decoder returned an empty incomplete chunk";
            return false;
        }
        if (points.size() > options.chunkPointLimit) {
            error = "LAZ decoder exceeded the requested chunk size";
            return false;
        }
        if (points.size() > header.pointCount - pointsRead ||
            pointsRead > std::numeric_limits<std::uint64_t>::max() -
                              points.size()) {
            error = "LAZ decoder returned too many points";
            return false;
        }
        pointsRead += points.size();
        if (!consume(header, points)) {
            error = "LAZ chunk consumer rejected a chunk";
            return false;
        }
    }
    if (pointsRead != header.pointCount) {
        error = "LAZ decoder point count does not match the header";
        return false;
    }
    return true;
}

} // namespace usdlaz
