#pragma once

#include <filesystem>

#include "common.h"

// Per-save-slot JSON sidecars for mod and first-party state outside SAVEDATA_FORMAT.
// Mod data accessors live in save_extensions_mod.hpp (include that only after game headers).

std::filesystem::path SaveExtensionsSidecarPath(const std::filesystem::path &save_image_path);

void SaveExtensionsClear();

bool SaveExtensionsLoad(const std::filesystem::path &save_image_path, u64 unique_counter);

bool SaveExtensionsSave(const std::filesystem::path &save_image_path, u64 unique_counter);

bool SaveExtensionsActive();
