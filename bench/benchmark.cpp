#include "efe/itch_decoder.hpp"
#include "efe/metrics.hpp"
#include "efe/order_book.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
using Bytes=std::vector<std::uint8_t>;
void p16(Bytes&b,std::size_t o,std::uint16_t v){b[o]=v>>8U;b[o+1]=v;} void p32(Bytes&b,std::size_t o,std::uint32_t v){b[o]=v>>24U;b[o+1]=v>>16U;b[o+2]=v>>8U;b[o+3]=v;} void p64(Bytes&b,std::size_t o,std::uint64_t v){for(int i=7;i>=0;--i)b[o+static_cast<std::size_t>(7-i)]=v>>(i*8);} void p48(Bytes&b,std::size_t o,std::uint64_t v){for(int i=5;i>=0;--i)b[o+static_cast<std::size_t>(5-i)]=v>>(i*8);} 
Bytes add(std::uint64_t id){Bytes b(36);b[0]='A';p16(b,1,1);p16(b,3,1);p48(b,5,id);p64(b,11,id);b[19]='B';p32(b,20,100);const char*s="AAPL    ";std::memcpy(b.data()+24,s,8);p32(b,32,2'000'000+static_cast<std::uint32_t>(id%100));return b;}

template<class Decoder> void run_decoder(std::string_view name,const std::vector<Bytes>&msgs,Decoder&d){efe::LatencySamples samples(msgs.size());const auto start=std::chrono::steady_clock::now();std::uint64_t sink=0;for(const auto&m:msgs){auto a=std::chrono::steady_clock::now();auto e=d.decode_book_event(m);auto z=std::chrono::steady_clock::now();samples.add(std::chrono::duration_cast<std::chrono::nanoseconds>(z-a).count());if(e)sink+=std::get<efe::AddEvent>(*e).order.id;}const auto sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto s=samples.summarize();std::cout<<name<<": "<<std::fixed<<std::setprecision(2)<<(msgs.size()/sec/1e6)<<" M msg/s p50="<<s.p50_ns<<"ns p99="<<s.p99_ns<<"ns p99.9="<<s.p999_ns<<"ns sink="<<sink<<"\n";}
}
int main(int argc,char**argv){const std::size_t n=argc>1?std::stoull(argv[1]):200000;std::vector<Bytes>msgs;msgs.reserve(n);for(std::size_t i=1;i<=n;++i)msgs.push_back(add(i));efe::ItchDecoder g;efe::ReferenceItchDecoder r;run_decoder("generated",msgs,g);run_decoder("handwritten-reference",msgs,r);return 0;}
