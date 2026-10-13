#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <thread>
struct Timeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};
struct ResourceUsage {
    Timeval ru_utime;
    Timeval ru_stime;
    std::int64_t rest[14];
};
extern "C" int APS5_VABI getrusage_nid_postfix(int who, ResourceUsage* usage);
extern "C" int* APS5_VABI __error_nid_postfix();

static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;
static void Require(bool value) { if (!value) std::abort(); }
static void RequireError(int who, ResourceUsage* usage, int error) {
    *__error_nid_postfix() = 0;
    Require(getrusage_nid_postfix(who, usage) == -1);
    Require(*__error_nid_postfix() == error);
}
static std::int64_t CpuMicros(int who) {
    ResourceUsage usage{};
    Require(getrusage_nid_postfix(who, &usage) == 0);
    return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
}
static void SpinUntil(int who, std::int64_t micros) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    volatile std::uint64_t sink = 0;
    while (CpuMicros(who) < micros) {
        for (int i = 0; i < 100000; ++i) sink = sink + i;
        Require(std::chrono::steady_clock::now() < deadline);
    }
}
int main() {
    ResourceUsage rejected{};
    for (int who : {-2, 2, 3, 100, 0x7fffffff, -0x7fffffff - 1}) RequireError(who, &rejected, GUEST_EINVAL);
    RequireError(2, nullptr, GUEST_EINVAL);
    for (int who : {-1, 0, 1}) RequireError(who, nullptr, GUEST_EFAULT);
    ResourceUsage children;
    std::memset(&children, 0xff, sizeof(children));
    Require(getrusage_nid_postfix(-1, &children) == 0);
    const ResourceUsage empty{};
    Require(std::memcmp(&children, &empty, sizeof(children)) == 0);
    SpinUntil(0, 400000);
    const std::int64_t mainThread = CpuMicros(1);
    Require(mainThread >= 200000);
    std::thread worker([] {
        const std::int64_t fresh = CpuMicros(1);
        Require(fresh < 100000);
        SpinUntil(1, fresh + 100000);
        Require(CpuMicros(0) >= CpuMicros(1) + 300000);
    });
    worker.join();
    Require(CpuMicros(0) >= 500000);
}
