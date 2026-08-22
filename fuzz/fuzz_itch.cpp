#include "efe/itch_decoder.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size){try{efe::ItchDecoder d;(void)d.decode_book_event(std::span<const std::uint8_t>(data,size));}catch(...){}return 0;}
