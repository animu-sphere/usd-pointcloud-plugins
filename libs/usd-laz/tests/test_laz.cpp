#include "usdlaz/Laz.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

class FakeDecoder final : public usdlaz::LazDecoder {
public:
    bool ReadHeader(usdlas::LasHeader& header, std::string&) override {
        header.pointCount = 3;
        return true;
    }

    bool ReadChunk(std::size_t maximumPoints,
                   std::vector<usdlas::LasPoint>& points,
                   bool& complete,
                   std::string&) override {
        Check(maximumPoints == 2);
        points.clear();
        if (offset_ == 0) {
            points.resize(2);
            offset_ = 2;
            complete = false;
        } else {
            points.resize(1);
            offset_ = 3;
            complete = true;
        }
        return true;
    }

private:
    std::size_t offset_ = 0;
};

void TestChunkForwarding() {
    usdlaz::LazReader reader(std::make_unique<FakeDecoder>());
    usdlaz::LazReadOptions options;
    options.chunkPointLimit = 2;
    usdlas::LasHeader header;
    std::string error;
    std::size_t chunks = 0;
    std::size_t points = 0;
    Check(reader.Read(
        options,
        [&](const usdlas::LasHeader&, const std::vector<usdlas::LasPoint>& data) {
            ++chunks;
            points += data.size();
            return true;
        },
        header, error));
    Check(error.empty());
    Check(chunks == 2);
    Check(points == 3);
}

} // namespace

int main() {
    TestChunkForwarding();
    return 0;
}
