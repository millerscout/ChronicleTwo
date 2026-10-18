# Design

## Context

- The hub (`ui-hub-screens-themes`) routes Item to `Hub::Placeholder`, which links to ABS Bank. Its only item data is `hub_preview::SampleData`.
- The save:
  - The bag is one array, `CUserDataManager::used_data[150]`. The main board holds 138 places, or 144 with `SAVE_FLAG_ITEM_BOARD_EXPANDED` (`GetItemBoardMaxNum(ITEM_BOARD_MAIN)`), and the rest are overflow places, up to `ITEM_BOARD_ALL`.
  - Tabs are not stored. An item's kind is `GetItemDataType(item_no)` (`ITEM_DATA_TYPE`, 35 values).
  - Max and Monica are `chara_data[USER_CHARA_MAX|MONICA]`, each with `equip[5]` (`CHARA_EQUIP_*`) and `active_item[3]`. The Ridepod is `robo_data.parts[4]` (`ROBO_PART_*`), and the Gilda is `money`.
  - Inside a dungeon the game works on `DngUserData` (`&DngSaveData->user_data`). Elsewhere it uses `GetUserDataMan()` (`GetSaveData()->user_data`, null with no save).
- Per item:
  - Name: `CGameDataUsed::GetName(ITEM_NAME_TYPE_*)`, a byte string with `[UNI00xx]` tags in the PAL European languages.
  - Count: `GetNum()`. Weapon data: `data.weapon` (`whp`, `abs`, `level`, `fusion_point`).
  - Stats: `GetStatusParam(values, time)`, which fills attack, durability and eight attributes. The retail weapon panel reads them in that order (`MenuPosFormValueSetWeapon`).
- Descriptions:
  - Each is message `GetItemMessageNo(item_no, 1)` in `allmenu.mes`, inside the menu pack `men0.pac` (`GetMenuMainMessageBuffer()` reads `MenuArg.pack`). The pack is resident in town and in dungeons.
  - The port already looks messages up with `FindLine` (`port/src/message_text.cpp`).
  - Messages are `u16` streams. PAL glyphs are ASCII − 0x20 (− 0x21 past the missing backslash) and Latin-1 as `0xFF00 | ch`. Control codes are `MesCode` (`nd_meswin.hpp`). Name insertions are `0xFAEA`–`0xFAF9` and number insertions `0xFBxx`. Button glyphs are `0xFD06`–`0xFD09` (Cross, Triangle, Circle, Square on PAL).
  - `CGameDataUsed::GetMsgAddInfo` gives a description's inserted names and numbers.
- Icons:
  - Icons live in the `wepicon` (sheet 0) and `itemicon` (sheet 1) atlases from `allitem`. An item's sheet is `CDataCommon::icon_texture_no`, its cell is `GetItemIconNo(item_no)`, and cells are 32 px, 8 per row.
  - The atlases are only resident while the retail menu is open (the dungeon loads them briefly for the HUD, then frees block `0x50`), and `GetMenuItemIconTexInfo` needs `MenuPosData`.
  - The port loads `allitem.json` with `MgTextureEnterDocument(&mgTexManager, path, block, …)` from the menu pack's directory (`AssetDirectoryPath`), as `EnterCommonTextures` does (`port/src/menumain.cpp`).

## Goals / Non-Goals

**Goals:**
- A ledger that can be tested without game data: the screen draws a plain view, and only a thin adapter touches retail state.
- Text that matches the retail item menu for names, descriptions and weapon numbers in the five PAL languages.
- A screen whose structure later stages can add actions to without reworking it.

**Non-Goals:**
- Any action or save write, and any hand-off to the retail item menu.
- Japanese text: the build is PAL, and NTSC-J glyph tables are not decoded.
- Monster form and fishing rod pages. Their items still appear in the bag tabs.
- Sorting other than bag order.

## Decisions

1. **The view is plain data.**
   - `ItemLedgerView { bool has_save; int gilda; std::array<LedgerTab, 9> tabs; }`.
   - Each `LedgerTab` holds `LedgerEntry { std::string name; int count; bool stacks; bool overflow; std::string slot; Icon icon; std::string kind; std::string description; std::optional<WeaponDetail> weapon; }`. Equipped headings are entries with a `heading` flag.
   - The view is built once when the Item screen is entered or returns to the top of the stack, not each frame. The hub freezes play, so the save cannot change underneath it, and building once also keeps message decoding and icon lookup out of the frame loop.
   - Alternative: read the save every frame. It is simpler, but decodes every description sixty times a second for nothing.

