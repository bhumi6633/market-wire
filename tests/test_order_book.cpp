#include "efe/order_book.hpp"
#include "test_support.hpp"

#include <limits>
#include <map>
#include <random>

int main() {
    using namespace efe;
    const Symbol aapl = Symbol::from_string("AAPL");
    OrderBook book(8);
    CHECK(book.add({{}, {1, aapl, Side::Buy, 100, 1000}}));
    CHECK(book.add({{}, {2, aapl, Side::Buy, 200, 1000}}));
    CHECK(book.add({{}, {3, aapl, Side::Sell, 50, 1010}}));
    CHECK(!book.add({{}, {1, aapl, Side::Buy, 1, 999}}));
    CHECK(book.best_bid(aapl) == 1000);
    CHECK(book.best_ask(aapl) == 1010);
    CHECK(book.bid_levels(aapl)[0].fifo_order_ids == std::vector<OrderId>({1, 2}));
    CHECK(!book.execute({{}, 99, 1, 0, false, 0, true}));
    CHECK(!book.execute({{}, 1, 101, 0, false, 0, true}));
    CHECK(book.execute({{}, 1, 25, 0, false, 0, true}));
    CHECK(book.find(1)->quantity == 75);
    CHECK(!book.cancel({{}, 1, 76}));
    CHECK(book.cancel({{}, 1, 75}));
    CHECK(book.bid_levels(aapl)[0].fifo_order_ids == std::vector<OrderId>({2}));
    CHECK(book.erase({{}, 3}));
    CHECK(!book.best_ask(aapl));
    CHECK(!book.erase({{}, 3}));
    CHECK(book.replace({{}, 2, 4, 150, 1005}));
    CHECK(!book.find(2) && book.find(4));

    OrderBook full(1);
    CHECK(full.add({{}, {10, aapl, Side::Buy, 10, 10}}));
    CHECK(full.replace({{}, 10, 11, 20, 11}));
    CHECK(!full.find(10) && full.find(11));
    CHECK(!full.replace({{}, 11, 0, 20, 12}));
    CHECK(full.find(11));
    CHECK(!full.add({{}, {12, aapl, Side::Sell, 1, 12}}));
    CHECK(full.erase({{},11}));
    CHECK(full.add({{}, {12, aapl, Side::Sell, 1, 12}}));
    CHECK(full.live_order_count()==1);

    const Symbol msft=Symbol::from_string("MSFT");
    OrderBook isolated(4);
    CHECK(isolated.add({{}, {13,aapl,Side::Buy,1,100}}));
    CHECK(isolated.add({{}, {14,msft,Side::Sell,1,200}}));
    CHECK(isolated.best_bid(aapl)==100&& !isolated.best_ask(aapl));
    CHECK(isolated.best_ask(msft)==200&& !isolated.best_bid(msft));

    OrderBook overflow(4);
    CHECK(overflow.add({{}, {20, aapl, Side::Buy, std::numeric_limits<Quantity>::max(), 500}}));
    CHECK(!overflow.add({{}, {21, aapl, Side::Buy, 1, 500}}));
    CHECK(!overflow.find(21));
    CHECK(overflow.bid_levels(aapl).size() == 1);

    OrderBook first(4), second(4);
    CHECK(first.add({{}, {30, aapl, Side::Buy, 10, 700}}));
    CHECK(first.add({{}, {31, aapl, Side::Buy, 20, 700}}));
    CHECK(second.add({{}, {31, aapl, Side::Buy, 20, 700}}));
    CHECK(second.add({{}, {30, aapl, Side::Buy, 10, 700}}));
    CHECK(first.state_hash() != second.state_hash());

    struct ModelOrder { Side side; Quantity quantity; Price price; std::uint64_t priority; };
    OrderBook random_book(2048);
    std::map<OrderId, ModelOrder> model;
    std::mt19937_64 random(0xEFE5EEDULL);
    constexpr std::uint64_t seed = 0xEFE5EEDULL;
    OrderId next_id = 100;
    std::uint64_t priority = 0;
    for (std::size_t transition = 0; transition < 100'000; ++transition) {
        if (model.empty() || (random() % 3U == 0U && model.size() < 1500)) {
            const Quantity quantity = static_cast<Quantity>(random() % 1000U + 1U);
            const Side side = random() % 2U ? Side::Buy : Side::Sell;
            const Price price = static_cast<Price>(random() % 100U + 1U);
            CHECK(random_book.add({{}, {next_id, aapl, side, quantity, price}}));
            model[next_id] = ModelOrder{side,quantity,price,priority++};
            ++next_id;
        } else {
            auto it = model.begin();
            std::advance(it, static_cast<std::ptrdiff_t>(random() % model.size()));
            const OrderId id = it->first;
            const Quantity quantity = static_cast<Quantity>(random() % it->second.quantity + 1U);
            switch (random() % 4U) {
                case 0:
                    CHECK(random_book.cancel({{},id,quantity}));
                    it->second.quantity-=quantity;
                    if(it->second.quantity==0)model.erase(it);
                    break;
                case 1:
                    CHECK(random_book.execute({{},id,quantity,transition,false,0,true}));
                    it->second.quantity-=quantity;
                    if(it->second.quantity==0)model.erase(it);
                    break;
                case 2:
                    CHECK(random_book.erase({{},id}));model.erase(it);break;
                default: {
                    const Quantity new_quantity=static_cast<Quantity>(random()%1000U+1U);
                    const Price new_price=static_cast<Price>(random()%100U+1U);
                    CHECK(random_book.replace({{},id,next_id,new_quantity,new_price}));
                    const Side side=it->second.side;model.erase(it);model[next_id]=ModelOrder{side,new_quantity,new_price,priority++};++next_id;break;
                }
            }
        }
        CHECK(random_book.live_order_count() == model.size());
        if (transition % 250U == 0U) {
            std::map<Price,std::vector<std::pair<std::uint64_t,OrderId>>,std::greater<Price>> bids;
            std::map<Price,std::vector<std::pair<std::uint64_t,OrderId>>,std::less<Price>> asks;
            std::map<Price,std::uint64_t,std::greater<Price>> bid_totals;
            std::map<Price,std::uint64_t,std::less<Price>> ask_totals;
            for (const auto& [id,order] : model) {
                const Order* actual=random_book.find(id);CHECK(actual&&actual->quantity==order.quantity&&actual->price==order.price&&actual->side==order.side);
                if(order.side==Side::Buy){bids[order.price].push_back({order.priority,id});bid_totals[order.price]+=order.quantity;}
                else{asks[order.price].push_back({order.priority,id});ask_totals[order.price]+=order.quantity;}
            }
            const auto actual_bids=random_book.bid_levels(aapl);const auto actual_asks=random_book.ask_levels(aapl);
            CHECK(actual_bids.size()==bids.size());CHECK(actual_asks.size()==asks.size());
            std::size_t level=0;for(auto&[price,orders]:bids){std::sort(orders.begin(),orders.end());std::vector<OrderId>ids;for(auto entry:orders)ids.push_back(entry.second);CHECK(actual_bids[level].price==price&&actual_bids[level].total_quantity==bid_totals[price]&&actual_bids[level].fifo_order_ids==ids);++level;}
            level=0;for(auto&[price,orders]:asks){std::sort(orders.begin(),orders.end());std::vector<OrderId>ids;for(auto entry:orders)ids.push_back(entry.second);CHECK(actual_asks[level].price==price&&actual_asks[level].total_quantity==ask_totals[price]&&actual_asks[level].fifo_order_ids==ids);++level;}
            CHECK(random_book.best_bid(aapl)==(bids.empty()?std::optional<Price>{}:std::optional<Price>{bids.begin()->first}));
            CHECK(random_book.best_ask(aapl)==(asks.empty()?std::optional<Price>{}:std::optional<Price>{asks.begin()->first}));
            if(test::failures){std::cerr<<"random seed="<<seed<<" transition="<<transition<<'\n';break;}
        }
    }
    return test::finish();
}
