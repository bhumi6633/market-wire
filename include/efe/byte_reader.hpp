#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>

namespace efe {

class DecodeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void require_bytes(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width) {
    if (offset > bytes.size() || width > bytes.size() - offset) {
        throw DecodeError("truncated binary message");
    }
}

inline std::uint8_t read_u8(std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, 1);
    return bytes[offset];
}

inline std::uint16_t read_u16_be(std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, 2);
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1]));
}

inline std::uint32_t read_u32_be(std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, 4);
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

inline std::uint64_t read_u48_be(std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, 6);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        value = (value << 8U) | bytes[offset + i];
    }
    return value;
}

inline std::uint64_t read_u64_be(std::span<const std::uint8_t> bytes, std::size_t offset) {
    require_bytes(bytes, offset, 8);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8U) | bytes[offset + i];
    }
    return value;
}

inline std::string_view read_ascii_view(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width) {
    require_bytes(bytes, offset, width);
    return {reinterpret_cast<const char*>(bytes.data() + offset), width};
}

inline char read_char(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<char>(read_u8(bytes, offset));
}

inline void write_u16_be(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value) {
    if (offset > bytes.size() || 2 > bytes.size() - offset) {
        throw DecodeError("short output buffer");
    }
    bytes[offset] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>(value & 0xFFU);
}

inline void write_u64_be(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value) {
    if (offset > bytes.size() || 8 > bytes.size() - offset) {
        throw DecodeError("short output buffer");
    }
    for (int i = 7; i >= 0; --i) {
        bytes[offset + static_cast<std::size_t>(7 - i)] = static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU);
    }
}

}  // namespace efe
