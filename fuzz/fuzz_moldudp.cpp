#include "efe/moldudp64.hpp"
#include "efe/byte_reader.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdlib>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size){try{const auto packet=efe::MoldUdp64::parse_downstream(std::span<const std::uint8_t>(data,size));if(!packet.heartbeat&&!packet.end_of_session&&packet.messages.size()!=packet.message_count)std::abort();for(std::size_t i=0;i<packet.messages.size();++i)if(packet.messages[i].sequence!=packet.first_sequence+i)std::abort();}catch(const efe::DecodeError&){}return 0;}
