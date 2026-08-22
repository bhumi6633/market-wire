#include "efe/order_book.hpp"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace efe {

namespace {
void hash_mix(std::uint64_t& h, std::uint64_t value) noexcept {
    h ^= value + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
}
}

OrderBook::OrderBook(std::size_t max_orders)
    : slots_(max_orders), index_(max_orders) {
    if (max_orders == 0 || max_orders >= static_cast<std::size_t>(kInvalidSlot)) {
        throw std::invalid_argument("max_orders must be between 1 and UINT32_MAX-1");
    }
    for (std::size_t i = 0; i < max_orders; ++i) {
        slots_[i].next_free = (i + 1 < max_orders) ? static_cast<SlotIndex>(i + 1) : kInvalidSlot;
    }
    free_head_ = 0;
}

SlotIndex OrderBook::allocate_slot() noexcept {
    if (free_head_ == kInvalidSlot) return kInvalidSlot;
    const SlotIndex slot = free_head_;
    free_head_ = slots_[slot].next_free;
    slots_[slot].next_free = kInvalidSlot;
    slots_[slot].alive = true;
    return slot;
}

void OrderBook::release_slot(SlotIndex slot) noexcept {
    slots_[slot] = Slot{};
    slots_[slot].next_free = free_head_;
    free_head_ = slot;
}

bool OrderBook::apply(const MarketEvent& event) {
    return std::visit([this](const auto& e) -> bool {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, AddEvent>) return add(e);
        if constexpr (std::is_same_v<T, ExecuteEvent>) return execute(e);
        if constexpr (std::is_same_v<T, CancelEvent>) return cancel(e);
        if constexpr (std::is_same_v<T, DeleteEvent>) return erase(e);
        if constexpr (std::is_same_v<T, ReplaceEvent>) return replace(e);
        return false;
    }, event);
}

void OrderBook::link_at_tail(PriceLevel& level, SlotIndex slot) noexcept {
    slots_[slot].prev = level.tail;
    slots_[slot].next = kInvalidSlot;
    if (level.tail != kInvalidSlot) slots_[level.tail].next = slot;
    else level.head = slot;
    level.tail = slot;
}

void OrderBook::unlink(PriceLevel& level, SlotIndex slot) noexcept {
    const auto prev = slots_[slot].prev;
    const auto next = slots_[slot].next;
    if (prev != kInvalidSlot) slots_[prev].next = next; else level.head = next;
    if (next != kInvalidSlot) slots_[next].prev = prev; else level.tail = prev;
    slots_[slot].prev = slots_[slot].next = kInvalidSlot;
}

bool OrderBook::add(const AddEvent& event) {
    const Order& order = event.order;
    if (order.id == 0 || order.quantity == 0 || index_.find(order.id)) return false;

    const SlotIndex slot = allocate_slot();
    if (slot == kInvalidSlot) return false;
    slots_[slot].order = order;

    if (!index_.insert(order.id, slot)) {
        release_slot(slot);
        return false;
    }

    try {
        auto& symbol_book = books_[order.symbol];
        const auto add_to = [&](auto& levels) {
            auto [level_it, inserted] = levels.try_emplace(order.price);
            auto& level = level_it->second;
            if (std::numeric_limits<Quantity>::max() - level.total_quantity < order.quantity) {
                if (inserted) levels.erase(level_it);
                return false;
            }
            level.total_quantity += order.quantity;
            link_at_tail(level, slot);
            return true;
        };
        const bool added = order.side == Side::Buy ? add_to(symbol_book.bids) : add_to(symbol_book.asks);
        if (added) return true;
        if (symbol_book.bids.empty() && symbol_book.asks.empty()) books_.erase(order.symbol);
        index_.erase(order.id);
        release_slot(slot);
        return false;
    } catch (...) {
        index_.erase(order.id);
        release_slot(slot);
        auto book_it = books_.find(order.symbol);
        if (book_it != books_.end() && book_it->second.bids.empty() && book_it->second.asks.empty()) books_.erase(book_it);
        throw;
    }
}

