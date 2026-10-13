#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI bind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
std::int64_t APS5_VABI sendto_nid_postfix(int, const void*, std::uint64_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI recvfrom_nid_postfix(int, void*, std::uint64_t, int, void*, std::uint32_t*);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}
struct Iovec {
    void* base;
    std::uint64_t length;
};
struct Msghdr {
    void* name;
    std::uint32_t nameLength;
    Iovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};
extern "C" std::int64_t APS5_VABI sendmsg_nid_postfix(int, const Msghdr*, int);

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
        std::exit(1);
    }
}

static void Check(int family) {
    const std::uint32_t size = family == 2 ? 16 : 28;
    const int receiver = socket_nid_postfix(family, 2, 0);
    if (receiver < 0 && family == 28 && *__error_nid_postfix() == 47) return;
    Require(receiver >= 0, "receiver socket");
    const int sender = socket_nid_postfix(family, 2, 0);
    Require(sender >= 0, "sender socket");
    std::array<unsigned char, 28> address{};
    address[0] = static_cast<unsigned char>(size);
    address[1] = static_cast<unsigned char>(family);
    if (family == 2) { address[4] = 127; address[7] = 1; }
    else address[23] = 1;
    const int bound = bind_nid_postfix(receiver, address.data(), size);
    if (bound < 0 && family == 28 && *__error_nid_postfix() == 49) {
        Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0, "close");
        return;
    }
    Require(bound == 0, "bind receiver");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &length) == 0 && length == size, "receiver name");
    char byte = 0;
    Require(sendto_nid_postfix(sender, "a", 1, 0, address.data(), size) == 1, "sendto on an unconnected socket");
    Require(recvfrom_nid_postfix(receiver, &byte, 1, 0, nullptr, nullptr) == 1 && byte == 'a', "first datagram");
    Require(connect_nid_postfix(sender, address.data(), size) == 0, "connect");
    Require(sendto_nid_postfix(sender, "b", 1, 0, address.data(), size) == -1 && *__error_nid_postfix() == 56,
        "sendto with an address on a connected socket must fail with EISCONN (56)");
    Iovec vector{const_cast<char*>("c"), 1};
    Msghdr message{address.data(), size, &vector, 1, nullptr, 0, 0};
    Require(sendmsg_nid_postfix(sender, &message, 0) == -1 && *__error_nid_postfix() == 56,
        "sendmsg with a name on a connected socket must fail with EISCONN (56)");
    Require(sendto_nid_postfix(sender, "d", 1, 0, nullptr, 0) == 1, "sendto without an address on a connected socket");
    Require(recvfrom_nid_postfix(receiver, &byte, 1, 0, nullptr, nullptr) == 1 && byte == 'd', "connected datagram");
    std::array<unsigned char, 28> unspecified{};
    unspecified[0] = static_cast<unsigned char>(size);
    Require(connect_nid_postfix(sender, unspecified.data(), size) == -1 && *__error_nid_postfix() == 47, "disconnect");
    length = address.size();
    Require(getpeername_nid_postfix(sender, unspecified.data(), &length) == -1 && *__error_nid_postfix() == 57, "peer after disconnect");
    Require(sendto_nid_postfix(sender, "e", 1, 0, address.data(), size) == 1, "sendto after disconnect");
    Require(recvfrom_nid_postfix(receiver, &byte, 1, 0, nullptr, nullptr) == 1 && byte == 'e', "datagram after disconnect");
    Require(close_nid_postfix(sender) == 0 && close_nid_postfix(receiver) == 0, "close");
}

int main() {
    Check(2);
    Check(28);
    return 0;
}
