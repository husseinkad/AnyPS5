#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
extern "C" {
int* APS5_VABI __error_nid_postfix();
}
using RenameFunction = int (APS5_VABI*)(const char*, const char*);
static RenameFunction LoadKernelRename(const char* name) {
#ifdef _WIN32
    const HMODULE module = LoadLibraryA(KERNEL_PRX_PATH);
    if (module == nullptr) return nullptr;
    const auto function = reinterpret_cast<RenameFunction>(reinterpret_cast<void*>(GetProcAddress(module, name)));
    HMODULE owner = nullptr;
    if (function == nullptr || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(function), &owner) || owner != module) return nullptr;
    return function;
#else
    void* module = dlopen(KERNEL_PRX_PATH, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) return nullptr;
    void* function = dlsym(module, name);
    Dl_info owner{};
    void* self = dlsym(module, "sceKernelRename");
    Dl_info expected{};
    if (function == nullptr || self == nullptr || !dladdr(function, &owner) || !dladdr(self, &expected) ||
        owner.dli_fname == nullptr || expected.dli_fname == nullptr || std::strcmp(owner.dli_fname, expected.dli_fname) != 0) return nullptr;
    return reinterpret_cast<RenameFunction>(function);
#endif
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Kernel rename check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    const RenameFunction rename_nid_postfix = LoadKernelRename("rename_nid_postfix");
    if (rename_nid_postfix == nullptr) { std::fputs("rename_nid_postfix is not defined by libkernel\n", stderr); return 1; }
    const RenameFunction kernelRename = LoadKernelRename("sceKernelRename");
    if (kernelRename == nullptr) { std::fputs("sceKernelRename is not defined by libkernel\n", stderr); return 1; }
    const auto root = std::filesystem::path("anyps5-kernel-rename-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "file.txt";
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(file); stream << "retained"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file) && std::filesystem::exists(renamed));
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(rename_nid_postfix(nullptr, renamed.string().c_str()) == -1 && *__error_nid_postfix() == 14);
    Require(rename_nid_postfix(renamed.string().c_str(), nullptr) == -1 && *__error_nid_postfix() == 14);
    Require(rename_nid_postfix("", renamed.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), "") == -1 && *__error_nid_postfix() == 2);
    const auto directory = root / "directory";
    const auto fullDirectory = root / "full";
    Require(std::filesystem::create_directory(directory) && std::filesystem::create_directory(fullDirectory));
    { std::ofstream stream(fullDirectory / "entry.txt"); stream << "entry"; }
    const auto missingParent = (root / "missing" / "target").string();
    constexpr int Enoent = static_cast<int>(0x80020002u);
    constexpr int Enotdir = static_cast<int>(0x80020014u);
    constexpr int Eisdir = static_cast<int>(0x80020015u);
    constexpr int Einval = static_cast<int>(0x80020016u);
    constexpr int Enotempty = static_cast<int>(0x80020042u);
    Require(kernelRename(renamed.string().c_str(), missingParent.c_str()) == Enoent);
    Require(kernelRename(renamed.string().c_str(), directory.string().c_str()) == Eisdir);
    Require(kernelRename(directory.string().c_str(), renamed.string().c_str()) == Enotdir);
    Require(kernelRename(directory.string().c_str(), fullDirectory.string().c_str()) == Enotempty);
    Require(kernelRename(directory.string().c_str(), (directory / "inside").string().c_str()) == Einval);
    Require(kernelRename(file.string().c_str(), renamed.string().c_str()) == Enoent);
    Require(std::filesystem::exists(renamed) && std::filesystem::is_directory(directory) && std::filesystem::exists(fullDirectory / "entry.txt"));
    Require(kernelRename(renamed.string().c_str(), file.string().c_str()) == 0);
    Require(std::filesystem::exists(file) && !std::filesystem::exists(renamed));
    std::filesystem::remove_all(root);
    return 0;
}
