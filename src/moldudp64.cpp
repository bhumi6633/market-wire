#include "efe/moldudp64.hpp"

#include "efe/byte_reader.hpp"

#include <algorithm>
#include <cstring>

namespace efe {

std::string MoldDownstreamPacket::session_string() const {
    std::size_t n = session.size();
    while (n && (session[n - 1] == ' ' || session[n - 1] == '\0')) --n;
    return std::string(session.data(), n);
}

std::array<char, 10> MoldUdp64::session_from_string(std::string_view value) {
    std::array<char, 10> out{};
    std::fill(out.begin(), out.end(), ' ');
    const auto n = std::min(value.size(), out.size());
    std::memcpy(out.data(), value.data(), n);
    return out;
}

MoldDownstreamPacket MoldUdp64::parse_downstream(std::span<const std::uint8_t> d) {
    require_bytes(d, 0, 20);
    MoldDownstreamPacket out{};
    std::memcpy(out.session.data(), d.data(), 10);
    out.first_sequence = read_u64_be(d, 10);
    out.message_count = read_u16_be(d, 18);
    if (out.message_count == 0) {
        out.heartbeat = true;
        if (d.size() != 20) throw DecodeError("MoldUDP64 heartbeat has trailing bytes");
        return out;
    }
    if (out.message_count == 0xFFFFu) {
        out.end_of_session = true;
        if (d.size() != 20) throw DecodeError("MoldUDP64 end-of-session has trailing bytes");
        return out;
    }

    out.messages.reserve(out.message_count);
    std::size_t offset = 20;
    for (std::uint16_t i = 0; i < out.message_count; ++i) {
        require_bytes(d, offset, 2);
        const auto len = read_u16_be(d, offset);
        offset += 2;
        require_bytes(d, offset, len);
        MoldMessage message;
        message.sequence = out.first_sequence + i;
        message.payload.assign(d.begin() + static_cast<std::ptrdiff_t>(offset),
                               d.begin() + static_cast<std::ptrdiff_t>(offset + len));
        out.messages.push_back(std::move(message));
        offset += len;
    }
    if (offset != d.size()) throw DecodeError("MoldUDP64 packet has trailing bytes or bad message count");
    return out;
}

std::array<std::uint8_t, 20> MoldUdp64::encode_request(const MoldRequest& r) {
    if (r.message_count == 0 || r.message_count == 0xFFFFu) throw DecodeError("invalid MoldUDP64 rerequest count");
    std::array<std::uint8_t, 20> out{};
    std::memcpy(out.data(), r.session.data(), 10);
    write_u64_be(out, 10, r.first_sequence);
    write_u16_be(out, 18, r.message_count);
    return out;
}

}  // namespace efe
