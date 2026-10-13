#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SECUREFORMATTING_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SECUREFORMATTING_HPP

#include <cstddef>

namespace LibcDetail {

inline bool SecureSprintfArguments(char* buffer, std::size_t size, const char* format) {
    if (buffer != nullptr && format != nullptr && size != 0) return true;
    if (buffer != nullptr && size != 0) buffer[0] = '\0';
    return false;
}

inline int SecureSprintfResult(char* buffer, std::size_t size, int length) {
    if (length >= 0 && static_cast<std::size_t>(length) < size) return length;
    buffer[0] = '\0';
    return length < 0 ? length : -1;
}

}

#endif
