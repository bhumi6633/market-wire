#include "efe/itch_decoder.hpp"
#include "efe/byte_reader.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdlib>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,std::size_t size){try{efe::ItchDecoder generated;efe::ReferenceItchDecoder reference;const auto bytes=std::span<const std::uint8_t>(data,size);const auto event=generated.decode_book_event(bytes);if(event){const auto comparison=reference.decode_book_event(bytes);if(!comparison||comparison->index()!=event->index())std::abort();}}catch(const efe::DecodeError&){}return 0;}
