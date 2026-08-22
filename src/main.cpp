#include "efe/byte_reader.hpp"
#include "efe/capture.hpp"
#include "efe/itch_decoder.hpp"
#include "efe/metrics.hpp"
#include "efe/moldudp64.hpp"
#include "efe/order_book.hpp"
#include "efe/recovery_engine.hpp"
#include "efe/replay.hpp"
#include "efe/udp_receiver.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void put_u32(Bytes& b, std::size_t off, std::uint32_t v) {
    b[off]=static_cast<std::uint8_t>(v>>24U); b[off+1]=static_cast<std::uint8_t>(v>>16U);
    b[off+2]=static_cast<std::uint8_t>(v>>8U); b[off+3]=static_cast<std::uint8_t>(v);
}
void put_u64(Bytes& b, std::size_t off, std::uint64_t v) { for(int i=7;i>=0;--i) b[off+static_cast<std::size_t>(7-i)]=static_cast<std::uint8_t>(v>>(i*8)); }
void put_u16(Bytes& b, std::size_t off, std::uint16_t v) { b[off]=static_cast<std::uint8_t>(v>>8U); b[off+1]=static_cast<std::uint8_t>(v); }
void put_u48(Bytes& b, std::size_t off, std::uint64_t v) { for(int i=5;i>=0;--i) b[off+static_cast<std::size_t>(5-i)]=static_cast<std::uint8_t>(v>>(i*8)); }
void put_ascii(Bytes& b, std::size_t off, std::size_t n, std::string_view s) { for(std::size_t i=0;i<n;++i) b[off+i]=static_cast<std::uint8_t>(i<s.size()?s[i]:' '); }

Bytes add_msg(std::uint64_t id, char side, std::uint32_t qty, std::string_view symbol, std::uint32_t price) {
    Bytes b(36); b[0]='A'; put_u16(b,1,1); put_u16(b,3,1); put_u48(b,5,1'000'000); put_u64(b,11,id);
    b[19]=static_cast<std::uint8_t>(side); put_u32(b,20,qty); put_ascii(b,24,8,symbol); put_u32(b,32,price); return b;
}
Bytes cancel_msg(std::uint64_t id, std::uint32_t qty) {
    Bytes b(23); b[0]='X'; put_u16(b,1,1); put_u16(b,3,2); put_u48(b,5,1'000'100); put_u64(b,11,id); put_u32(b,19,qty); return b;
}

Bytes mold_packet(std::string_view session, efe::Sequence first, const std::vector<Bytes>& messages) {
    std::size_t size=20; for(const auto& m:messages) size += 2+m.size(); Bytes d(size);
    for(std::size_t i=0;i<10;++i) d[i]=static_cast<std::uint8_t>(i<session.size()?session[i]:' ');
    put_u64(d,10,first); put_u16(d,18,static_cast<std::uint16_t>(messages.size())); std::size_t off=20;
    for(const auto& m:messages){ put_u16(d,off,static_cast<std::uint16_t>(m.size())); off+=2; std::memcpy(d.data()+off,m.data(),m.size()); off+=m.size(); }
    return d;
}

void process_packet(const Bytes& datagram, efe::RecoveryEngine& recovery, efe::ItchDecoder& decoder, efe::OrderBook& book) {
    const auto packet=efe::MoldUdp64::parse_downstream(datagram); const auto r=recovery.ingest(packet);
    if(r.request) std::cout << "GAP: request seq " << r.request->first << " count " << r.request->count << '\n';
    for(const auto& m:r.ready) {
        const auto event=decoder.decode_book_event(m.payload);
        std::cout << "seq " << m.sequence << " " << decoder.message_name(m.payload);
        if(event) std::cout << (book.apply(*event)?" applied":" rejected");
        std::cout << '\n';
    }
}

int demo() {
    std::cout << "Exchange Feed Engine demo: gap recovery + generated ITCH decoding + arena book\n\n";
    efe::RecoveryEngine recovery(1); efe::ItchDecoder decoder; efe::OrderBook book(1024);
    const auto p1=mold_packet("DEMO",1,{add_msg(10,'B',100,"AAPL",2'000'000)});
    const auto p3=mold_packet("DEMO",3,{cancel_msg(10,25)});
    const auto p2=mold_packet("DEMO",2,{add_msg(11,'B',200,"AAPL",1'999'000)});
    process_packet(p1,recovery,decoder,book); process_packet(p3,recovery,decoder,book); process_packet(p2,recovery,decoder,book);
    const auto sym=efe::Symbol::from_string("AAPL");
    std::cout << "\nfinal live orders: " << book.live_order_count() << '\n';
    if(auto bid=book.best_bid(sym)) std::cout << "best bid: $" << std::fixed << std::setprecision(4) << efe::to_decimal_price(*bid) << '\n';
    std::cout << "state hash: 0x" << std::hex << book.state_hash() << std::dec << '\n';
    return 0;
}

