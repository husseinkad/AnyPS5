#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Rename.hpp"
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>

static std::filesystem::path WithoutTrailingSeparator(const std::filesystem::path& path) {
    return path.has_relative_path() && !path.has_filename() ? path.parent_path() : path;
}

int RenamePath_nid_no_patch(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath) {
    constexpr int GuestEnoent = 2;
    constexpr int GuestEnotdir = 20;
    constexpr int GuestEisdir = 21;
    constexpr int GuestEinval = 22;
    constexpr int GuestEnotempty = 66;
    namespace fs = std::filesystem;
    const auto source = WithoutTrailingSeparator(sourcePath);
    const auto destination = WithoutTrailingSeparator(destinationPath);
    const bool sourceNeedsDirectory = source != sourcePath;
    const bool destinationNeedsDirectory = destination != destinationPath;
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    if (sourceStatus.type() == fs::file_type::not_found) return GuestEnoent;
    if (sourceNeedsDirectory && !fs::is_directory(sourceStatus)) return GuestEnotdir;

    const auto parent = destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
    error.clear();
    const auto parentStatus = fs::status(parent, error);
    if (error) return FilesystemError(error);
    if (!fs::exists(parentStatus)) return GuestEnoent;
    if (!fs::is_directory(parentStatus)) return GuestEnotdir;

    error.clear();
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    const bool destinationExists = destinationStatus.type() != fs::file_type::not_found;
    const bool sourceIsDirectory = fs::is_directory(sourceStatus);
    if (destinationNeedsDirectory && !sourceIsDirectory) return GuestEnotdir;
    const bool destinationIsDirectory = destinationExists && fs::is_directory(destinationStatus);
    bool sameFile = false;
    if (destinationExists) {
        error.clear();
        sameFile = fs::equivalent(source, destination, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return FilesystemError(error);
        if (!sameFile && sourceIsDirectory && !destinationIsDirectory) return GuestEnotdir;
        if (!sameFile && !sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
    }
    if (sourceIsDirectory && !sameFile) {
        error.clear();
        const auto canonicalSource = fs::weakly_canonical(source, error);
        if (error) return FilesystemError(error);
        const auto canonicalDestination = fs::weakly_canonical(destination, error);
        if (error) return FilesystemError(error);
        const auto relative = canonicalDestination.lexically_relative(canonicalSource);
        if (!relative.empty() && *relative.begin() != "..") return GuestEinval;
    }
    if (destinationExists && !sameFile && destinationIsDirectory) {
        error.clear();
        if (!fs::is_empty(destination, error)) {
            return error ? FilesystemError(error) : GuestEnotempty;
        }
    }
#ifdef _WIN32
    if (!sameFile && sourceIsDirectory && destinationIsDirectory) {
        auto displaced = destination;
        displaced += ".anyps5-displaced-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        error.clear();
        fs::rename(destination, displaced, error);
        if (error) return FilesystemError(error);
        error.clear();
        fs::rename(source, destination, error);
        const int failure = error ? FilesystemError(error) : 0;
        std::error_code cleanup;
        if (failure) {
            fs::rename(displaced, destination, cleanup);
            if (cleanup) throw std::runtime_error("rename: cannot restore the displaced destination directory");
            return failure;
        }
        fs::remove(displaced, cleanup);
        if (cleanup) throw std::runtime_error("rename: cannot remove the displaced destination directory");
        return 0;
    }
#endif

    error.clear();
    fs::rename(source, destination, error);
    return error ? FilesystemError(error) : 0;
}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        const int error = RenamePath_nid_no_patch(source, destination);
        if (error) { errno = error; return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
