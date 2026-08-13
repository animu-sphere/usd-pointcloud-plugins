#pragma once

#include "usdgeo/Diagnostic.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace usdgeo {

class RandomAccessSource {
public:
    virtual ~RandomAccessSource() = default;

    virtual bool GetSize(std::uint64_t& size,
                         std::vector<Diagnostic>& diagnostics) const = 0;
    virtual bool Read(std::uint64_t offset,
                      std::size_t size,
                      std::vector<std::uint8_t>& bytes,
                      std::vector<Diagnostic>& diagnostics) = 0;

    virtual std::uint64_t BytesRead() const noexcept { return 0; }
};

class LocalRandomAccessSource final : public RandomAccessSource {
public:
    explicit LocalRandomAccessSource(std::string filename);

    bool GetSize(std::uint64_t& size,
                 std::vector<Diagnostic>& diagnostics) const override;
    bool Read(std::uint64_t offset,
              std::size_t size,
              std::vector<std::uint8_t>& bytes,
              std::vector<Diagnostic>& diagnostics) override;
    std::uint64_t BytesRead() const noexcept override;

private:
    std::string filename_;
    std::uint64_t bytesRead_ = 0;
};

std::unique_ptr<RandomAccessSource> OpenLocalRandomAccessSource(
    const std::string& filename);

} // namespace usdgeo