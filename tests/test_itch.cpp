#include "efe/generated/itch50.hpp"
#include "efe/itch_decoder.hpp"
#include "test_support.hpp"

#include <array>
#include <tuple>

namespace {
bool equal(const efe::MarketEvent& left, const efe::MarketEvent& right) {
    if (left.index() != right.index()) return false;
    return std::visit([&](const auto& a) {
        using T = std::decay_t<decltype(a)>;
        const auto& b = std::get<T>(right);
        if constexpr (std::is_same_v<T, efe::AddEvent>) return a.meta.timestamp_ns == b.meta.timestamp_ns && std::tie(a.order.id,a.order.symbol,a.order.side,a.order.quantity,a.order.price)==std::tie(b.order.id,b.order.symbol,b.order.side,b.order.quantity,b.order.price);
        if constexpr (std::is_same_v<T, efe::ExecuteEvent>) return std::tie(a.order_id,a.executed_shares,a.match_number,a.has_execution_price,a.execution_price,a.printable)==std::tie(b.order_id,b.executed_shares,b.match_number,b.has_execution_price,b.execution_price,b.printable);
        if constexpr (std::is_same_v<T, efe::CancelEvent>) return a.order_id == b.order_id && a.canceled_shares == b.canceled_shares;
        if constexpr (std::is_same_v<T, efe::DeleteEvent>) return a.order_id == b.order_id;
        if constexpr (std::is_same_v<T, efe::ReplaceEvent>) return std::tie(a.old_order_id,a.new_order_id,a.new_quantity,a.new_price)==std::tie(b.old_order_id,b.new_order_id,b.new_quantity,b.new_price);
        return false;
    }, left);
}

test::Bytes book_message(char type) {
    using test::base; using test::put_ascii; using test::put_u32; using test::put_u64;
    switch (type) {
        case 'A': case 'F': {
            auto b=base(type,type=='A'?36U:40U); put_u64(b,11,0x0102030405060708ULL); b[19]='B'; put_u32(b,20,1234); put_ascii(b,24,8,"MSFT"); put_u32(b,32,2'345'678); if(type=='F')put_ascii(b,36,4,"ABCD"); return b;
        }
        case 'E': { auto b=base(type,31);put_u64(b,11,9);put_u32(b,19,10);put_u64(b,23,11);return b; }
        case 'C': { auto b=base(type,36);put_u64(b,11,9);put_u32(b,19,10);put_u64(b,23,11);b[31]='Y';put_u32(b,32,123);return b; }
        case 'X': { auto b=base(type,23);put_u64(b,11,9);put_u32(b,19,10);return b; }
        case 'D': { auto b=base(type,19);put_u64(b,11,9);return b; }
        case 'U': { auto b=base(type,35);put_u64(b,11,9);put_u64(b,19,10);put_u32(b,27,11);put_u32(b,31,12);return b; }
        default: return {};
    }
}
}

int main() {
    namespace g = efe::generated::nasdaqitch50;
    constexpr std::array<std::pair<char,std::size_t>,23> layouts{{
        {'S',12},{'R',39},{'H',25},{'Y',20},{'L',26},{'V',35},{'W',12},{'K',28},{'J',35},{'h',21},
        {'A',36},{'F',40},{'E',31},{'C',36},{'X',23},{'D',19},{'U',35},{'P',44},{'Q',40},{'B',19},{'I',50},{'N',20},{'O',48}
    }};
    efe::ItchDecoder generated;
    for (const auto& [type,size] : layouts) {
        CHECK(g::message_size(type) == size);
        auto bytes = test::base(type,size);
        generated.validate(bytes);
        CHECK(generated.message_name(bytes) != "Unknown");
        switch(type){
            case 'S':(void)g::SystemEventView(bytes).event_code();break;
            case 'R':(void)g::StockDirectoryView(bytes).inverse_indicator();break;
            case 'H':(void)g::StockTradingActionView(bytes).reason();break;
            case 'Y':(void)g::RegShoRestrictionView(bytes).reg_sho_action();break;
            case 'L':(void)g::MarketParticipantPositionView(bytes).market_participant_state();break;
            case 'V':(void)g::MwcbDeclineLevelView(bytes).level3();break;
            case 'W':(void)g::MwcbStatusView(bytes).breached_level();break;
            case 'K':(void)g::IpoQuotingPeriodUpdateView(bytes).ipo_price();break;
            case 'J':(void)g::LuldAuctionCollarView(bytes).auction_collar_extension();break;
            case 'h':(void)g::OperationalHaltView(bytes).operational_halt_action();break;
            case 'A':(void)g::AddOrderView(bytes).price();break;
            case 'F':(void)g::AddOrderWithMpidView(bytes).attribution();break;
            case 'E':(void)g::OrderExecutedView(bytes).match_number();break;
            case 'C':(void)g::OrderExecutedWithPriceView(bytes).execution_price();break;
            case 'X':(void)g::OrderCancelView(bytes).canceled_shares();break;
            case 'D':(void)g::OrderDeleteView(bytes).order_reference();break;
            case 'U':(void)g::OrderReplaceView(bytes).price();break;
            case 'P':(void)g::TradeView(bytes).match_number();break;
            case 'Q':(void)g::CrossTradeView(bytes).cross_type();break;
            case 'B':(void)g::BrokenTradeView(bytes).match_number();break;
            case 'I':(void)g::NoiiView(bytes).price_variation_indicator();break;
            case 'N':(void)g::RetailInterestView(bytes).interest_flag();break;
            case 'O':(void)g::DirectListingCapitalRaiseView(bytes).upper_price_range_collar();break;
            default:CHECK(false);
        }
        for (std::size_t length=0; length<size; ++length) test::throws<efe::DecodeError>([&]{generated.validate(std::span(bytes.data(),length));});
        auto wrong=bytes;wrong[0]='?';test::throws<efe::DecodeError>([&]{generated.validate(wrong);});
    }
    efe::ReferenceItchDecoder reference;
    for (char type : std::string_view("AFECXDU")) {
        const auto bytes=book_message(type);
        const auto a=generated.decode_book_event(bytes);const auto b=reference.decode_book_event(bytes);
        CHECK(a && b && equal(*a,*b));
    }
    const auto add=book_message('A');g::AddOrderView view(add);
    CHECK(view.stock_locate()==0x1234U);CHECK(view.tracking_number()==0x5678U);
    CHECK(view.timestamp()==0x010203040506ULL);CHECK(view.order_reference()==0x0102030405060708ULL);
    CHECK(view.stock()=="MSFT    ");CHECK(view.price()==2'345'678U);
    auto price8=test::base('V',35);test::put_u64(price8,11,0x0102030405060708ULL);g::MwcbDeclineLevelView decline(price8);
    CHECK(decline.level1()==0x0102030405060708ULL);
    auto bad_side=add;bad_side[19]='?';test::throws<efe::DecodeError>([&]{(void)generated.decode_book_event(bad_side);});
    return test::finish();
}
