#include "efe/moldudp64.hpp"
#include "efe/recovery_engine.hpp"
#include "test_support.hpp"

#include <limits>

int main() {
    using namespace efe;
    const test::Bytes one{1}, two{2}, three{3}, four{4}, five{5};
    const auto multi_bytes=test::mold(10,{one,two,three});
    const auto multi=MoldUdp64::parse_downstream(multi_bytes);
    CHECK(multi.session_string()=="TEST");CHECK(multi.first_sequence==10);CHECK(multi.messages.size()==3);
    CHECK(multi.messages[2].sequence==12 && multi.messages[2].payload==three);
    for(std::size_t n=0;n<multi_bytes.size();++n)test::throws<DecodeError>([&]{(void)MoldUdp64::parse_downstream(std::span(multi_bytes.data(),n));});
    auto trailing=multi_bytes;trailing.push_back(0);test::throws<DecodeError>([&]{(void)MoldUdp64::parse_downstream(trailing);});
    auto bad_length=multi_bytes;test::put_u16(bad_length,20,0xFFFFU);test::throws<DecodeError>([&]{(void)MoldUdp64::parse_downstream(bad_length);});

    const auto heartbeat=MoldUdp64::parse_downstream(test::mold(13,{}));CHECK(heartbeat.heartbeat&&!heartbeat.end_of_session);
    const auto eos=MoldUdp64::parse_downstream(test::mold(13,{},0xFFFFU));CHECK(eos.end_of_session&&!eos.heartbeat);
    const auto request=MoldUdp64::encode_request({MoldUdp64::session_from_string("TEST"),77,4});
    CHECK(read_u64_be(request,10)==77);CHECK(read_u16_be(request,18)==4);
    test::throws<DecodeError>([&]{(void)MoldUdp64::encode_request({MoldUdp64::session_from_string("TEST"),1,0});});
    test::throws<DecodeError>([&]{(void)MoldUdp64::parse_downstream(test::mold(std::numeric_limits<Sequence>::max(),{one,two}));});

    RecoveryEngine ordered(1);auto r=ordered.ingest(MoldUdp64::parse_downstream(test::mold(1,{one,two})));
    CHECK(r.ready.size()==2&&r.ready[0].sequence==1&&r.ready[1].sequence==2&&ordered.expected()==3);
    RecoveryEngine recovery(1);
    auto future=recovery.ingest(MoldUdp64::parse_downstream(test::mold(4,{four,five})));
    CHECK(future.ready.empty()&&future.request&&future.request->first==1&&future.request->count==3);
    CHECK(recovery.buffered_messages()==2);
    auto partial=recovery.ingest(MoldUdp64::parse_downstream(test::mold(1,{one,two})));
    CHECK(partial.ready.size()==2&&recovery.expected()==3&&partial.request&&partial.request->first==3);
    auto complete=recovery.ingest(MoldUdp64::parse_downstream(test::mold(3,{three})));
    CHECK(complete.ready.size()==3&&complete.ready[0].sequence==3&&complete.ready[2].sequence==5&&recovery.expected()==6);
    auto stale=recovery.ingest(MoldUdp64::parse_downstream(test::mold(1,{one,two})));
    CHECK(stale.ready.empty()&&stale.duplicate_or_stale);
    auto second_gap=recovery.ingest(MoldUdp64::parse_downstream(test::mold(8,{one})));
    CHECK(second_gap.request&&second_gap.request->first==6&&second_gap.request->count==2);
    for(const auto& ready:complete.ready)CHECK(ready.sequence>=3&&ready.sequence<=5);
    auto other=test::mold(6,{one});test::put_ascii(other,0,10,"OTHER");
    test::throws<DecodeError>([&]{(void)recovery.ingest(MoldUdp64::parse_downstream(other));});
    RecoveryEngine exhausted(std::numeric_limits<Sequence>::max());
    test::throws<DecodeError>([&]{(void)exhausted.ingest(MoldUdp64::parse_downstream(test::mold(std::numeric_limits<Sequence>::max(),{one})));});
    return test::finish();
}
