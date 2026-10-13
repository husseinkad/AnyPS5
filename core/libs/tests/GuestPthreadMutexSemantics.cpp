#include "SceTypes.hpp"
#include <chrono>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr);
int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type);
int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr);
int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex);
int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime);
int APS5_VABI clock_gettime_nid_postfix(int clockId, KernelTimespec* tp);
}

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ETIMEDOUT = 60;
static constexpr int ERRORCHECK = 1;
static constexpr int RECURSIVE = 2;
static constexpr int NORMAL = 3;
static constexpr int ADAPTIVE = 4;

static void Require(bool value) { if (!value) std::abort(); }

static KernelTimespec After(long long seconds, long long nanos) {
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(0, &now) == 0);
    const long long total = now.tv_nsec + nanos;
    return KernelTimespec{now.tv_sec + seconds + total / 1000000000, total % 1000000000};
}

static void CheckSelfTimedlock(int type) {
    PthreadMutexattr attr = nullptr;
    PthreadMutex mutex = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, type) == 0);
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);

    const KernelTimespec deadline = After(0, 120000000);
    const auto start = std::chrono::steady_clock::now();
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &deadline) == GUEST_ETIMEDOUT);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(100));

    const KernelTimespec past = After(-5, 0);
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &past) == GUEST_ETIMEDOUT);
    const KernelTimespec negative{-1, 0};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &negative) == GUEST_EINVAL);
    const KernelTimespec lowNanos{After(1, 0).tv_sec, -1};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &lowNanos) == GUEST_EINVAL);
    const KernelTimespec highNanos{After(1, 0).tv_sec, 1000000000};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &highNanos) == GUEST_EINVAL);

    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == GUEST_EPERM);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

static void CheckRecursiveTimedlock() {
    PthreadMutexattr attr = nullptr;
    PthreadMutex mutex = nullptr;
    Require(pthread_mutexattr_init_nid_postfix(&attr) == 0);
    Require(pthread_mutexattr_settype_nid_postfix(&attr, RECURSIVE) == 0);
    Require(pthread_mutex_init_nid_postfix(&mutex, &attr) == 0);
    Require(pthread_mutex_lock_nid_postfix(&mutex) == 0);
    const KernelTimespec negative{-1, 0};
    Require(pthread_mutex_timedlock_nid_postfix(&mutex, &negative) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == 0);
    Require(pthread_mutex_unlock_nid_postfix(&mutex) == GUEST_EPERM);
    Require(pthread_mutex_destroy_nid_postfix(&mutex) == 0);
    Require(pthread_mutexattr_destroy_nid_postfix(&attr) == 0);
}

int main() {
    for (const int type : {ERRORCHECK, NORMAL, ADAPTIVE}) CheckSelfTimedlock(type);
    CheckRecursiveTimedlock();
}