bool OrderBook::execute(const ExecuteEvent& event) {
    const auto slot_opt = index_.find(event.order_id);
    if (!slot_opt || event.executed_shares == 0) return false;
    Slot& slot = slots_[*slot_opt];
    if (event.executed_shares > slot.order.quantity) return false;

    auto& book = books_.at(slot.order.symbol);
    auto& level = slot.order.side == Side::Buy ? book.bids.at(slot.order.price) : book.asks.at(slot.order.price);
    level.total_quantity -= event.executed_shares;
    slot.order.quantity -= event.executed_shares;
    if (slot.order.quantity == 0) return remove_order(event.order_id);
    return true;
}

bool OrderBook::cancel(const CancelEvent& event) {
    const auto slot_opt = index_.find(event.order_id);
    if (!slot_opt || event.canceled_shares == 0) return false;
    Slot& slot = slots_[*slot_opt];
    if (event.canceled_shares > slot.order.quantity) return false;

    auto& book = books_.at(slot.order.symbol);
    auto& level = slot.order.side == Side::Buy ? book.bids.at(slot.order.price) : book.asks.at(slot.order.price);
    level.total_quantity -= event.canceled_shares;
    slot.order.quantity -= event.canceled_shares;
    if (slot.order.quantity == 0) return remove_order(event.order_id);
    return true;
}

bool OrderBook::erase(const DeleteEvent& event) { return remove_order(event.order_id); }

bool OrderBook::replace(const ReplaceEvent& event) {
    const auto old_slot = index_.find(event.old_order_id);
    if (!old_slot || event.new_order_id == 0 || event.new_quantity == 0 || index_.find(event.new_order_id)) return false;
    Slot& slot = slots_[*old_slot];
    const Order old = slot.order;
    auto& symbol_book = books_.at(old.symbol);
    const auto replace_in = [&](auto& levels) {
        auto old_level_it = levels.find(old.price);
        if (old.price == event.new_price) {
            auto& level = old_level_it->second;
            const auto without_old = static_cast<std::uint64_t>(level.total_quantity) - old.quantity;
            if (without_old + event.new_quantity > std::numeric_limits<Quantity>::max()) return false;
            if (!index_.insert(event.new_order_id, *old_slot)) return false;
            unlink(level, *old_slot);
            level.total_quantity = static_cast<Quantity>(without_old + event.new_quantity);
            slot.order.id = event.new_order_id;
            slot.order.quantity = event.new_quantity;
            link_at_tail(level, *old_slot);
            index_.erase(event.old_order_id);
            return true;
        }
        auto [new_level_it, inserted] = levels.try_emplace(event.new_price);
        auto& new_level = new_level_it->second;
        if (std::numeric_limits<Quantity>::max() - new_level.total_quantity < event.new_quantity) {
            if (inserted) levels.erase(new_level_it);
            return false;
        }
        if (!index_.insert(event.new_order_id, *old_slot)) {
            if (inserted) levels.erase(new_level_it);
            return false;
        }
        auto& old_level = old_level_it->second;
        unlink(old_level, *old_slot);
        old_level.total_quantity -= old.quantity;
        slot.order.id = event.new_order_id;
        slot.order.quantity = event.new_quantity;
        slot.order.price = event.new_price;
        new_level.total_quantity += event.new_quantity;
        link_at_tail(new_level, *old_slot);
        index_.erase(event.old_order_id);
        if (old_level.head == kInvalidSlot) levels.erase(old_level_it);
        return true;
    };
    return old.side == Side::Buy ? replace_in(symbol_book.bids) : replace_in(symbol_book.asks);
}

