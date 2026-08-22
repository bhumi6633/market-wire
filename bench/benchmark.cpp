#include "efe/itch_decoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <sys/utsname.h>
#include <vector>
namespace {
using Bytes=std::vector<std::uint8_t>;
void p32(Bytes&b,std::size_t o,std::uint32_t v){for(int i=3;i>=0;--i)b[o+static_cast<std::size_t>(3-i)]=static_cast<std::uint8_t>(v>>(i*8));}
void p64(Bytes&b,std::size_t o,std::uint64_t v){for(int i=7;i>=0;--i)b[o+static_cast<std::size_t>(7-i)]=static_cast<std::uint8_t>(v>>(i*8));}
Bytes base(char type,std::size_t size,std::uint64_t id){Bytes b(size);b[0]=static_cast<std::uint8_t>(type);p64(b,11,id);return b;}
std::array<Bytes,5> templates(){auto add=base('A',36,1);add[19]='B';p32(add,20,100);std::memcpy(add.data()+24,"AAPL    ",8);p32(add,32,2'000'000);auto execute=base('E',31,1);p32(execute,19,25);p64(execute,23,2);auto cancel=base('X',23,1);p32(cancel,19,25);auto del=base('D',19,1);auto replace=base('U',35,1);p64(replace,19,2);p32(replace,27,100);p32(replace,31,2'000'100);return {std::move(add),std::move(execute),std::move(cancel),std::move(del),std::move(replace)};}
template<class Decoder>std::uint64_t decode_all(const std::vector<Bytes>&messages,Decoder&decoder){std::uint64_t sink=0;for(const auto&message:messages){const auto event=decoder.decode_book_event(message);if(event)sink+=event->index()+message.size();}std::atomic_signal_fence(std::memory_order_seq_cst);return sink;}
struct Result{double throughput_mps{};double batch_median_us{};std::uint64_t sink{};};
template<class Decoder>Result measure(const std::vector<Bytes>&messages,Decoder&decoder){constexpr std::size_t batch_size=256;std::vector<double>batches;batches.reserve((messages.size()+batch_size-1)/batch_size);std::uint64_t sink=0;const auto total_start=std::chrono::steady_clock::now();for(std::size_t begin=0;begin<messages.size();begin+=batch_size){const auto end=std::min(begin+batch_size,messages.size());const auto start=std::chrono::steady_clock::now();for(std::size_t i=begin;i<end;++i){const auto event=decoder.decode_book_event(messages[i]);if(event)sink+=event->index()+messages[i].size();}const auto finish=std::chrono::steady_clock::now();batches.push_back(std::chrono::duration<double,std::micro>(finish-start).count());}const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-total_start).count();std::sort(batches.begin(),batches.end());std::atomic_signal_fence(std::memory_order_seq_cst);return {static_cast<double>(messages.size())/seconds/1e6,batches[batches.size()/2],sink};}
double median(std::vector<double>values){std::sort(values.begin(),values.end());return values[values.size()/2];}
}
int main(int argc,char**argv){const std::size_t n=argc>1?static_cast<std::size_t>(std::stoull(argv[1])):500'000;const auto patterns=templates();std::vector<Bytes>messages;messages.reserve(n);for(std::size_t i=0;i<n;++i){messages.push_back(patterns[i%patterns.size()]);p64(messages.back(),11,static_cast<std::uint64_t>(i+1));}efe::ItchDecoder generated;efe::ReferenceItchDecoder reference;std::cout<<"decoder benchmark: mixed A/E/X/D/U, "<<n<<" messages, 256-message timing batches, 7 trials\n";
#if defined(__clang__)
std::cout<<"compiler=Clang "<<__clang_version__;
#elif defined(__GNUC__)
std::cout<<"compiler=GCC "<<__VERSION__;
#else
std::cout<<"compiler=unknown";
#endif
#ifdef NDEBUG
std::cout<<" build=Release";
#else
std::cout<<" build=non-Release";
#endif
utsname machine{};if(uname(&machine)==0)std::cout<<" machine="<<machine.machine<<" os="<<machine.sysname<<' '<<machine.release;std::cout<<'\n';for(int warmup=0;warmup<2;++warmup){(void)decode_all(messages,generated);(void)decode_all(messages,reference);}std::vector<double>generated_rate,reference_rate,generated_batch,reference_batch;std::uint64_t sink=0;for(int trial=0;trial<7;++trial){if(trial%2==0){auto g=measure(messages,generated);auto r=measure(messages,reference);generated_rate.push_back(g.throughput_mps);reference_rate.push_back(r.throughput_mps);generated_batch.push_back(g.batch_median_us);reference_batch.push_back(r.batch_median_us);sink+=g.sink+r.sink;}else{auto r=measure(messages,reference);auto g=measure(messages,generated);generated_rate.push_back(g.throughput_mps);reference_rate.push_back(r.throughput_mps);generated_batch.push_back(g.batch_median_us);reference_batch.push_back(r.batch_median_us);sink+=g.sink+r.sink;}}std::cout<<std::fixed<<std::setprecision(2)<<"generated median throughput="<<median(generated_rate)<<" M msg/s; median 256-message batch="<<median(generated_batch)<<" us\n";std::cout<<"reference median throughput="<<median(reference_rate)<<" M msg/s; median 256-message batch="<<median(reference_batch)<<" us\n";std::cout<<"sink="<<sink<<"; no per-message nanosecond percentiles are reported\n";return 0;}
