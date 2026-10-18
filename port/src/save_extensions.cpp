#include "save_extensions_mod.hpp"

#include <array>
#include <cstdio>
#include <string>

#include "platform/files.hpp"
#include "platform/log.hpp"

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {
constexpr int           kSchemaVersion = 1;
constexpr std::size_t   kMaxFileBytes = 1024 * 1024;
constexpr std::size_t   kReadBufferSize = kMaxFileBytes + 1;

struct Store {
    bool     active = false;
    bool     dirty = false;
    fs::path bound_image;
    u64      loaded_counter = 0;
    Json     mods = Json::object();
};

Store g_store;

const Json &EmptyMods() {
    static const Json empty = Json::object();
    return empty;
}

bool ModsNonempty(const Json &mods) {
    return mods.is_object() && !mods.empty();
}

bool ParseSidecar(const char *bytes, std::size_t size, u64 image_counter, const std::string &basename) {
    g_store.mods = Json::object();
    g_store.dirty = false;
    if (size == 0) {
        return true;
    }
    Json document = Json::parse(bytes, bytes + size, nullptr, false, true);
    if (!document.is_object()) {
        return true;
    }
    if (!document.contains("schema_version") || !document["schema_version"].is_number_integer() ||
        document["schema_version"].get<int>() != kSchemaVersion) {
        return true;
    }
    if (document.contains("mods") && document["mods"].is_object()) {
        g_store.mods = document["mods"];
    }
    if (document.contains("unique_counter") && document["unique_counter"].is_number_unsigned()) {
        const u64 sidecar_counter = document["unique_counter"].get<u64>();
        if (sidecar_counter != image_counter) {
            CT_LOG_WARN("save extensions", "stale sidecar for {} (file {}, save {})", basename,
                        static_cast<unsigned long long>(sidecar_counter), static_cast<unsigned long long>(image_counter));
        }
    }
    return true;
}
} // namespace

fs::path SaveExtensionsSidecarPath(const fs::path &save_image_path) {
    const std::string name = save_image_path.filename().string();
    return save_image_path.parent_path() / (name + ".extensions.json");
}

void SaveExtensionsClear() {
    g_store = {};
}

bool SaveExtensionsLoad(const fs::path &save_image_path, u64 unique_counter) {
    g_store = {};
    g_store.active = true;
    g_store.bound_image = save_image_path;
    g_store.loaded_counter = unique_counter;

    const fs::path sidecar = SaveExtensionsSidecarPath(save_image_path);
    std::array<char, kReadBufferSize> bytes{};
    const std::size_t                 count = FilesRead(sidecar, bytes.data(), kReadBufferSize - 1);
    if (count == 0 || count >= kReadBufferSize) {
        return true;
    }
    return ParseSidecar(bytes.data(), count, unique_counter, save_image_path.filename().string());
}

bool SaveExtensionsSave(const fs::path &save_image_path, u64 unique_counter) {
    if (!g_store.active) {
        return false;
    }
    if (!g_store.dirty && !ModsNonempty(g_store.mods)) {
        return true;
    }
    const Json document = {
        {"schema_version", kSchemaVersion},
        {"save_file",      save_image_path.filename().string()    },
        {"unique_counter", unique_counter                       },
        {"mods",           g_store.mods                           }
    };
    const std::string text = document.dump(2) + "\n";
    const auto        sidecar = SaveExtensionsSidecarPath(save_image_path);
    if (FilesWrite(sidecar, text.data(), text.size(), true) != FilesResult::kWritten) {
        return false;
    }
    g_store.dirty = false;
    g_store.loaded_counter = unique_counter;
    return true;
}

Json *SaveExtensionsMod(const std::string &mod_id) {
    if (!g_store.active || mod_id.empty()) {
        return nullptr;
    }
    if (!g_store.mods.is_object()) {
        g_store.mods = Json::object();
    }
    if (!g_store.mods.contains(mod_id) || !g_store.mods[mod_id].is_object()) {
        g_store.mods[mod_id] = Json::object();
    }
    g_store.dirty = true;
    return &g_store.mods[mod_id];
}

const Json &SaveExtensionsMods() {
    return g_store.active ? g_store.mods : EmptyMods();
}

bool SaveExtensionsActive() { return g_store.active; }
