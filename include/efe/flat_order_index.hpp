#pragma once

#include "efe/types.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace efe {

// Fixed-capacity open-addressing index. All memory is allocated at construction;
// inserts/erases perform no heap allocation.
class FlatOrderIndex {
public:
    explicit FlatOrderIndex(std::size_t max_entries) {
        const auto wanted = max_entries < 8 ? 16ULL : static_cast<unsigned long long>(max_entries * 2ULL);
        const std::size_t cap = std::bit_ceil(static_cast<std::size_t>(wanted));
        entries_.resize(cap);
        mask_ = cap - 1;
    }

    [[nodiscard]] std::optional<SlotIndex> find(OrderId key) const noexcept {
        std::size_t pos = hash(key) & mask_;
        for (std::size_t probes = 0; probes < entries_.size(); ++probes) {
            const Entry& e = entries_[pos];
            if (e.state == State::Empty) return std::nullopt;
            if (e.state == State::Occupied && e.key == key) return e.value;
            pos = (pos + 1) & mask_;
        }
        return std::nullopt;
    }

    bool insert(OrderId key, SlotIndex value) noexcept {
        if (size_ * 10 >= entries_.size() * 7) return false; // keep <=70% load.
        std::size_t pos = hash(key) & mask_;
        std::optional<std::size_t> tombstone;
        for (std::size_t probes = 0; probes < entries_.size(); ++probes) {
            Entry& e = entries_[pos];
            if (e.state == State::Occupied && e.key == key) return false;
            if (e.state == State::Tombstone && !tombstone) tombstone = pos;
            if (e.state == State::Empty) {
                const std::size_t target = tombstone.value_or(pos);
                entries_[target] = Entry{key, value, State::Occupied};
                ++size_;
                return true;
            }
            pos = (pos + 1) & mask_;
        }
        return false;
    }

    bool erase(OrderId key) noexcept {
        std::size_t pos = hash(key) & mask_;
        for (std::size_t probes = 0; probes < entries_.size(); ++probes) {
            Entry& e = entries_[pos];
            if (e.state == State::Empty) return false;
            if (e.state == State::Occupied && e.key == key) {
                e.state = State::Tombstone;
                --size_;
                return true;
            }
            pos = (pos + 1) & mask_;
        }
        return false;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return entries_.size(); }

private:
    enum class State : std::uint8_t { Empty, Occupied, Tombstone };
    struct Entry {
        OrderId key{};
        SlotIndex value{kInvalidSlot};
        State state{State::Empty};
    };

    static std::size_t hash(std::uint64_t x) noexcept {
        // SplitMix64 finalizer.
        x ^= x >> 30U;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27U;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31U;
        return static_cast<std::size_t>(x);
    }

    std::vector<Entry> entries_;
    std::size_t mask_{};
    std::size_t size_{};
};

}  // namespace efe
