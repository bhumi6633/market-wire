#include "efe/udp_receiver.hpp"
#include "test_support.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
int main(){
    try{efe::UdpMulticastReceiver invalid("not-an-address",0);CHECK(false);}catch(const std::runtime_error&){}
    std::unique_ptr<efe::UdpMulticastReceiver> receiver;
    try{receiver=std::make_unique<efe::UdpMulticastReceiver>("239.255.0.1",0);}catch(const std::runtime_error&e){std::cout<<"SKIP: local UDP unavailable: "<<e.what()<<'\n';return 77;}
    auto result=std::async(std::launch::async,[&]{return receiver->receive();});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));receiver->request_stop();
    CHECK(result.wait_for(std::chrono::seconds(1))==std::future_status::ready);CHECK(result.get().empty());CHECK(receiver->stop_requested());
    return test::finish();
}
