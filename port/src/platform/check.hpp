#pragma once

#include <cstdint>
#include <type_traits>

// Checked invariants, on in every build. A failed check logs the expression, where it is, the message and the
// breadcrumbs at error level, then aborts, which writes a crash report.
[[noreturn]] void CheckFailed(const char *expression, const char *file, int line, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

#define CT_CHECK(condition, ...)                                          \
    do {                                                                  \
        if (!(condition)) [[unlikely]] {                                  \
            CheckFailed(#condition, __FILE__, __LINE__, __VA_ARGS__);     \
        }                                                                 \
    } while (0)

namespace ct {

// Non-null, outside the null page, in canonical user space and aligned for T. No system call: game data can carry
// a bad pointer on hot paths, and this only catches values no mapping could have.
template <class T>
bool PlausiblePointer(const T *pointer) {
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    if (value < 0x1000) {
        return false;
    }
#if UINTPTR_MAX > 0xffffffffu
    if (value >= 0x0000'8000'0000'0000ull) {
        return false;
    }
#endif
    if constexpr (!std::is_void_v<T>) {
        return value % alignof(T) == 0;
    }
    return true;
}

} // namespace ct
