#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int);
std::int64_t APS5_VABI write_nid_postfix(int, const char*, std::int64_t);
int* APS5_VABI __error_nid_postfix();
}

#include <cstdio>
static void Check(bool value, int line) { if (!value) { std::fprintf(stderr, "line %d errno %d\n", line, *__error_nid_postfix()); std::abort(); } }
#define Require(v) Check((v), __LINE__)

constexpr int CreateReadWrite = 0x202;
constexpr int FromStart = 0;
constexpr int FromCurrent = 1;
constexpr int FromEnd = 2;
constexpr int GuestEoverflow = 84;
constexpr int GuestEinval = 22;
constexpr std::int64_t Largest = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t SceEoverflow = static_cast<int>(0x80020054u);
constexpr std::int64_t SceEinval = static_cast<int>(0x80020016u);

static void RequireFailure(std::int64_t result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

int main() {
    const int file = sceKernelOpen("lseek_overflow.bin", CreateReadWrite, 0644);
    Require(file >= 0);
    Require(write_nid_postfix(file, "12345", 5) == 5);
    Require(lseek_nid_postfix(file, 2, FromStart) == 2);
    RequireFailure(lseek_nid_postfix(file, Largest, FromCurrent), GuestEoverflow);
    Require(sceKernelLseek(file, Largest, FromCurrent) == SceEoverflow);
    RequireFailure(lseek_nid_postfix(file, Largest, FromEnd), GuestEoverflow);
    Require(sceKernelLseek(file, Largest, FromEnd) == SceEoverflow);
    Require(lseek_nid_postfix(file, 0, FromCurrent) == 2);
    Require(lseek_nid_postfix(file, Largest - 2, FromCurrent) == Largest);
    RequireFailure(lseek_nid_postfix(file, 1, FromCurrent), GuestEoverflow);
    Require(lseek_nid_postfix(file, Largest - 5, FromEnd) == Largest);
    RequireFailure(lseek_nid_postfix(file, -6, FromEnd), GuestEinval);
    Require(sceKernelLseek(file, -6, FromEnd) == SceEinval);
    Require(sceKernelClose(file) == 0);
}
