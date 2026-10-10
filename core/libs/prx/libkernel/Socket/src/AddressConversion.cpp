#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {
constexpr int InetFamily = 2;
constexpr int Inet6Family = 28;

int digitValue(char c) {
    return c >= '0' && c <= '9' ? c - '0' : -1;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parseV4(const char* text, unsigned char* destination) {
    unsigned char octets[4]{};
    std::size_t index = 0;
    std::size_t count = 0;
    bool sawDigit = false;
    for (; *text; ++text) {
        const int digit = digitValue(*text);
        if (digit >= 0) {
            const unsigned next = octets[index] * 10u + static_cast<unsigned>(digit);
            if (sawDigit && octets[index] == 0) return false;
            if (next > 255) return false;
            octets[index] = static_cast<unsigned char>(next);
            if (!sawDigit) {
                if (++count > 4) return false;
                sawDigit = true;
            }
        } else if (*text == '.' && sawDigit) {
            if (count == 4) return false;
            octets[++index] = 0;
            sawDigit = false;
        } else {
            return false;
        }
    }
    if (count < 4) return false;
    std::memcpy(destination, octets, sizeof(octets));
    return true;
}

bool parseV6(const char* text, unsigned char* destination) {
    unsigned char bytes[16]{};
    std::size_t position = 0;
    std::size_t colon = sizeof(bytes) + 1;
    if (*text == ':' && *++text != ':') return false;
    const char* token = text;
    unsigned groupDigits = 0;
    unsigned value = 0;
    for (; *text; ++text) {
        const int digit = hexValue(*text);
        if (digit >= 0) {
            value = value << 4 | static_cast<unsigned>(digit);
            if (++groupDigits > 4) return false;
            continue;
        }
        if (*text == ':') {
            token = text + 1;
            if (groupDigits == 0) {
                if (colon <= sizeof(bytes)) return false;
                colon = position;
                continue;
            }
            if (text[1] == '\0') return false;
            if (position + 2 > sizeof(bytes)) return false;
            bytes[position++] = static_cast<unsigned char>(value >> 8);
            bytes[position++] = static_cast<unsigned char>(value);
            groupDigits = 0;
            value = 0;
            continue;
        }
        if (*text == '.' && position + 4 <= sizeof(bytes) && parseV4(token, bytes + position)) {
            position += 4;
            groupDigits = 0;
            break;
        }
        return false;
    }
    if (groupDigits != 0) {
        if (position + 2 > sizeof(bytes)) return false;
        bytes[position++] = static_cast<unsigned char>(value >> 8);
        bytes[position++] = static_cast<unsigned char>(value);
    }
    if (colon <= sizeof(bytes)) {
        if (position == sizeof(bytes)) return false;
        const std::size_t tail = position - colon;
        std::memmove(bytes + sizeof(bytes) - tail, bytes + colon, tail);
        std::memset(bytes + colon, 0, sizeof(bytes) - tail - colon);
        position = sizeof(bytes);
    }
    if (position != sizeof(bytes)) return false;
    std::memcpy(destination, bytes, sizeof(bytes));
    return true;
}

std::size_t formatV4(const unsigned char* source, char* text) {
    char* end = text;
    for (std::size_t index = 0; index < 4; ++index) {
        if (index != 0) *end++ = '.';
        const unsigned value = source[index];
        if (value >= 100) *end++ = static_cast<char>('0' + value / 100);
        if (value >= 10) *end++ = static_cast<char>('0' + value / 10 % 10);
        *end++ = static_cast<char>('0' + value % 10);
    }
    *end = '\0';
    return static_cast<std::size_t>(end - text);
}

std::size_t formatV6(const unsigned char* source, char* text) {
    unsigned words[8]{};
    for (std::size_t index = 0; index < 16; ++index)
        words[index / 2] |= static_cast<unsigned>(source[index]) << ((1 - index % 2) * 8);
    int bestBase = -1;
    int bestLength = 0;
    int currentBase = -1;
    int currentLength = 0;
    for (int index = 0; index < 8; ++index) {
        if (words[index] == 0) {
            if (currentBase == -1) {
                currentBase = index;
                currentLength = 1;
            } else {
                ++currentLength;
            }
        } else if (currentBase != -1) {
            if (bestBase == -1 || currentLength > bestLength) {
                bestBase = currentBase;
                bestLength = currentLength;
            }
            currentBase = -1;
        }
    }
    if (currentBase != -1 && (bestBase == -1 || currentLength > bestLength)) {
        bestBase = currentBase;
        bestLength = currentLength;
    }
    if (bestBase != -1 && bestLength < 2) bestBase = -1;
    char* end = text;
    for (int index = 0; index < 8; ++index) {
        if (bestBase != -1 && index >= bestBase && index < bestBase + bestLength) {
            if (index == bestBase) *end++ = ':';
            continue;
        }
        if (index != 0) *end++ = ':';
        if (index == 6 && bestBase == 0 &&
            (bestLength == 6 || (bestLength == 7 && words[7] != 1) || (bestLength == 5 && words[5] == 0xffff))) {
            end += formatV4(source + 12, end);
            break;
        }
        static constexpr char hex[] = "0123456789abcdef";
        bool started = false;
        for (int shift = 12; shift >= 0; shift -= 4) {
            const unsigned nibble = words[index] >> shift & 0xf;
            if (nibble != 0 || started || shift == 0) {
                *end++ = hex[nibble];
                started = true;
            }
        }
    }
    if (bestBase != -1 && bestBase + bestLength == 8) *end++ = ':';
    *end = '\0';
    return static_cast<std::size_t>(end - text);
}
}

extern "C" {
int APS5_VABI __inet_pton_nid_postfix(int family, const char* text, void* destination) {
    if (family != InetFamily && family != Inet6Family) { *__error_nid_postfix() = 47; return -1; }
    if (!text || !destination) { *__error_nid_postfix() = 14; return -1; }
    unsigned char address[16];
    if (!(family == InetFamily ? parseV4(text, address) : parseV6(text, address))) return 0;
    std::memcpy(destination, address, family == InetFamily ? 4 : 16);
    return 1;
}

const char* APS5_VABI __inet_ntop_nid_postfix(int family, const void* source,
                                            char* destination, std::uint32_t capacity) {
    if (family != InetFamily && family != Inet6Family) { *__error_nid_postfix() = 47; return nullptr; }
    if (!source || !destination) { *__error_nid_postfix() = 14; return nullptr; }
    char text[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
    const auto* address = static_cast<const unsigned char*>(source);
    const auto length = family == InetFamily ? formatV4(address, text) : formatV6(address, text);
    if (length + 1 > capacity) { *__error_nid_postfix() = 28; return nullptr; }
    std::memcpy(destination, text, length + 1);
    return destination;
}
}
