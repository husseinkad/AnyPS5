#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI listen_nid_postfix(int, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI accept_nid_postfix(int, void*, std::uint32_t*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int Inet = 2;
constexpr int Inet6 = 28;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorRefused = 61;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

std::array<std::uint8_t, 28> Name(int descriptor, std::uint32_t expectedLength) {
    std::array<std::uint8_t, 28> name{};
    std::uint32_t length = name.size();
    Require(getsockname_nid_postfix(descriptor, name.data(), &length) == 0, "getsockname failed");
    Require(length == expectedLength, "getsockname length is not the address size");
    return name;
}

bool Port(const std::array<std::uint8_t, 28>& name) { return (name[2] | name[3]) != 0; }

void ListenWithoutBind() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket failed");
    Require(listen_nid_postfix(listener, 4) == 0, "listen on an unbound stream socket must bind the wildcard address and succeed");
    auto name = Name(listener, 16);
    Require(name[1] == Inet && Port(name), "the listener must have an ephemeral port");
    Require(name[4] == 0 && name[5] == 0 && name[6] == 0 && name[7] == 0, "the listener must hold the wildcard address");
    name[4] = 127;
    name[7] = 1;
    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0 && connect_nid_postfix(client, name.data(), 16) == 0, "connect to the implicit listener failed");
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");
    char received = 0;
    Require(send_nid_postfix(client, "x", 1, 0) == 1 && recv_nid_postfix(accepted, &received, 1, 0) == 1 && received == 'x', "exchange failed");
    Require(close_nid_postfix(accepted) == 0 && close_nid_postfix(client) == 0 && close_nid_postfix(listener) == 0, "close failed");
}

void ListenKeepsBoundAddress() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket failed");
    const std::array<std::uint8_t, 16> loopback{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, loopback.data(), loopback.size()) == 0, "bind failed");
    const auto before = Name(listener, 16);
    Require(listen_nid_postfix(listener, 4) == 0, "listen on a bound socket failed");
    const auto after = Name(listener, 16);
    Require(before == after && after[4] == 127 && after[7] == 1, "listen must keep the bound address");
    Require(close_nid_postfix(listener) == 0, "close failed");
}

void DatagramListenStaysUnbound() {
    const int datagram = socket_nid_postfix(Inet, Datagram, 0);
    Require(datagram >= 0, "socket failed");
    Require(listen_nid_postfix(datagram, 1) == -1 && *__error_nid_postfix() == ErrorNotSupported, "listen on a datagram socket must fail with EOPNOTSUPP (45)");
    Require(!Port(Name(datagram, 16)), "a refused listen must not bind the socket");
    Require(close_nid_postfix(datagram) == 0, "close failed");
}

void ListenWithoutBindIpv6() {
    const int probe = socket_nid_postfix(Inet6, Stream, 0);
    if (probe < 0) return;
    const std::array<std::uint8_t, 28> wildcard{28, Inet6};
    const bool available = bind_nid_postfix(probe, wildcard.data(), wildcard.size()) == 0;
    Require(close_nid_postfix(probe) == 0, "close failed");
    if (!available) return;
    const int listener = socket_nid_postfix(Inet6, Stream, 0);
    Require(listener >= 0, "socket failed");
    Require(listen_nid_postfix(listener, 4) == 0, "listen on an unbound IPv6 stream socket must succeed");
    const auto name = Name(listener, 28);
    Require(name[1] == Inet6 && Port(name), "the IPv6 listener must have an ephemeral port");
    Require(close_nid_postfix(listener) == 0, "close failed");
}

void ListenAfterRefusedConnect(int family) {
    const std::uint32_t size = family == Inet ? 16 : 28;
    const int probe = socket_nid_postfix(family, Stream, 0);
    if (probe < 0) return;
    std::array<std::uint8_t, 28> closed{};
    closed[0] = static_cast<std::uint8_t>(size);
    closed[1] = static_cast<std::uint8_t>(family);
    if (family == Inet) {
        closed[4] = 127;
        closed[7] = 1;
    } else {
        closed[23] = 1;
    }
    const bool available = bind_nid_postfix(probe, closed.data(), size) == 0;
    if (available) closed = Name(probe, size);
    Require(close_nid_postfix(probe) == 0, "close failed");
    if (!available) return;
    const int stream = socket_nid_postfix(family, Stream, 0);
    Require(stream >= 0, "socket failed");
    *__error_nid_postfix() = 0;
    Require(connect_nid_postfix(stream, closed.data(), size) == -1 && *__error_nid_postfix() == ErrorRefused, "connect to a closed loopback port must fail with ECONNREFUSED (61)");
    Require(listen_nid_postfix(stream, 4) == 0, "listen after a refused connect must succeed");
    Require(close_nid_postfix(stream) == 0, "close failed");
}

}

int main() {
    ListenWithoutBind();
    ListenKeepsBoundAddress();
    DatagramListenStaysUnbound();
    ListenWithoutBindIpv6();
    ListenAfterRefusedConnect(Inet);
    ListenAfterRefusedConnect(Inet6);
    return 0;
}
