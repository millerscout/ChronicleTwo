#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "save_extensions.hpp"

nlohmann::json *SaveExtensionsMod(const std::string &mod_id);

const nlohmann::json &SaveExtensionsMods();
