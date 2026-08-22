#pragma once

#include "efe/byte_reader.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace test {
using Bytes = std::vector<std::uint8_t>;
inline int failures = 0;

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ':' << line << ": CHECK(" << expression << ") failed\n";
        ++failures;
    }
}

#define CHECK(expression) ::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

inline void put_u16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
    bytes[offset + 1] = static_cast<std::uint8_t>(value);
}
inline void put_u32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 3; i >= 0; --i) bytes[offset + static_cast<std::size_t>(3 - i)] = static_cast<std::uint8_t>(value >> (i * 8));
}
inline void put_u48(Bytes& bytes, std::size_t offset, std::uint64_t value) {
    for (int i = 5; i >= 0; --i) bytes[offset + static_cast<std::size_t>(5 - i)] = static_cast<std::uint8_t>(value >> (i * 8));
}
inline void put_u64(Bytes& bytes, std::size_t offset, std::uint64_t value) {
    for (int i = 7; i >= 0; --i) bytes[offset + static_cast<std::size_t>(7 - i)] = static_cast<std::uint8_t>(value >> (i * 8));
}
inline void put_ascii(Bytes& bytes, std::size_t offset, std::size_t width, std::string_view value) {
    for (std::size_t i = 0; i < width; ++i) {
        const char c = i < value.size() ? value[i] : ' ';
        bytes[offset + i] = static_cast<std::uint8_t>(static_cast<unsigned char>(c));
    }
}
inline Bytes base(char type, std::size_t size) {
    Bytes bytes(size);
    bytes[0] = static_cast<std::uint8_t>(static_cast<unsigned char>(type));
    put_u16(bytes, 1, 0x1234U);
    put_u16(bytes, 3, 0x5678U);
    put_u48(bytes, 5, 0x010203040506ULL);
    return bytes;
}
inline Bytes mold(std::uint64_t first, const std::vector<Bytes>& messages, std::uint16_t count_override = 0) {
    std::size_t size = 20;
    for (const auto& message : messages) size += 2 + message.size();
    Bytes bytes(size);
    put_ascii(bytes, 0, 10, "TEST");
    put_u64(bytes, 10, first);
    put_u16(bytes, 18, count_override == 0 ? static_cast<std::uint16_t>(messages.size()) : count_override);
    std::size_t offset = 20;
    for (const auto& message : messages) {
        put_u16(bytes, offset, static_cast<std::uint16_t>(message.size()));
        offset += 2;
        std::memcpy(bytes.data() + offset, message.data(), message.size());
        offset += message.size();
    }
    return bytes;
}
template <class Exception, class Function>
inline void throws(Function&& function) {
    try { function(); CHECK(false); } catch (const Exception&) {}
}
inline int finish() {
    if (failures == 0) std::cout << "all checks passed\n";
    return failures == 0 ? 0 : 1;
}
}  // namespace test
