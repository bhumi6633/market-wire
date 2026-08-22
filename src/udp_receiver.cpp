#include "efe/udp_receiver.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#error "UdpMulticastReceiver currently supports POSIX sockets only"
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace efe {
namespace {
[[noreturn]] void sys_fail(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}
}

UdpMulticastReceiver::UdpMulticastReceiver(const std::string& group, std::uint16_t port, const std::string& interface_ip) {
    const int candidate = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (candidate < 0) sys_fail("socket");
    try {
        int reuse = 1;
        if (::setsockopt(candidate, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) sys_fail("setsockopt SO_REUSEADDR");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(candidate, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) sys_fail("bind");

        ip_mreq mreq{};
        if (::inet_pton(AF_INET, group.c_str(), &mreq.imr_multiaddr) != 1) throw std::runtime_error("invalid multicast group");
        if (interface_ip == "0.0.0.0") mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        else if (::inet_pton(AF_INET, interface_ip.c_str(), &mreq.imr_interface) != 1) throw std::runtime_error("invalid interface IP");
        if (::setsockopt(candidate, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) sys_fail("IP_ADD_MEMBERSHIP");
        int wake_pipe[2]{};
        if (::pipe(wake_pipe) < 0) sys_fail("pipe");
        fd_ = candidate;
        wake_read_fd_ = wake_pipe[0];
        wake_write_fd_ = wake_pipe[1];
    } catch (...) {
        ::close(candidate);
        throw;
    }
}

UdpMulticastReceiver::~UdpMulticastReceiver() {
    if (fd_ >= 0) ::close(fd_);
    if (wake_read_fd_ >= 0) ::close(wake_read_fd_);
    if (wake_write_fd_ >= 0) ::close(wake_write_fd_);
}

std::vector<std::uint8_t> UdpMulticastReceiver::receive(std::size_t max_datagram) {
    if (stop_requested()) return {};
    std::vector<std::uint8_t> buffer(max_datagram);
    pollfd descriptors[2]{{fd_, POLLIN, 0}, {wake_read_fd_, POLLIN, 0}};
    const int ready = ::poll(descriptors, 2, -1);
    if (ready < 0) {
        if (errno == EINTR) return receive(max_datagram);
        sys_fail("poll");
    }
    if ((descriptors[1].revents & POLLIN) != 0 || stop_requested()) return {};
    const auto n = ::recv(fd_, buffer.data(), buffer.size(), 0);
    if (n < 0) {
        if (stop_requested()) return {};
        sys_fail("recv");
    }
    buffer.resize(static_cast<std::size_t>(n));
    return buffer;
}

void UdpMulticastReceiver::request_stop() noexcept {
    if (stop_requested_.exchange(true, std::memory_order_relaxed)) return;
    if (wake_write_fd_ >= 0) {
        const std::uint8_t byte = 1;
        (void)::write(wake_write_fd_, &byte, sizeof(byte));
    }
}

void UdpMulticastReceiver::send_unicast(const std::string& host, std::uint16_t port, std::span<const std::uint8_t> bytes) {
    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &dst.sin_addr) != 1) throw std::runtime_error("invalid rerequest host");
    const auto n = ::sendto(fd_, bytes.data(), bytes.size(), 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
    if (n < 0 || static_cast<std::size_t>(n) != bytes.size()) sys_fail("sendto");
}

}  // namespace efe
