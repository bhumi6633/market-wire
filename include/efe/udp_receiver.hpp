#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace efe {

class UdpMulticastReceiver {
public:
    UdpMulticastReceiver(const std::string& group, std::uint16_t port, const std::string& interface_ip = "0.0.0.0");
    ~UdpMulticastReceiver();
    UdpMulticastReceiver(const UdpMulticastReceiver&) = delete;
    UdpMulticastReceiver& operator=(const UdpMulticastReceiver&) = delete;

    [[nodiscard]] std::vector<std::uint8_t> receive(std::size_t max_datagram = 65536);
    void send_unicast(const std::string& host, std::uint16_t port, std::span<const std::uint8_t> bytes);

private:
    int fd_{-1};
};

}  // namespace efe
