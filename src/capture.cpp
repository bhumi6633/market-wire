#include "efe/capture.hpp"

#include <array>
#include <stdexcept>

namespace efe {
namespace {
void write_u32(std::ostream& out, std::uint32_t v) {
    std::array<char,4> b{static_cast<char>(v>>24U), static_cast<char>(v>>16U), static_cast<char>(v>>8U), static_cast<char>(v)};
    out.write(b.data(), 4);
}
void write_u64(std::ostream& out, std::uint64_t v) {
    std::array<char,8> b{};
    for (int i=7;i>=0;--i) b[static_cast<std::size_t>(7-i)] = static_cast<char>((v>>(i*8))&0xFFU);
    out.write(b.data(),8);
}
std::uint32_t read_u32(std::istream& in) {
    std::array<unsigned char,4> b{}; in.read(reinterpret_cast<char*>(b.data()),4);
    if(!in) throw std::runtime_error("truncated capture length");
    return (static_cast<std::uint32_t>(b[0])<<24U)|(static_cast<std::uint32_t>(b[1])<<16U)|(static_cast<std::uint32_t>(b[2])<<8U)|b[3];
}
std::uint64_t read_u64(std::istream& in) {
    std::array<unsigned char,8> b{}; in.read(reinterpret_cast<char*>(b.data()),8);
    if(!in) throw std::runtime_error("truncated capture timestamp");
    std::uint64_t v=0; for(auto x:b) v=(v<<8U)|x; return v;
}
}

CaptureWriter::CaptureWriter(const std::string& path) : out_(path, std::ios::binary) {
    if(!out_) throw std::runtime_error("failed to open capture for writing: "+path);
    out_.write("EFC2",4);
}
void CaptureWriter::write(std::uint64_t ns, std::span<const std::uint8_t> d) {
    if(d.size()>0xFFFF'FFFFULL) throw std::runtime_error("capture datagram too large");
    write_u64(out_,ns); write_u32(out_,static_cast<std::uint32_t>(d.size()));
    out_.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    if(!out_) throw std::runtime_error("capture write failed");
}
CaptureReader::CaptureReader(const std::string& path) : in_(path,std::ios::binary) {
    if(!in_) throw std::runtime_error("failed to open capture: "+path);
    std::array<char,4> magic{}; in_.read(magic.data(),4);
    if(!in_ || std::string_view(magic.data(),4)!="EFC2") throw std::runtime_error("not an EFC2 capture");
}
std::optional<CaptureRecord> CaptureReader::next() {
    const int c=in_.peek(); if(c==std::char_traits<char>::eof()) return std::nullopt;
    CaptureRecord r; r.monotonic_ns=read_u64(in_); const auto len=read_u32(in_); r.datagram.resize(len);
    in_.read(reinterpret_cast<char*>(r.datagram.data()), static_cast<std::streamsize>(len));
    if(!in_) throw std::runtime_error("truncated capture payload");
    return r;
}
}  // namespace efe
