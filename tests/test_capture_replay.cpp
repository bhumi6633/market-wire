#include "efe/capture.hpp"
#include "efe/itch_decoder.hpp"
#include "efe/moldudp64.hpp"
#include "efe/order_book.hpp"
#include "efe/recovery_engine.hpp"
#include "efe/replay.hpp"
#include "test_support.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
std::filesystem::path path(std::string_view name){return std::filesystem::temp_directory_path()/std::string(name);}
std::uint64_t replay_hash(const std::filesystem::path& file,const efe::ReplayOptions& options){
    std::uint64_t hash=1469598103934665603ULL;
    const auto count=efe::ReplayEngine::run(file.string(),options,[&](const efe::CaptureRecord&r){for(auto byte:r.datagram){hash^=byte;hash*=1099511628211ULL;}});
    CHECK(count==3);return hash;
}
std::uint64_t engine_hash(const std::filesystem::path& file){
    efe::RecoveryEngine recovery;efe::ItchDecoder decoder;efe::OrderBook book(16);
    efe::ReplayEngine::run(file.string(),{},[&](const efe::CaptureRecord&record){const auto packet=efe::MoldUdp64::parse_downstream(record.datagram);const auto result=recovery.ingest(packet);for(const auto&message:result.ready)if(const auto event=decoder.decode_book_event(message.payload))CHECK(book.apply(*event));});
    return book.state_hash();
}
}
int main(){
    const auto good=path("efe_capture_replay_test.efc");
    {efe::CaptureWriter writer(good.string());writer.write(0,test::Bytes{1,2});writer.write(1,test::Bytes{3});writer.write(2,test::Bytes{4,5});}
    const auto first=replay_hash(good,{efe::ReplayMode::MaxSpeed,1.0});
    CHECK(first==replay_hash(good,{efe::ReplayMode::MaxSpeed,1.0}));CHECK(first==replay_hash(good,{efe::ReplayMode::Scaled,1'000'000.0}));
    CHECK(first==replay_hash(good,{efe::ReplayMode::Realtime,1.0}));
    std::istringstream input("\n\n\n");std::ostringstream output;efe::ReplayOptions step{efe::ReplayMode::Step,1.0,&input,&output};
    CHECK(first==replay_hash(good,step));CHECK(output.str().find("press ENTER")!=std::string::npos);
    test::throws<std::invalid_argument>([&]{(void)replay_hash(good,{efe::ReplayMode::Scaled,0.0});});

    const auto pipeline=path("efe_capture_pipeline.efc");auto add=test::base('A',36);test::put_u64(add,11,1);add[19]='B';test::put_u32(add,20,100);test::put_ascii(add,24,8,"AAPL");test::put_u32(add,32,1000);
    auto cancel=test::base('X',23);test::put_u64(cancel,11,1);test::put_u32(cancel,19,25);
    {efe::CaptureWriter writer(pipeline.string());writer.write(0,test::mold(2,{cancel}));writer.write(1,test::mold(1,{add}));}
    const auto hash=engine_hash(pipeline);CHECK(hash==engine_hash(pipeline));CHECK(hash==engine_hash(pipeline));

    const auto short_file=path("efe_capture_short.efc");{std::ofstream out(short_file,std::ios::binary);out.write("EFC2",4);out.put(0);}
    test::throws<std::runtime_error>([&]{efe::CaptureReader reader(short_file.string());(void)reader.next();});
    const auto huge=path("efe_capture_huge.efc");{std::ofstream out(huge,std::ios::binary);out.write("EFC2",4);std::array<char,8> timestamp{};out.write(timestamp.data(),8);std::array<char,4> length{0,1,0,0};out.write(length.data(),4);}
    test::throws<std::runtime_error>([&]{efe::CaptureReader reader(huge.string());(void)reader.next();});
    const auto backwards=path("efe_capture_backwards.efc");{efe::CaptureWriter writer(backwards.string());writer.write(2,test::Bytes{1});writer.write(1,test::Bytes{2});}
    test::throws<std::runtime_error>([&]{(void)efe::ReplayEngine::run(backwards.string(),{efe::ReplayMode::Realtime,1.0},[](const auto&){});});
    test::throws<std::runtime_error>([&]{efe::CaptureWriter writer(path("efe_capture_large.efc").string());test::Bytes bytes(efe::kMaxCaptureDatagramSize+1);writer.write(0,bytes);});
    std::error_code ec;std::filesystem::remove(good,ec);std::filesystem::remove(pipeline,ec);std::filesystem::remove(short_file,ec);std::filesystem::remove(huge,ec);std::filesystem::remove(backwards,ec);
    return test::finish();
}
