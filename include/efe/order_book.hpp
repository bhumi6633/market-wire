#pragma once

#include "efe/events.hpp"
#include "efe/flat_order_index.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace efe {

class OrderBook {
public:
    struct LevelSnapshot {
        Price price{};
        Quantity total_quantity{};
        std::vector<OrderId> fifo_order_ids;
    };

    explicit OrderBook(std::size_t max_orders = 100'000);

    bool apply(const MarketEvent& event);
    bool add(const AddEvent& event);
    bool execute(const ExecuteEvent& event);
    bool cancel(const CancelEvent& event);
    bool erase(const DeleteEvent& event);
    bool replace(const ReplaceEvent& event);

    [[nodiscard]] const Order* find(OrderId id) const noexcept;
    [[nodiscard]] std::optional<Price> best_bid(const Symbol& symbol) const;
    [[nodiscard]] std::optional<Price> best_ask(const Symbol& symbol) const;
    [[nodiscard]] std::vector<LevelSnapshot> bid_levels(const Symbol& symbol) const;
    [[nodiscard]] std::vector<LevelSnapshot> ask_levels(const Symbol& symbol) const;
    [[nodiscard]] std::size_t live_order_count() const noexcept { return index_.size(); }
    [[nodiscard]] std::size_t max_orders() const noexcept { return slots_.size(); }
    [[nodiscard]] std::uint64_t state_hash() const;

private:
    struct PriceLevel {
        Quantity total_quantity{};
        SlotIndex head{kInvalidSlot};
        SlotIndex tail{kInvalidSlot};
    };

    struct SymbolBook {
        std::map<Price, PriceLevel, std::greater<Price>> bids;
        std::map<Price, PriceLevel, std::less<Price>> asks;
    };

    struct Slot {
        Order order{};
        SlotIndex prev{kInvalidSlot};
        SlotIndex next{kInvalidSlot};
        SlotIndex next_free{kInvalidSlot};
        bool alive{false};
    };

    std::vector<Slot> slots_;
    SlotIndex free_head_{kInvalidSlot};
    FlatOrderIndex index_;
    std::map<Symbol, SymbolBook> books_;

    [[nodiscard]] SlotIndex allocate_slot() noexcept;
    void release_slot(SlotIndex slot) noexcept;
    bool remove_order(OrderId id);
    void link_at_tail(PriceLevel& level, SlotIndex slot) noexcept;
    void unlink(PriceLevel& level, SlotIndex slot) noexcept;
    void erase_empty_level(const Order& order);
};

}  // namespace efe