int replay(const std::string& path, const efe::ReplayOptions& options) {
    efe::RecoveryEngine recovery(1); efe::ItchDecoder decoder; efe::OrderBook book(1'000'000);
    const auto start=std::chrono::steady_clock::now(); std::size_t messages=0;
    const auto packets=efe::ReplayEngine::run(path,options,[&](const efe::CaptureRecord& rec){
        const auto packet=efe::MoldUdp64::parse_downstream(rec.datagram); const auto result=recovery.ingest(packet);
        for(const auto& m:result.ready){ if(auto e=decoder.decode_book_event(m.payload)) book.apply(*e); ++messages; }
    });
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout << "replayed " << packets << " packets / " << messages << " messages in " << elapsed << " s\n";
    std::cout << "live orders=" << book.live_order_count() << " state_hash=0x" << std::hex << book.state_hash() << std::dec << '\n';
    return 0;
}

efe::ReplayOptions replay_options(int argc,char**argv){
    efe::ReplayOptions options;
    if(argc<4)return options;
    const std::string_view argument=argv[3];
    if(argument=="--step"||argument=="step")options.mode=efe::ReplayMode::Step;
    else if(argument=="--realtime"||argument=="realtime")options.mode=efe::ReplayMode::Realtime;
    else{
        std::string_view value=argument;
        if(argument=="--speed"){
            if(argc<5)throw std::invalid_argument("--speed requires max or a positive multiplier");
            value=argv[4];
        }
        if(value=="max"||value=="0")return options;
        options.speed=std::stod(std::string(value));
        if(options.speed<=0.0)throw std::invalid_argument("replay speed must be positive");
        options.mode=efe::ReplayMode::Scaled;
    }
    return options;
}

void help(){
    std::cout<<"Exchange Feed Engine\n"
             <<"  feed_engine demo\n"
             <<"  feed_engine make-sample-capture <file>\n"
             <<"  feed_engine replay <capture> [--speed max|N | --realtime | --step]\n"
             <<"  feed_engine listen <group> <port> <rerequest-host> <rerequest-port> [capture.efc]\n";
}

int make_sample_capture(const std::string& path) {
    efe::CaptureWriter writer(path);
    const auto p1=mold_packet("DEMO",1,{add_msg(10,'B',100,"AAPL",2'000'000)});
    const auto p3=mold_packet("DEMO",3,{cancel_msg(10,25)});
    const auto p2=mold_packet("DEMO",2,{add_msg(11,'B',200,"AAPL",1'999'000)});
    writer.write(0,p1);
    writer.write(1'000'000,p3);
    writer.write(2'000'000,p2);
    std::cout << "wrote sample capture: " << path << '\n';
    return 0;
}

int listen(int argc, char** argv) {
    if(argc<6){ std::cerr << "usage: feed_engine listen <group> <port> <rerequest-host> <rerequest-port> [capture.efc]\n"; return 2; }
    const std::string group=argv[2], host=argv[4]; const auto port=static_cast<std::uint16_t>(std::stoi(argv[3])); const auto rport=static_cast<std::uint16_t>(std::stoi(argv[5]));
    efe::UdpMulticastReceiver socket(group,port); efe::RecoveryEngine recovery(1); efe::ItchDecoder decoder; efe::OrderBook book(2'000'000);
    std::optional<efe::CaptureWriter> capture; if(argc>=7) capture.emplace(argv[6]);
    const auto epoch=std::chrono::steady_clock::now();
    while(true){
        const auto d=socket.receive();
        if (d.empty() && socket.stop_requested()) break;
        const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-epoch).count(); if(capture) capture->write(static_cast<std::uint64_t>(now),d);
        const auto packet=efe::MoldUdp64::parse_downstream(d); const auto r=recovery.ingest(packet);
        if(r.request && recovery.session()) { const auto req=efe::MoldUdp64::encode_request({*recovery.session(),r.request->first,r.request->count}); socket.send_unicast(host,rport,req); }
        for(const auto& m:r.ready) if(auto e=decoder.decode_book_event(m.payload)) if(!book.apply(*e)) std::cerr << "book rejected seq " << m.sequence << '\n';
        if(r.end_of_session){ std::cout << "end of session; state_hash=0x" << std::hex << book.state_hash() << std::dec << '\n'; break; }
    }
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        if(argc==1 || std::string_view(argv[1])=="demo") return demo();
        if(std::string_view(argv[1])=="--help"||std::string_view(argv[1])=="help"){help();return 0;}
        if(std::string_view(argv[1])=="replay") { if(argc<3){std::cerr<<"usage: feed_engine replay <capture.efc> [--speed max|N | --realtime | --step]\n";return 2;} return replay(argv[2],replay_options(argc,argv)); }
        if(std::string_view(argv[1])=="make-sample-capture") { if(argc<3){std::cerr<<"usage: feed_engine make-sample-capture <capture.efc>\n";return 2;} return make_sample_capture(argv[2]); }
        if(std::string_view(argv[1])=="listen") return listen(argc,argv);
        std::cerr << "unknown command; run feed_engine --help\n"; return 2;
    } catch(const std::exception& e) { std::cerr << "fatal: " << e.what() << '\n'; return 1; }
}
