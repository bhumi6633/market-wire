#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace efe {

struct CaptureRecord {
    std::uint64_t monotonic_ns{};
    std::vector<std::uint8_t> datagram;
};

class CaptureWriter {
public:
    explicit CaptureWriter(const std::string& path);
    void write(std::uint64_t monotonic_ns, std::span<const std::uint8_t> datagram);

private:
    std::ofstream out_;
};

class CaptureReader {
public:
    explicit CaptureReader(const std::string& path);
    std::optional<CaptureRecord> next();

private:
    std::ifstream in_;
};

}  // namespace efe
