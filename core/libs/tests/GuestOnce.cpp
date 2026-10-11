#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>

struct GuestOnce { std::int32_t state; void* mutex; };
static_assert(sizeof(GuestOnce) == 16 && offsetof(GuestOnce, mutex) == 8);
using Initializer = void (APS5_VABI *)();
extern "C" {
int APS5_VABI pthread_once_nid_postfix(GuestOnce*, Initializer);
int APS5_VABI scePthreadOnce(std::int32_t*, Initializer);
int APS5_VABI scePthreadCreate(Pthread*, const PthreadAttr*, PthreadEntry, void*, const char*);
int APS5_VABI scePthreadJoin(Pthread, void**);
void APS5_VABI scePthreadExit(void*);
}
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);
static void Require(bool value) { if (!value) std::abort(); }
static GuestOnce control{}, nested{}, retry{};
static std::atomic<int> calls{0}, arrived{0};
static int published = 0, nestedValue = 0, attempts = 0;
static void APS5_VABI Nested() { nestedValue = 17; }
static void APS5_VABI Initialize() {
    ++calls;
    while (arrived.load() != 16) std::this_thread::yield();
    Require(pthread_once_nid_postfix(&nested, Nested) == 0);
    published = 42;
}
static void APS5_VABI Retry() {
    if (++attempts == 1) throw 7;
}
static void APS5_VABI ExitThread() { scePthreadExit(nullptr); }
static void APS5_VABI Count() { ++attempts; }
static void* APS5_VABI PosixExitCaller(void* target) {
    pthread_once_nid_postfix(static_cast<GuestOnce*>(target), ExitThread);
    return nullptr;
}
static void* APS5_VABI SceExitCaller(void* target) {
    scePthreadOnce(static_cast<std::int32_t*>(target), ExitThread);
    return nullptr;
}
static void CheckExitInsideInitializer(PthreadEntry caller, GuestOnce& control) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, caller, &control, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(control.state == 0);
    attempts = 0;
    Require(pthread_once_nid_postfix(&control, Count) == 0 && attempts == 1 && control.state == 1);
}

int main() {
    std::array<std::thread, 16> workers;
    for (auto& worker : workers) worker = std::thread([] {
        ++arrived;
        Require(pthread_once_nid_postfix(&control, Initialize) == 0);
        Require(published == 42 && nestedValue == 17);
    });
    for (auto& worker : workers) worker.join();
    Require(calls == 1 && control.state == 1 && control.mutex == nullptr);
    Require(pthread_once_nid_postfix(&control, Initialize) == 0 && calls == 1);
    try { pthread_once_nid_postfix(&retry, Retry); std::abort(); }
    catch (int value) { Require(value == 7 && retry.state == 0); }
    Require(pthread_once_nid_postfix(&retry, Retry) == 0 && attempts == 2);
    Require(pthread_once_nid_postfix(nullptr, Initialize) == 22);
    Require(pthread_once_nid_postfix(&control, nullptr) == 22);
    GuestOnce invalid{9, nullptr};
    Require(pthread_once_nid_postfix(&invalid, Initialize) == 22);
    static GuestOnce posixExit{}, sceExit{}, sceRetry{};
    CheckExitInsideInitializer(PosixExitCaller, posixExit);
    CheckExitInsideInitializer(SceExitCaller, sceExit);
    attempts = 0;
    try { scePthreadOnce(&sceRetry.state, Retry); std::abort(); }
    catch (int value) { Require(value == 7 && sceRetry.state == 0); }
    Require(scePthreadOnce(&sceRetry.state, Retry) == 0 && attempts == 2 && sceRetry.state == 1);
    Require(scePthreadOnce(&sceRetry.state, Count) == 0 && attempts == 2);
    Require(scePthreadOnce(nullptr, Count) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadOnce(&sceRetry.state, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadOnce(&invalid.state, Count) == SCE_KERNEL_ERROR_EINVAL && attempts == 2);
}
