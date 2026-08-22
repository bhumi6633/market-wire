#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace efe {

struct LatencySummary {
    double mean_ns{};
    std::uint64_t p50_ns{};
    std::uint64_t p95_ns{};
    std::uint64_t p99_ns{};
    std::uint64_t p999_ns{};
    std::uint64_t max_ns{};
};

class LatencySamples {
public:
    explicit LatencySamples(std::size_t reserve = 0) { samples_.reserve(reserve); }
    void add(std::uint64_t ns) { samples_.push_back(ns); }
    [[nodiscard]] LatencySummary summarize() const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
private:
    std::vector<std::uint64_t> samples_;
};

}  // namespace efe
