#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
Pthread APS5_VABI pthread_self_nid_postfix(void);
int APS5_VABI pthread_getaffinity_np_nid_postfix(Pthread thread, std::size_t size, void* set);
int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr);
int APS5_VABI pthread_getcpuclockid_nid_postfix(Pthread thread, int* clockId);
}

static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;
static constexpr std::size_t CPUSET_BYTES = 32;
static constexpr unsigned char UNTOUCHED = 0xAA;
static constexpr int UNTOUCHED_CLOCK = 0x5A5A5A5A;
static constexpr auto GIVE_UP = std::chrono::seconds(10);

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI Worker(void* arg) {
    static_cast<std::future<void>*>(arg)->get();
    return nullptr;
}

static void RequireQueriesSucceed(Pthread thread) {
    std::array<unsigned char, CPUSET_BYTES> set{};
    Require(pthread_getaffinity_np_nid_postfix(thread, CPUSET_BYTES, set.data()) == 0);
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    Require(pthread_attr_get_np_nid_postfix(thread, &attr) == 0);
    Require(scePthreadAttrDestroy(&attr) == 0);
    int clockId = UNTOUCHED_CLOCK;
    Require(pthread_getcpuclockid_nid_postfix(thread, &clockId) == 0 && clockId != UNTOUCHED_CLOCK);
}

static bool QueriesReportNoSuchThread(Pthread thread) {
    std::array<unsigned char, CPUSET_BYTES> set;
    set.fill(UNTOUCHED);
    const bool affinity = pthread_getaffinity_np_nid_postfix(thread, CPUSET_BYTES, set.data()) == GUEST_ESRCH;
    if (affinity) {
        for (unsigned char byte : set) Require(byte == UNTOUCHED);
    }

    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    const bool attributes = pthread_attr_get_np_nid_postfix(thread, &attr) == GUEST_ESRCH;
    Require(scePthreadAttrDestroy(&attr) == 0);

    int clockId = UNTOUCHED_CLOCK;
    const bool clock = pthread_getcpuclockid_nid_postfix(thread, &clockId) == GUEST_ESRCH;
    if (clock) Require(clockId == UNTOUCHED_CLOCK);
    return affinity && attributes && clock;
}

int main() {
    const Pthread self = pthread_self_nid_postfix();
    RequireQueriesSucceed(self);

    std::array<unsigned char, CPUSET_BYTES> set{};
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == 0);
    int clockId = 0;
    Require(pthread_getaffinity_np_nid_postfix(nullptr, CPUSET_BYTES, set.data()) == GUEST_EINVAL);
    Require(pthread_attr_get_np_nid_postfix(nullptr, &attr) == GUEST_EINVAL);
    Require(pthread_getcpuclockid_nid_postfix(nullptr, &clockId) == GUEST_EINVAL);
    Require(scePthreadAttrDestroy(&attr) == 0);

    std::promise<void> release;
    auto released = release.get_future();
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &released, nullptr) == 0);
    RequireQueriesSucceed(thread);
    release.set_value();

    const auto deadline = std::chrono::steady_clock::now() + GIVE_UP;
    while (!QueriesReportNoSuchThread(thread)) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(QueriesReportNoSuchThread(thread));
}
