#pragma once

#include "efe/events.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace efe {

class ItchDecoder {
public:
    // Validates any message type present in the checked-in ITCH 5.0 schema.
    void validate(std::span<const std::uint8_t> bytes) const;

    // Normalizes book-affecting messages (A/F/E/C/X/D/U). Other valid ITCH
    // messages return nullopt because they do not mutate the displayed book.
    [[nodiscard]] std::optional<MarketEvent> decode_book_event(std::span<const std::uint8_t> bytes) const;

    [[nodiscard]] std::string_view message_name(std::span<const std::uint8_t> bytes) const;
};

// Small handwritten reference decoder for differential tests/benchmarks.
class ReferenceItchDecoder {
public:
    [[nodiscard]] std::optional<MarketEvent> decode_book_event(std::span<const std::uint8_t> bytes) const;
};

}  // namespace efe
