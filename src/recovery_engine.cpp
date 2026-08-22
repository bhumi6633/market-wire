#include "efe/recovery_engine.hpp"

#include "efe/byte_reader.hpp"

#include <algorithm>
#include <limits>

namespace efe {

void RecoveryEngine::maybe_request(Result& r, Sequence observed_next) {
    if (observed_next <= expected_) return;
    const Sequence last_missing = observed_next - 1;
    if (last_requested_first_ == expected_ && last_requested_last_ >= last_missing) return;
    const Sequence gap = last_missing - expected_ + 1;
    const auto count = static_cast<std::uint16_t>(std::min<Sequence>(gap, 0xFFFEu));
    r.request = GapRequest{expected_, count};
    last_requested_first_ = expected_;
    last_requested_last_ = expected_ + count - 1;
}

void RecoveryEngine::drain(Result& r) {
    while (true) {
        auto it = pending_.find(expected_);
        if (it == pending_.end()) break;
        r.ready.push_back(MoldMessage{it->first, std::move(it->second)});
        pending_.erase(it);
        ++expected_;
    }
    if (last_requested_last_ < expected_) {
        last_requested_first_ = last_requested_last_ = 0;
    }
}

RecoveryEngine::Result RecoveryEngine::ingest(const MoldDownstreamPacket& packet) {
    Result r;
    if (!session_) session_ = packet.session;
    if (*session_ != packet.session) throw DecodeError("MoldUDP64 session changed unexpectedly");

    if (packet.heartbeat || packet.end_of_session) {
        maybe_request(r, packet.first_sequence);
        r.end_of_session = packet.end_of_session;
        return r;
    }

    for (const auto& message : packet.messages) {
        if (message.sequence < expected_) {
            r.duplicate_or_stale = true;
            continue;
        }
        if (message.sequence == expected_) {
            r.ready.push_back(message);
            ++expected_;
            drain(r);
        } else {
            pending_.try_emplace(message.sequence, message.payload);
        }
    }

    if (!pending_.empty()) maybe_request(r, pending_.begin()->first);
    return r;
}

}  // namespace efe
