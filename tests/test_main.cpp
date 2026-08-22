#include "efe/byte_reader.hpp"
#include "efe/capture.hpp"
#include "efe/generated/itch50.hpp"
#include "efe/itch_decoder.hpp"
#include "efe/moldudp64.hpp"
#include "efe/order_book.hpp"
#include "efe/recovery_engine.hpp"
#include "efe/replay.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){std::cerr<<"CHECK failed "<<__FILE__<<":"<<__LINE__<<" "#x"\n";++failures;}}while(false)
using Bytes=std::vector<std::uint8_t>;
void p16(Bytes& b,std::size_t o,std::uint16_t v){b[o]=v>>8U;b[o+1]=v;}
void p32(Bytes& b,std::size_t o,std::uint32_t v){b[o]=v>>24U;b[o+1]=v>>16U;b[o+2]=v>>8U;b[o+3]=v;}
void p64(Bytes& b,std::size_t o,std::uint64_t v){for(int i=7;i>=0;--i)b[o+static_cast<std::size_t>(7-i)]=v>>(i*8);}
void p48(Bytes& b,std::size_t o,std::uint64_t v){for(int i=5;i>=0;--i)b[o+static_cast<std::size_t>(5-i)]=v>>(i*8);}
void ascii(Bytes& b,std::size_t o,std::size_t n,std::string_view s){for(std::size_t i=0;i<n;++i)b[o+i]=i<s.size()?s[i]:' ';}
Bytes add(std::uint64_t id,char side,std::uint32_t qty,std::uint32_t price){Bytes b(36);b[0]='A';p16(b,1,7);p16(b,3,9);p48(b,5,123456);p64(b,11,id);b[19]=side;p32(b,20,qty);ascii(b,24,8,"AAPL");p32(b,32,price);return b;}
Bytes cancel(std::uint64_t id,std::uint32_t qty){Bytes b(23);b[0]='X';p16(b,1,7);p16(b,3,10);p48(b,5,123457);p64(b,11,id);p32(b,19,qty);return b;}
Bytes mold(std::string_view s,efe::Sequence first,const std::vector<Bytes>& msgs){std::size_t n=20;for(auto&m:msgs)n+=2+m.size();Bytes d(n);for(std::size_t i=0;i<10;++i)d[i]=i<s.size()?s[i]:' ';p64(d,10,first);p16(d,18,static_cast<std::uint16_t>(msgs.size()));std::size_t o=20;for(auto&m:msgs){p16(d,o,static_cast<std::uint16_t>(m.size()));o+=2;std::memcpy(d.data()+o,m.data(),m.size());o+=m.size();}return d;}

void test_generated_schema(){
    namespace g=efe::generated::nasdaqitch50;
    CHECK(g::AddOrderView::encoded_size==36); CHECK(g::OrderExecutedView::encoded_size==31); CHECK(g::OrderReplaceView::encoded_size==35);
    CHECK(g::StockDirectoryView::encoded_size==39); CHECK(g::NoiiView::encoded_size==50); CHECK(g::DirectListingCapitalRaiseView::encoded_size==48);
    CHECK(g::message_size('Q')==40); CHECK(g::message_name('A')=="AddOrder");
}

void test_differential_decoder(){
    efe::ItchDecoder generated; efe::ReferenceItchDecoder reference; auto b=add(100,'B',250,1'234'500);
    const auto a=generated.decode_book_event(b); const auto r=reference.decode_book_event(b); CHECK(a.has_value()&&r.has_value());
    const auto* ga=std::get_if<efe::AddEvent>(&*a); const auto* ra=std::get_if<efe::AddEvent>(&*r); CHECK(ga&&ra);
    CHECK(ga->order.id==ra->order.id); CHECK(ga->order.quantity==ra->order.quantity); CHECK(ga->order.price==ra->order.price); CHECK(ga->order.symbol==ra->order.symbol);
}

void test_arena_book(){
    efe::OrderBook b(8); const auto s=efe::Symbol::from_string("AAPL");
    CHECK(b.add({{}, {10,s,efe::Side::Buy,100,500000}})); CHECK(b.add({{}, {11,s,efe::Side::Buy,200,500000}})); CHECK(b.add({{}, {12,s,efe::Side::Sell,50,501000}}));
    CHECK(b.best_bid(s)==500000); CHECK(b.best_ask(s)==501000); auto levels=b.bid_levels(s); CHECK(levels[0].fifo_order_ids[0]==10&&levels[0].fifo_order_ids[1]==11);
    CHECK(b.cancel({{},10,25})); CHECK(b.find(10)->quantity==75); CHECK(b.erase({{},10})); CHECK(!b.find(10));
    CHECK(b.replace({{},11,20,150,500500})); CHECK(!b.find(11)); CHECK(b.find(20)&&b.find(20)->price==500500);
}

void test_mold_recovery(){
    efe::RecoveryEngine recovery(1); auto p1=efe::MoldUdp64::parse_downstream(mold("TEST",1,{add(1,'B',100,500000)})); auto p3=efe::MoldUdp64::parse_downstream(mold("TEST",3,{cancel(1,10)})); auto p2=efe::MoldUdp64::parse_downstream(mold("TEST",2,{add(2,'B',50,499000)}));
    auto r1=recovery.ingest(p1); CHECK(r1.ready.size()==1&&recovery.expected()==2); auto r3=recovery.ingest(p3); CHECK(r3.ready.empty()&&r3.request&&r3.request->first==2&&r3.request->count==1); auto r2=recovery.ingest(p2); CHECK(r2.ready.size()==2&&r2.ready[0].sequence==2&&r2.ready[1].sequence==3&&recovery.expected()==4);
    const auto req=efe::MoldUdp64::encode_request({efe::MoldUdp64::session_from_string("TEST"),4,3}); CHECK(efe::read_u64_be(req,10)==4); CHECK(efe::read_u16_be(req,18)==3);
}

void test_capture_replay(){
    const auto path=std::filesystem::temp_directory_path()/"efe_full_test.efc"; const auto d1=mold("CAP",1,{add(1,'B',10,100000)}); const auto d2=mold("CAP",2,{cancel(1,2)});
    {efe::CaptureWriter w(path.string());w.write(0,d1);w.write(100,d2);} std::size_t seen=0; efe::ReplayEngine::run(path.string(),{efe::ReplayMode::MaxSpeed,1.0},[&](const efe::CaptureRecord&r){CHECK(!r.datagram.empty());++seen;}); CHECK(seen==2); std::error_code ec;std::filesystem::remove(path,ec);
}
}

int main(){test_generated_schema();test_differential_decoder();test_arena_book();test_mold_recovery();test_capture_replay();if(failures){std::cerr<<failures<<" test(s) failed\n";return EXIT_FAILURE;}std::cout<<"all tests passed\n";return EXIT_SUCCESS;}
