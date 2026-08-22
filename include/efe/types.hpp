#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

namespace efe {

using OrderId = std::uint64_t;
using Quantity = std::uint32_t;
using Price = std::uint32_t;      // Nasdaq ITCH Price(4): implied four decimal places.
using Timestamp = std::uint64_t;  // Nanoseconds since midnight for ITCH event metadata.
using SequenceNumber = std::uint64_t;
using Sequence = SequenceNumber;
using SlotIndex = std::uint32_t;

constexpr SlotIndex kInvalidSlot = 0xFFFF'FFFFu;

constexpr double to_decimal_price(Price raw_price) noexcept {
    return static_cast<double>(raw_price) / 10'000.0;
}

struct Symbol {
    std::array<char, 8> bytes{};

    static Symbol from_string(std::string_view value) noexcept {
        Symbol s{};
        const auto n = value.size() < s.bytes.size() ? value.size() : s.bytes.size();
        std::memcpy(s.bytes.data(), value.data(), n);
        for (std::size_t i = n; i < s.bytes.size(); ++i) {
            s.bytes[i] = ' ';
        }
        return s;
    }

    static Symbol from_padded(std::string_view value) noexcept {
        Symbol s{};
        const auto n = value.size() < s.bytes.size() ? value.size() : s.bytes.size();
        std::memcpy(s.bytes.data(), value.data(), n);
        for (std::size_t i = n; i < s.bytes.size(); ++i) {
            s.bytes[i] = ' ';
        }
        return s;
    }

    [[nodiscard]] std::string str() const {
        std::size_t n = bytes.size();
        while (n > 0 && bytes[n - 1] == ' ') {
            --n;
        }
        return std::string(bytes.data(), n);
    }

    friend bool operator==(const Symbol&, const Symbol&) = default;
    friend bool operator<(const Symbol& a, const Symbol& b) noexcept {
        return a.bytes < b.bytes;
    }
};

struct SymbolHash {
    std::size_t operator()(const Symbol& symbol) const noexcept {
        // FNV-1a over the fixed-width eight-byte symbol.
        std::uint64_t h = 1469598103934665603ULL;
        for (char c : symbol.bytes) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }
};

inline std::ostream& operator<<(std::ostream& os, const Symbol& symbol) {
    return os << symbol.str();
}

enum class Side : std::uint8_t {
    Buy,
    Sell,
};

inline const char* to_string(Side side) noexcept {
    return side == Side::Buy ? "BUY" : "SELL";
}

struct EventMeta {
    std::uint16_t stock_locate{};
    std::uint16_t tracking_number{};
    Timestamp timestamp_ns{};
};

struct Order {
    OrderId id{};
    Symbol symbol{};
    Side side{Side::Buy};
    Quantity quantity{};
    Price price{};
};

}  // namespace efe
