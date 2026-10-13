#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#ifndef _WIN32
#include <csignal>
#endif

struct GuestIovec {
    const void* base;
    std::size_t length;
};

extern "C" {
int APS5_VABI pipe_nid_postfix(int*);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
std::int64_t APS5_VABI writev_nid_postfix(int, const GuestIovec*, int);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
std::int64_t APS5_VABI sceKernelWritev(int, const GuestIovec*, int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEpipe = 32;
constexpr int SetFlags = 4;
constexpr int NonBlocking = 4;
constexpr std::int64_t SceEpipe = static_cast<int>(0x80020020u);

static void RequireBrokenWrites(int writer) {
    const GuestIovec vector{"x", 1};
    *__error_nid_postfix() = 0;
    Require(write_nid_postfix(writer, "x", 1) == -1);
    Require(*__error_nid_postfix() == GuestEpipe);
    *__error_nid_postfix() = 0;
    Require(writev_nid_postfix(writer, &vector, 1) == -1);
    Require(*__error_nid_postfix() == GuestEpipe);
    Require(sceKernelWrite(writer, "x", 1) == SceEpipe);
    Require(sceKernelWritev(writer, &vector, 1) == SceEpipe);
}

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    int blocking[2];
    Require(pipe_nid_postfix(blocking) == 0);
    Require(close_nid_postfix(blocking[0]) == 0);
    RequireBrokenWrites(blocking[1]);
    Require(close_nid_postfix(blocking[1]) == 0);

    int nonblocking[2];
    Require(pipe_nid_postfix(nonblocking) == 0);
    Require(fcntl_nid_postfix(nonblocking[1], SetFlags, NonBlocking) == 0);
    Require(write_nid_postfix(nonblocking[1], "x", 1) == 1);
    Require(close_nid_postfix(nonblocking[0]) == 0);
    RequireBrokenWrites(nonblocking[1]);
    Require(close_nid_postfix(nonblocking[1]) == 0);
}
