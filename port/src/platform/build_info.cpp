#include "platform/build_info.hpp"

#include <cstdio>
#include <cstring>

#include "kazarov/data_version.hpp"

#if defined(__linux__)
#include <elf.h>
#include <link.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

#if defined(__linux__)
int FindMainImage(dl_phdr_info *info, std::size_t, void *data) {
    // The first object dl_iterate_phdr reports is the executable.
    auto *build = static_cast<BuildInfo *>(data);
    build->load_base = info->dlpi_addr;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_NOTE) {
            continue;
        }
        const auto *cursor = reinterpret_cast<const unsigned char *>(info->dlpi_addr + phdr.p_vaddr);
        const auto *end = cursor + phdr.p_memsz;
        while (cursor + sizeof(ElfW(Nhdr)) <= end) {
            const auto *note = reinterpret_cast<const ElfW(Nhdr) *>(cursor);
            const auto *name = cursor + sizeof(ElfW(Nhdr));
            const auto *desc = name + ((note->n_namesz + 3) & ~3u);
            if (note->n_type == NT_GNU_BUILD_ID && note->n_namesz == 4 && std::memcmp(name, "GNU", 4) == 0) {
                const unsigned length = note->n_descsz < 20 ? note->n_descsz : 20;
                for (unsigned b = 0; b < length; ++b) {
                    std::snprintf(build->build_id + b * 2, 3, "%02x", desc[b]);
                }
                return 1;
            }
            cursor = desc + ((note->n_descsz + 3) & ~3u);
        }
    }
    return 1;
}
#endif

BuildInfo Read() {
    BuildInfo build;
#if defined(__linux__)
    dl_iterate_phdr(FindMainImage, &build);
    std::snprintf(build.platform, sizeof build.platform, "Linux %s",
#if defined(__x86_64__)
                  "x86-64"
#elif defined(__aarch64__)
                  "arm64"
#else
                  "unknown"
#endif
    );
#elif defined(__APPLE__)
    build.load_base = reinterpret_cast<std::uintptr_t>(_dyld_get_image_header(0));
    std::snprintf(build.platform, sizeof build.platform, "macOS arm64");
#elif defined(_WIN32)
    build.load_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::snprintf(build.platform, sizeof build.platform, "Windows x86-64");
#endif
    std::snprintf(build.version, sizeof build.version, "data v%d, built %s %s", KAZAROV_DATA_VERSION, __DATE__, __TIME__);
    return build;
}

} // namespace

const BuildInfo &BuildInfoGet() {
    static const BuildInfo build = Read();
    return build;
}
