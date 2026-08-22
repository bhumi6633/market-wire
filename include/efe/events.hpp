#pragma once

#include "efe/types.hpp"

#include <cstdint>
#include <variant>

namespace efe {

struct AddEvent {
    EventMeta meta;
    Order order;
};

struct ExecuteEvent {
    EventMeta meta;
    OrderId order_id{};
    Quantity executed_shares{};
    std::uint64_t match_number{};
    bool has_execution_price{false};
    Price execution_price{};
    bool printable{true};
};

struct CancelEvent {
    EventMeta meta;
    OrderId order_id{};
    Quantity canceled_shares{};
};

struct DeleteEvent {
    EventMeta meta;
    OrderId order_id{};
};

struct ReplaceEvent {
    EventMeta meta;
    OrderId old_order_id{};
    OrderId new_order_id{};
    Quantity new_quantity{};
    Price new_price{};
};

using MarketEvent = std::variant<AddEvent, ExecuteEvent, CancelEvent, DeleteEvent, ReplaceEvent>;

}  // namespace efe
