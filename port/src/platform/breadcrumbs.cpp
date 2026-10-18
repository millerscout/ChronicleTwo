#include "platform/breadcrumbs.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "platform/log.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

// A seqlock: the sequence is odd while a writer copies, so a reader that sees it change copies again.
struct Slot {
    std::atomic<unsigned> sequence{0};
    std::atomic_flag      writing = ATOMIC_FLAG_INIT;
    char                  value[kCrumbValueSize] = {};
};

std::array<Slot, static_cast<int>(Crumb::Count)> slots;

constexpr std::string_view kNames[] = {"mode", "scene", "menu", "floor", "asset"};
static_assert(std::size(kNames) == static_cast<int>(Crumb::Count));

void Write(int fd, const char *data, std::size_t size) {
#ifdef _WIN32
    _write(fd, data, static_cast<unsigned>(size));
#else
    while (size > 0) {
        const ssize_t written = ::write(fd, data, size);
        if (written <= 0) {
            return;
        }
        data += written;
        size -= static_cast<std::size_t>(written);
    }
#endif
}

// Copies a consistent value into out; the crash handler gives up after a few tries rather than spin.
void Read(const Slot &slot, char (&out)[kCrumbValueSize], int attempts) {
    for (int attempt = 0; attempt < attempts; ++attempt) {
        const unsigned before = slot.sequence.load(std::memory_order_acquire);
        std::memcpy(out, slot.value, sizeof out);
        std::atomic_thread_fence(std::memory_order_acquire);
        if ((before & 1) == 0 && slot.sequence.load(std::memory_order_relaxed) == before) {
            break;
        }
    }
    out[kCrumbValueSize - 1] = '\0';
}

} // namespace

void BreadcrumbSet(Crumb crumb, std::string_view value) {
    Slot &slot = slots[static_cast<int>(crumb)];
    value = value.substr(0, kCrumbValueSize - 1);
    while (slot.writing.test_and_set(std::memory_order_acquire)) {
    }
    if (std::string_view(slot.value) == value) {
        slot.writing.clear(std::memory_order_release);
        return;
    }
    slot.sequence.fetch_add(1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    if (!value.empty()) {
        std::memcpy(slot.value, value.data(), value.size());
    }
    std::memset(slot.value + value.size(), 0, sizeof slot.value - value.size());
    slot.sequence.fetch_add(1, std::memory_order_release);
    slot.writing.clear(std::memory_order_release);
    CT_LOG_DEBUG("crumb", "{} = {}", BreadcrumbName(crumb), value);
}

void BreadcrumbSetf(Crumb crumb, const char *format, ...) {
    char    value[kCrumbValueSize];
    va_list args;
    va_start(args, format);
    const int size = std::vsnprintf(value, sizeof value, format, args);
    va_end(args);
    BreadcrumbSet(crumb, std::string_view(value, static_cast<std::size_t>(std::clamp(size, 0, kCrumbValueSize - 1))));
}

std::string BreadcrumbGet(Crumb crumb) {
    char value[kCrumbValueSize];
    Read(slots[static_cast<int>(crumb)], value, 1 << 20);
    return value;
}

std::string_view BreadcrumbName(Crumb crumb) {
    return kNames[static_cast<int>(crumb)];
}

void BreadcrumbsClear() {
    for (int crumb = 0; crumb < static_cast<int>(Crumb::Count); ++crumb) {
        BreadcrumbSet(static_cast<Crumb>(crumb), {});
    }
}

void BreadcrumbsWrite(int fd) {
    for (int crumb = 0; crumb < static_cast<int>(Crumb::Count); ++crumb) {
        char value[kCrumbValueSize];
        Read(slots[crumb], value, 4);
        Write(fd, kNames[crumb].data(), kNames[crumb].size());
        Write(fd, ": ", 2);
        Write(fd, value, std::strlen(value));
        Write(fd, "\n", 1);
    }
}
