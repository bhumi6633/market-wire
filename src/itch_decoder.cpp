#include "efe/itch_decoder.hpp"

#include "efe/byte_reader.hpp"
#include "efe/generated/itch50.hpp"

#include <string>

namespace efe {
namespace gen = generated::nasdaqitch50;

namespace {
EventMeta meta(std::uint16_t locate, std::uint16_t tracking, std::uint64_t ts) {
    return EventMeta{locate, tracking, ts};
}

Side side_from_char(char c) {
    if (c == 'B') return Side::Buy;
    if (c == 'S') return Side::Sell;
    throw DecodeError("invalid ITCH side");
}

Symbol symbol_from_view(std::string_view s) { return Symbol::from_padded(s); }

void require_exact(std::span<const std::uint8_t> bytes, std::size_t size, char type) {
    if (bytes.size() != size || bytes.empty() || bytes[0] != static_cast<std::uint8_t>(type)) {
        throw DecodeError("invalid ITCH message size/type");
    }
}
}

void ItchDecoder::validate(std::span<const std::uint8_t> bytes) const {
    if (bytes.empty()) throw DecodeError("empty ITCH message");
    const char type = static_cast<char>(bytes[0]);
    const auto expected = gen::message_size(type);
    if (!expected) throw DecodeError("unsupported ITCH message type");
    if (bytes.size() != *expected) throw DecodeError("ITCH message length mismatch");
}

std::string_view ItchDecoder::message_name(std::span<const std::uint8_t> bytes) const {
    if (bytes.empty()) return "Empty";
    return gen::message_name(static_cast<char>(bytes[0]));
}

std::optional<MarketEvent> ItchDecoder::decode_book_event(std::span<const std::uint8_t> bytes) const {
    validate(bytes);
    const char type = static_cast<char>(bytes[0]);
    switch (type) {
        case 'A': {
            gen::AddOrderView v(bytes);
            return AddEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()),
                            Order{v.order_reference(), symbol_from_view(v.stock()), side_from_char(v.side()), v.shares(), v.price()}};
        }
        case 'F': {
            gen::AddOrderWithMpidView v(bytes);
            return AddEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()),
                            Order{v.order_reference(), symbol_from_view(v.stock()), side_from_char(v.side()), v.shares(), v.price()}};
        }
        case 'E': {
            gen::OrderExecutedView v(bytes);
            return ExecuteEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()), v.order_reference(),
                                v.executed_shares(), v.match_number(), false, 0, true};
        }
        case 'C': {
            gen::OrderExecutedWithPriceView v(bytes);
            return ExecuteEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()), v.order_reference(),
                                v.executed_shares(), v.match_number(), true, v.execution_price(), v.printable() == 'Y'};
        }
        case 'X': {
            gen::OrderCancelView v(bytes);
            return CancelEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()), v.order_reference(), v.canceled_shares()};
        }
        case 'D': {
            gen::OrderDeleteView v(bytes);
            return DeleteEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()), v.order_reference()};
        }
        case 'U': {
            gen::OrderReplaceView v(bytes);
            return ReplaceEvent{meta(v.stock_locate(), v.tracking_number(), v.timestamp()), v.original_order_reference(),
                                v.new_order_reference(), v.shares(), v.price()};
        }
        default:
            return std::nullopt;
    }
}

std::optional<MarketEvent> ReferenceItchDecoder::decode_book_event(std::span<const std::uint8_t> b) const {
    if (b.empty()) throw DecodeError("empty ITCH message");
    const char t = static_cast<char>(b[0]);
    const auto m = [&]() { return EventMeta{read_u16_be(b,1), read_u16_be(b,3), read_u48_be(b,5)}; };
    switch (t) {
        case 'A':
            require_exact(b, 36, 'A');
            return AddEvent{m(), Order{read_u64_be(b,11), Symbol::from_padded(read_ascii_view(b,24,8)),
                                      side_from_char(read_char(b,19)), read_u32_be(b,20), read_u32_be(b,32)}};
        case 'F':
            require_exact(b, 40, 'F');
            return AddEvent{m(), Order{read_u64_be(b,11), Symbol::from_padded(read_ascii_view(b,24,8)),
                                      side_from_char(read_char(b,19)), read_u32_be(b,20), read_u32_be(b,32)}};
        case 'E':
            require_exact(b, 31, 'E');
            return ExecuteEvent{m(), read_u64_be(b,11), read_u32_be(b,19), read_u64_be(b,23), false, 0, true};
        case 'C':
            require_exact(b, 36, 'C');
            return ExecuteEvent{m(), read_u64_be(b,11), read_u32_be(b,19), read_u64_be(b,23), true,
                                read_u32_be(b,32), read_char(b,31) == 'Y'};
        case 'X':
            require_exact(b, 23, 'X');
            return CancelEvent{m(), read_u64_be(b,11), read_u32_be(b,19)};
        case 'D':
            require_exact(b, 19, 'D');
            return DeleteEvent{m(), read_u64_be(b,11)};
        case 'U':
            require_exact(b, 35, 'U');
            return ReplaceEvent{m(), read_u64_be(b,11), read_u64_be(b,19), read_u32_be(b,27), read_u32_be(b,31)};
        default:
            return std::nullopt;
    }
}

}  // namespace efe