2. **One adapter, behind an interface.**
   - `ItemSource` has `Type(item)`, `Name(used)`, `Stacks`, `Count`, `Description(used)` and `Weapon(used)`. `BuildLedger(CUserDataManager *, const ItemSource &)` fills the view from slots and the source. The pointer is not const because the retail accessors are not.
   - The game source wraps the retail calls. Tests pass a fake source over a `CSaveData` built in the test, which is how `ui_hub_preview_test` already sets `PortActiveSaveData`.
   - The user data is `DngUserData` when `PortLoopNo == LOOP_DUNGEON` and it is set (as `quick_loadout.cpp` checks), else `GetUserDataMan()`, else no save.
   - Alternative: call retail functions straight from the screen. That can't be tested without game tables.

3. **Tab mapping is a table over `ITEM_DATA_TYPE`**, with every type assigned and a test checking that:
   - Weapons: 1–4
   - Clothing: 5–10
   - Ridepod: 11–15
   - Attachments: 16–19, 34
   - Items: 22–25
   - Materials: 20
   - Fishing: 29, 30, 32, 35
   - Key items: 21, 26–28, 33
   - `ITEM_DATA_NONE` (empty slot) is skipped.
   - Items in the overflow places (`ITEM_BOARD_MAIN` ≤ index < `ITEM_BOARD_ALL`) follow the main board's items.

4. **Decoding lives in `legacy_encoding`, beside `GameText`.**
   - `NameToUtf8(std::string_view)` turns `[UNI00xx]` tags into code points and passes ASCII through.
   - `MessageToUtf8(const u16 *, const MessageInserts &)` walks to `MES_CODE_END`. It maps glyphs as the inverse of `message_text.cpp`'s `Append`, a newline code to `\n` and a space code to a space. Name and number insertion codes are filled from `MessageInserts`, and button glyphs become `options::ButtonName` for the PS2 button. Colour, justify, space-width, move, wait, voice and page codes are dropped.
   - The glyph inversion shares a table with `Append`, so encoding then decoding a string round-trips, and a test checks it.
   - Alternative: run `ClsMes::MakeMesWinTbl` and read its layout table. It needs a live message window, and it lays text out for the retail font rather than producing text.

5. **Descriptions are found in the resident `allmenu.mes`.**
   - `ClsMes::GetTextLineDataTop(GetItemMessageNo(item, 1))` on `GetMenuMainMessageBuffer()`, with inserts from `GetMsgAddInfo`. That is the retail lookup, so port message overrides apply. `FindLine` stays private to `message_text.cpp`.
   - When the pack is missing (no save, title screen), descriptions are empty.

6. **Icons come from a hub-owned load of `allitem`.**
   - Entering the Item screen loads `allitem.json` into a texture block the hub reserves, unless `wepicon` and `itemicon` are already registered. Closing the hub deletes that block.
   - Rows sample the atlas cell directly (`GetItemIconNo`, 32 px cells) through `mgPortBinding`, without `MenuPosData`.
   - The block number is a named constant, checked by a test against the blocks retail code uses.
   - Alternative: stand up `MenuPosData` and call `GetMenuItemIconTexInfo`. That couples the hub to the retail menu's lifetime.

7. **The screen is a two-page ledger.**
   - `TabBar` across the top, with Gilda and the ABS Bank button at the right.
   - The left page is a new `ItemRow` list widget: icon, name, and a right-aligned count or slot. It reuses `List`'s focus, scroll and pointer logic, at 56 units per row.
   - The right page shows the details: a 128 px icon, the name, the kind line, the description in a `TextBlock`, and for weapons `StatRow`s with bars for durability and ABS.
   - Each page is a `theme.page` panel, so parchment gets paper pages over the blueprint and the other themes their own pages.
   - Focus moves left and right between the list and the ABS Bank button. Up and down move in the list, and the tab buttons change tab.

8. **Strings are i18n keys** (`item.tab.*`, `item.slot.*`, `item.stat.*`, `item.empty`, `item.no_save`, `item.overflow`, `item.gilda`), in English plus the bundled catalogs. Attribute names follow the game's own terms for the five languages.

## Risks / Trade-offs

- [Port message overrides or added lines change a description's codes] → The decoder reads through the same lookup retail draws use, and a test decodes every item description in the data archive and checks that nothing is left undecoded.
- [The hub's texture block collides with a retail block] → A named constant, a test against the known retail blocks, and a delete on hub close, so nothing stays loaded into gameplay.
- [`GetStatusParam` depends on the time of day (some weapons change at night)] → The game time passed is the time when the view is built, as the retail menu passes `MenuMainScene->time`.
- [Equipment worn by Monica's monster form or a Ridepod part with special rules shows differently from retail] → The tab only lists slots, and monster form is a non-goal for now.
- [Opening Item in a dungeon reads `DngUserData`, while the town save holds an older copy] → This is intended. The spec says the dungeon's copy is used inside a dungeon.

## Open Questions

- Whether the ledger keeps each tab's focus and scroll across hub sessions or resets them when the hub opens. Either works with this design; it defaults to resetting.
