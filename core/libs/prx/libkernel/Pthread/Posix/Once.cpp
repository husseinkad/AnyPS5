#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "../include/Cancel.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace {
// FreeBSD guest ABI: PTHREAD_ONCE_INIT is {0, nullptr}; completed state is 1.
struct GuestOnce {
    std::int32_t state;
    void* mutex;
};
static_assert(offsetof(GuestOnce, mutex) == 8 && sizeof(GuestOnce) == 16);
std::mutex onceMutex;
std::condition_variable onceChanged;

void APS5_VABI AbandonOnce(void* argument) {
    {
        std::lock_guard lock(onceMutex);
        static_cast<GuestOnce*>(argument)->state = 0;
    }
    onceChanged.notify_all();
}
}

extern "C" int APS5_VABI pthread_once_nid_postfix(
    GuestOnce* control, void (APS5_VABI *initialize)()) {
    if (!control || !initialize) return 22;
    std::unique_lock lock(onceMutex);
    onceChanged.wait(lock, [&] { return control->state != 2; });
    if (control->state == 1) return 0;
    if (control->state != 0) return 22;
    control->state = 2;
    lock.unlock();
    // Initializers may initialize other once controls. Never run guest code
    // under the shared lock. The lock publishes all initializer writes.
    std::uintptr_t cleanup[8];
    __pthread_cleanup_push_imp_nid_postfix(AbandonOnce, control, cleanup);
    try {
        initialize();
    } catch (...) {
        __pthread_cleanup_pop_imp_nid_postfix(1);
        throw;
    }
    __pthread_cleanup_pop_imp_nid_postfix(0);
    lock.lock();
    control->state = 1;
    lock.unlock();
    onceChanged.notify_all();
    return 0;
}

extern "C" int APS5_VABI scePthreadOnce(std::int32_t* once, void (APS5_VABI *initialize)()) {
    return pthread_once_nid_postfix(reinterpret_cast<GuestOnce*>(once), initialize) == 0 ? 0 : SCE_KERNEL_ERROR_EINVAL;
}
