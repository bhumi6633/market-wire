#pragma once

#include "efe/moldudp64.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace efe {

class RecoveryEngine {
public:
    struct GapRequest {
        Sequence first{};
        std::uint16_t count{};
    };

    struct Result {
        std::vector<MoldMessage> ready;
        std::optional<GapRequest> request;
        bool duplicate_or_stale{false};
        bool end_of_session{false};
    };

    explicit RecoveryEngine(Sequence first_expected = 1) : expected_(first_expected) {}

    Result ingest(const MoldDownstreamPacket& packet);
    [[nodiscard]] Sequence expected() const noexcept { return expected_; }
    [[nodiscard]] std::size_t buffered_messages() const noexcept { return pending_.size(); }
    [[nodiscard]] const std::optional<std::array<char, 10>>& session() const noexcept { return session_; }

private:
    void maybe_request(Result& result, Sequence observed_next);
    void drain(Result& result);

    Sequence expected_;
    std::map<Sequence, std::vector<std::uint8_t>> pending_;
    std::optional<std::array<char, 10>> session_;
    Sequence last_requested_first_{};
    Sequence last_requested_last_{};
};

}  // namespace efe
