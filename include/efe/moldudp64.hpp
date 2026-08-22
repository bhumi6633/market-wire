#pragma once

#include "efe/types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace efe {

struct MoldMessage {
    Sequence sequence{};
    std::vector<std::uint8_t> payload;
};

struct MoldDownstreamPacket {
    std::array<char, 10> session{};
    Sequence first_sequence{};
    std::uint16_t message_count{};
    bool heartbeat{false};
    bool end_of_session{false};
    std::vector<MoldMessage> messages;

    [[nodiscard]] std::string session_string() const;
};

struct MoldRequest {
    std::array<char, 10> session{};
    Sequence first_sequence{};
    std::uint16_t message_count{};
};

class MoldUdp64 {
public:
    static MoldDownstreamPacket parse_downstream(std::span<const std::uint8_t> datagram);
    static std::array<std::uint8_t, 20> encode_request(const MoldRequest& request);
    static std::array<char, 10> session_from_string(std::string_view session);
};

}  // namespace efe
