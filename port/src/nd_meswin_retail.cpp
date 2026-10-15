#include "nd_meswin.hpp"

#include "port.h"

#ifdef PORT

#include "options/retail_system_mes.hpp"

PORT_OVERRIDE u16 *ClsMes::GetTextLineDataTop(int line_id) {
    if (u16 *override_text = PortRetailSystemMessageOverride(line_id)) {
        return override_text;
    }

    short *table = buff;
    if (table == nullptr) {
        return nullptr;
    }

    const int count = *table;
    short    *entries = table + 1;

    for (int i = 0; i < count; i++) {
        if (line_id == static_cast<int>(static_cast<unsigned short>(entries[i * 2 + 1]))) {
            return reinterpret_cast<u16 *>(entries + count + static_cast<unsigned short>(entries[i * 2 + 2]));
        }
    }

    return nullptr;
}

PORT_OVERRIDE u16 *ClsMes::GetTextLineDataTop_system(int line_id) {
    if (u16 *override_text = PortRetailSystemMessageOverride(line_id)) {
        return override_text;
    }

    short *table = buff_system;
    if (table == nullptr) {
        return nullptr;
    }

    const int count = *table;
    short    *entries = table + 1;

    for (int i = 0; i < count; i++) {
        if (line_id == static_cast<int>(static_cast<unsigned short>(entries[i * 2 + 1]))) {
            return reinterpret_cast<u16 *>(entries + count + static_cast<unsigned short>(entries[i * 2 + 2]));
        }
    }

    return nullptr;
}

#endif
