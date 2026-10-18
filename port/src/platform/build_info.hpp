#pragma once

#include <cstdint>
#include <string_view>

// Identity of the running executable, read once from its own mapped image so it stays right after the
// file on disk is replaced.
struct BuildInfo {
    char          build_id[41] = {};    // GNU build ID as hex, empty when the image carries none
    std::uintptr_t load_base = 0;       // where the executable is mapped
    char          version[96] = {};    // data version and build time
    char          platform[64] = {};
};

// Filled on first call; later calls return the same object, so the crash handler can read it without work.
const BuildInfo &BuildInfoGet();
