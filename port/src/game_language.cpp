#include "game_language.hpp"

#ifdef PORT

#include "editdata.hpp"
#include "gamedata.hpp"
#include "gaiji.hpp"
#include "helpmes.hpp"
#include "inventmn.hpp"
#include "mainloop.hpp"
#include "mapselect.hpp"
#include "mainloop_port.hpp"
#include "monster.hpp"
#include "npccfg.hpp"
#include "nowload.hpp"
#include "options/retail_system_mes.hpp"
#include "platform/config.hpp"
#include "port_options.hpp"
#include "sysmes.hpp"
#include "userdata.hpp"
#include "vlgr_info.hpp"

namespace {
constexpr int kLanguageScratchQuads = 0x186A0;
} // namespace

int ConfigParseGameLanguage(std::string_view name) {
    if (name == "english") {
        return LANG_ENGLISH;
    }
    if (name == "french") {
        return LANG_FRENCH;
    }
    if (name == "german") {
        return LANG_GERMAN;
    }
    if (name == "italian") {
        return LANG_ITALIAN;
    }
    if (name == "spanish") {
        return LANG_SPANISH;
    }
    return 0;
}

const char *ConfigGameLanguageString(int language) {
    switch (language) {
        case LANG_ENGLISH:
            return "english";
        case LANG_FRENCH:
            return "french";
        case LANG_GERMAN:
            return "german";
        case LANG_ITALIAN:
            return "italian";
        case LANG_SPANISH:
            return "spanish";
        default:
            return nullptr;
    }
}

int PortConfiguredGameLanguage() {
    const int language = ConfigGet().game_language;
    if (language < LANG_ENGLISH || language > LANG_SPANISH) {
        return 0;
    }
    return language;
}

void PortPersistGameLanguage(int language) {
    if (language < LANG_ENGLISH || language > LANG_SPANISH) {
        return;
    }
    Config config = ConfigGet();
    if (config.game_language == language) {
        return;
    }
    config.game_language = language;
    ConfigChange(config);
}

// Retail LanguageChange always reads through read_buffer, which during play aliases the active
// scene's load scratch (dungeon BuffReadData, town buffers, and so on). Use the caller's buffer.
PORT_OVERRIDE void LanguageChange(int language, u_long128 *buffer) {
    u_long128 *scratch = buffer != nullptr ? buffer : read_buffer;

    LanguageCode = language;
    GameItemDataManage.LoadItemSystemMes(language);
    LoadHelpMes(scratch);
    LoadMapName(LanguageCode, scratch);
    LanguageEquipChange();
    LoadNPCCfg();
    LoadSystemMes();
    CreateSystemMes();
    LoadFontTex2Img();
    LoadGaijiImg();
    LoadFontTexture();
    LoadFontTblBin();
    LoadEditAnalyzeData(LanguageCode, scratch);
    LoadFilePictureName();
    LoadMonsterLanguage(LanguageCode);
    InfoStack.stack_used = 0;
    InfoStack.lock = 0;
    LoadGameInfo(&InfoStack);
    InitPauseData();
    PortInitPadTable(LanguageCode);
    PortRetailInstallHostOptionSystemLines();
    PortOptionsRefreshRetailLabels();
}

void PortApplyGameLanguage(int language) {
    if (language < LANG_ENGLISH || language > LANG_SPANISH || language == LanguageCode) {
        return;
    }
    mgCMemory *memory = GetMainStack();
    const int  saved_used = memory->stack_used;
    u_long128 *scratch = memory->stAlloc64(kLanguageScratchQuads);
    LanguageChange(language, scratch);
    memory->stack_used = saved_used;
}

#endif
