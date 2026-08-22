#include "efe/itch_decoder.hpp"
#include "efe/moldudp64.hpp"
#include "efe/order_book.hpp"
#include "efe/recovery_engine.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace {
constexpr std::uint64_t kSeed = 0xC0171'6A9ULL;
constexpr std::size_t kMessageCount = 50'000;

test::Bytes add_message(std::uint64_t id, std::uint32_t quantity, std::uint32_t price) {
    auto bytes = test::base('A', 36);
    test::put_u64(bytes, 11, id);
    bytes[19] = static_cast<std::uint8_t>('B');
    test::put_u32(bytes, 20, quantity);
    test::put_ascii(bytes, 24, 8, "PIPE");
    test::put_u32(bytes, 32, price);
    return bytes;
}

test::Bytes delete_message(std::uint64_t id) {
    auto bytes = test::base('D', 19);
    test::put_u64(bytes, 11, id);
    return bytes;
}

void apply_payload(const test::Bytes& payload, efe::ItchDecoder& decoder, efe::OrderBook& book) {
    const auto event = decoder.decode_book_event(payload);
    CHECK(event && book.apply(*event));
}
}  // namespace

int main() {
    std::vector<test::Bytes> messages;
    messages.reserve(kMessageCount);
    for (std::size_t i = 0; i < kMessageCount / 2; ++i) {
        const auto id = static_cast<std::uint64_t>(i + 1);
        messages.push_back(add_message(id, static_cast<std::uint32_t>(i % 100 + 1),
                                       static_cast<std::uint32_t>(1'000'000 + i % 200)));
        messages.push_back(delete_message(id));
    }

    efe::ItchDecoder decoder;
    efe::OrderBook ordered_book(1024);
    for (const auto& message : messages) apply_payload(message, decoder, ordered_book);

    std::vector<std::size_t> arrival_order(messages.size());
    std::iota(arrival_order.begin(), arrival_order.end(), 0);
    std::mt19937_64 random(kSeed);
    std::shuffle(arrival_order.begin(), arrival_order.end(), random);

    efe::RecoveryEngine recovery(1);
    efe::OrderBook recovered_book(1024);
    efe::Sequence last_released = 0;
    std::size_t released = 0;
    std::size_t requests = 0;
    for (const std::size_t index : arrival_order) {
        const auto packet = efe::MoldUdp64::parse_downstream(
            test::mold(static_cast<efe::Sequence>(index + 1), {messages[index]}));
        const auto result = recovery.ingest(packet);
        if (result.request) ++requests;
        for (const auto& ready : result.ready) {
            CHECK(ready.sequence == last_released + 1);
            last_released = ready.sequence;
            ++released;
            apply_payload(ready.payload, decoder, recovered_book);
        }
        if (index % 997U == 0U) {
            const auto duplicate = recovery.ingest(packet);
            for (const auto& ready : duplicate.ready) {
                CHECK(ready.sequence == last_released + 1);
                last_released = ready.sequence;
                ++released;
                apply_payload(ready.payload, decoder, recovered_book);
            }
        }
    }

    CHECK(released == messages.size());
    CHECK(last_released == messages.size());
    CHECK(recovery.expected() == messages.size() + 1);
    CHECK(recovery.buffered_messages() == 0);
    CHECK(requests > 0);
    CHECK(recovered_book.live_order_count() == 0);
    CHECK(recovered_book.state_hash() == ordered_book.state_hash());
    if (test::failures != 0) std::cerr << "pipeline property seed=" << kSeed << '\n';
    return test::finish();
}