bool OrderBook::remove_order(OrderId id) {
    const auto slot_opt = index_.find(id);
    if (!slot_opt) return false;
    const SlotIndex idx = *slot_opt;
    const Order order = slots_[idx].order;
    auto& book = books_.at(order.symbol);
    auto& level = order.side == Side::Buy ? book.bids.at(order.price) : book.asks.at(order.price);
    level.total_quantity -= order.quantity;
    unlink(level, idx);
    index_.erase(id);
    release_slot(idx);
    erase_empty_level(order);
    return true;
}

void OrderBook::erase_empty_level(const Order& order) {
    auto book_it = books_.find(order.symbol);
    if (book_it == books_.end()) return;
    auto& book = book_it->second;
    if (order.side == Side::Buy) {
        auto it = book.bids.find(order.price);
        if (it != book.bids.end() && it->second.head == kInvalidSlot) book.bids.erase(it);
    } else {
        auto it = book.asks.find(order.price);
        if (it != book.asks.end() && it->second.head == kInvalidSlot) book.asks.erase(it);
    }
    if (book.bids.empty() && book.asks.empty()) books_.erase(book_it);
}

const Order* OrderBook::find(OrderId id) const noexcept {
    const auto slot = index_.find(id);
    return slot ? &slots_[*slot].order : nullptr;
}

std::optional<Price> OrderBook::best_bid(const Symbol& symbol) const {
    auto it = books_.find(symbol);
    if (it == books_.end() || it->second.bids.empty()) return std::nullopt;
    return it->second.bids.begin()->first;
}

std::optional<Price> OrderBook::best_ask(const Symbol& symbol) const {
    auto it = books_.find(symbol);
    if (it == books_.end() || it->second.asks.empty()) return std::nullopt;
    return it->second.asks.begin()->first;
}

std::vector<OrderBook::LevelSnapshot> OrderBook::bid_levels(const Symbol& symbol) const {
    std::vector<LevelSnapshot> out;
    auto it = books_.find(symbol);
    if (it == books_.end()) return out;
    out.reserve(it->second.bids.size());
    for (const auto& [price, level] : it->second.bids) {
        LevelSnapshot snap{price, level.total_quantity, {}};
        for (auto slot = level.head; slot != kInvalidSlot; slot = slots_[slot].next) snap.fifo_order_ids.push_back(slots_[slot].order.id);
        out.push_back(std::move(snap));
    }
    return out;
}

std::vector<OrderBook::LevelSnapshot> OrderBook::ask_levels(const Symbol& symbol) const {
    std::vector<LevelSnapshot> out;
    auto it = books_.find(symbol);
    if (it == books_.end()) return out;
    out.reserve(it->second.asks.size());
    for (const auto& [price, level] : it->second.asks) {
        LevelSnapshot snap{price, level.total_quantity, {}};
        for (auto slot = level.head; slot != kInvalidSlot; slot = slots_[slot].next) snap.fifo_order_ids.push_back(slots_[slot].order.id);
        out.push_back(std::move(snap));
    }
    return out;
}

std::uint64_t OrderBook::state_hash() const {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    hash_mix(h, books_.size());
    const auto hash_levels = [this, &h](const auto& levels, Side side) {
        hash_mix(h, static_cast<std::uint8_t>(side));
        hash_mix(h, levels.size());
        for (const auto& [price, level] : levels) {
            hash_mix(h, price);
            hash_mix(h, level.total_quantity);
            for (auto slot = level.head; slot != kInvalidSlot; slot = slots_[slot].next) {
                const Order& order = slots_[slot].order;
                hash_mix(h, order.id);
                hash_mix(h, order.quantity);
            }
            hash_mix(h, kInvalidSlot);
        }
    };
    for (const auto& [symbol, book] : books_) {
        for (char c : symbol.bytes) hash_mix(h, static_cast<unsigned char>(c));
        hash_levels(book.bids, Side::Buy);
        hash_levels(book.asks, Side::Sell);
    }
    return h;
}

}  // namespace efe
