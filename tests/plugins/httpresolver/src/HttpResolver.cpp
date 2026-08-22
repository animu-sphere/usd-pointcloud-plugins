#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/assetInfo.h>
#include <pxr/usd/ar/defineResolver.h>
#include <pxr/usd/ar/resolver.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

class HttpAsset final : public pxr::ArAsset {
public:
    explicit HttpAsset(std::vector<char> bytes)
        : bytes_(std::make_shared<std::vector<char>>(std::move(bytes))) {}

    std::size_t GetSize() const override { return bytes_->size(); }

    std::shared_ptr<const char> GetBuffer() const override {
        std::shared_ptr<const std::vector<char>> owner = bytes_;
        return std::shared_ptr<const char>(owner, owner->data());
    }

    std::size_t Read(void* buffer,
                     std::size_t count,
                     std::size_t offset) const override {
        if (offset > bytes_->size() || count > bytes_->size() - offset) {
            return 0;
        }
        std::memcpy(buffer, bytes_->data() + offset, count);
        return count;
    }

    std::pair<FILE*, std::size_t> GetFileUnsafe() const override {
        return {nullptr, 0};
    }

private:
    std::shared_ptr<std::vector<char>> bytes_;
};

std::shared_ptr<pxr::ArAsset> LoadTestAsset() {
    const auto* path = std::getenv("USDGEOCOPC_TEST_ASSET");
    if (!path) {
        return nullptr;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return nullptr;
    }
    const auto end = input.tellg();
    if (end < 0) {
        return nullptr;
    }
    input.seekg(0, std::ios::beg);
    std::vector<char> bytes(static_cast<std::size_t>(end));
    if (!bytes.empty() &&
        !input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        return nullptr;
    }
    return std::make_shared<HttpAsset>(std::move(bytes));
}

std::string TestAssetVersion() {
    const auto* identityMode = std::getenv("USDGEOCOPC_TEST_IDENTITY");
    if (identityMode && std::string(identityMode) == "unavailable") {
        return {};
    }
    if (identityMode && std::string(identityMode) == "unstable") {
        return " ";
    }
    const auto* path = std::getenv("USDGEOCOPC_TEST_ASSET");
    if (!path) {
        return {};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    constexpr std::uint64_t offsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;
    std::uint64_t hash = offsetBasis;
    char value = 0;
    while (input.get(value)) {
        hash ^= static_cast<unsigned char>(value);
        hash *= prime;
    }
    return input.eof() ? "test-fnv1a64:" + std::to_string(hash) : "";
}

bool IsTestMemoryUri(const std::string& path) {
    return path == "http://memory.copc" ||
           path == "https://memory.copc";
}

} // namespace

PXR_NAMESPACE_OPEN_SCOPE

class HttpResolver final : public ArResolver {
protected:
    std::string _CreateIdentifier(
        const std::string& assetPath,
        const ArResolvedPath&) const override {
        return assetPath;
    }

    std::string _CreateIdentifierForNewAsset(
        const std::string& assetPath,
        const ArResolvedPath&) const override {
        return assetPath;
    }

    ArResolvedPath _Resolve(const std::string& assetPath) const override {
        const auto* identityMode = std::getenv("USDGEOCOPC_TEST_IDENTITY");
        if (identityMode && std::string(identityMode) == "unavailable") {
            return ArResolvedPath();
        }
        if (IsTestMemoryUri(assetPath)) {
            return ArResolvedPath(assetPath);
        }
        return ArResolvedPath();
    }

    ArResolvedPath _ResolveForNewAsset(
        const std::string& assetPath) const override {
        return ArResolvedPath(assetPath);
    }

    ArAssetInfo _GetAssetInfo(
        const std::string&, const ArResolvedPath&) const override {
        return {TestAssetVersion(), {}, {}, {}};
    }

    std::shared_ptr<ArAsset> _OpenAsset(
        const ArResolvedPath& resolvedPath) const override {
        return IsTestMemoryUri(resolvedPath.GetPathString())
                   ? LoadTestAsset()
                   : nullptr;
    }

    std::shared_ptr<ArWritableAsset> _OpenAssetForWrite(
        const ArResolvedPath&, WriteMode) const override {
        return nullptr;
    }
};

AR_DEFINE_RESOLVER(HttpResolver, ArResolver);

PXR_NAMESPACE_CLOSE_SCOPE