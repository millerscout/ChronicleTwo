# Tasks

## 1. Game text as UTF-8

- [x] 1.1 Add `legacy_encoding::NameToUtf8` (ASCII as is, `[UNI00xx]` tags to code points); verify a test that German, French and Spanish names decode with no tag text left
- [x] 1.2 Move `message_text.cpp`'s glyph mapping (`Append`) into a table shared with a new `legacy_encoding::MessageToUtf8(const u16 *, const MessageInserts &)`. It decodes glyphs, newline and space; fills name (`0xFAEA`–`0xFAF9`) and number (`0xFBxx`) insertions; turns button glyphs `0xFD06`–`0xFD09` into `options::ButtonName`; drops colour, justify, spacing, move, wait, voice and page codes; stops at the end code; and warns once per unknown code. Verify tests for an encode-then-decode round trip of ASCII and Latin-1, a two-line message, colour codes, a number and a name insertion, a button glyph, and an unknown code warned once

## 2. The ledger view and its adapter

- [x] 2.1 Add `port/src/ui/item_ledger.{hpp,cpp}` with `ItemLedgerView`, `LedgerTab`, `LedgerEntry` and `WeaponDetail` (design decision 1), the `ITEM_DATA_TYPE` to tab table (decision 3), and an `ItemSource` interface; verify a test that every `ITEM_DATA_TYPE` but `ITEM_DATA_NONE` maps to exactly one tab
- [x] 2.2 Implement `BuildLedger(CUserDataManager *, const ItemSource &)`. The retail accessors are not const, and building still does not write the save. Bag tabs come from `GetUsedDataPtr` over `ITEM_BOARD_ALL` in bag order, with overflow places marked and last. Equipped gets headings for Max and Monica (five `equip` slots, three `active_item` slots) and the Ridepod (four `parts`), empty slots shown as empty. Gilda comes from `money`. Verify `item_ledger_test` with a test-built `CSaveData` and a fake source: each spec tab scenario (one item per tab, empty tab, full bag with overflow, Max's melee slot, a stacked count of 5), and that building leaves the save byte-for-byte unchanged
- [x] 2.3 Add `GameItemSource` and `ItemLedgerUser`:
  - `GetItemDataType`; `GetName(ITEM_NAME_TYPE_PLAIN)` through `NameToUtf8`; `GetNum` and `CheckTypeEnableStack`.
  - The description: `ClsMes::GetTextLineDataTop` on `GetMenuMainMessageBuffer()` for `GetItemMessageNo(item, 1)`, with `GetMsgAddInfo` inserts, through `MessageToUtf8`. `GetTextLineDataTop` is the retail lookup, so port message overrides apply.
  - Weapon stats: `GetStatusParam` at `GetSaveData()->now_time`, plus `whp`, `abs`, `level` and `fusion_point`, for `USED_ITEM_TYPE_WEAPON` only.
  - The user data in use: `DngUserData` in `LOOP_DUNGEON` when set, else `GetUserDataMan()`.
  
  Verify `item_ledger_test` still passes
- [ ] 2.4 Load the data archive in the description test and decode every item description in `allmenu.mes` for the five PAL languages, with no unknown-code warnings. `ItemLedger.DescriptionsDecodeOrSkipWithoutTheArchive` already skips when the test process has not loaded the archive; the remaining work is to load `menu/<language>` and the item tables the way `dataread_test` loads extracted data, then assert the warnings stay empty

## 3. Icons

- [ ] 3.1 Add the hub's icon load. Entering Item loads `allitem.json` from the menu pack's directory into a named hub texture block unless `wepicon` and `itemicon` are registered; closing the hub deletes the block. Entries carry a binding and 32 px cell from `icon_texture_no` and `GetItemIconNo`. Verify a test that the block constant differs from the retail blocks in use (including `0x50` and `MenuItemIconTextureBlock`), and a test that the cell for an icon number matches `GetMenuItemIconTexGetXY`'s for the same item

## 4. The Item screen

- [ ] 4.1 Add the `ItemRow` list widget: icon (or a tab badge when it has none), name, and a right-aligned count or slot text, with an overflow mark, reusing `List`'s focus, scroll and pointer handling. Verify `ui_widgets_test` cases for scrolling to the focused row, a pointer pick, the badge fallback and the count shown only for stacking items
- [ ] 4.2 Build the Item screen in place of `Placeholder` for `Screen::Item` (decision 7): `TabBar`, the list page, the details page with `TextBlock` and weapon `StatRow`s, Gilda, and the ABS Bank button entering `Screen::AbsBank`. Rebuild the view on entering Item and on returning to it, and draw the no-save and empty-tab states. Verify `ui_hub_preview_test`: Item opens the ledger, tab buttons cycle every tab, ABS Bank and back returns with the ABS Bank button focused, no save says so and backs out, and the existing navigation tests still pass
- [ ] 4.3 Add the i18n keys (`item.tab.*`, `item.slot.*`, `item.stat.*`, `item.empty`, `item.no_save`, `item.overflow`, `item.gilda`) to `strings_en.cpp` and the fr/de/it/es catalogs, using the game's own attribute terms; verify the bundled-catalog completeness test passes
- [ ] 4.4 Take 1080p screenshots with `--ui-screen item` in `parchment`, `dc2` and `chronicle`, in town and in a dungeon (`--jump`), with a weapon focused; verify that icons, names, descriptions and weapon numbers match the retail item menu for the same save, and that the wide window (2560x1080) keeps both pages readable

## 5. Docs and checks

- [ ] 5.1 Describe the Item screen in `docs/PC.md`'s hub section (what it shows, that it is read-only for now, the tab buttons, ABS Bank from it); verify the section matches the screen in the 4.4 screenshots
- [ ] 5.2 Run `ctest --test-dir port/build/pc -R "^Ui|I18n|ItemLedger|LegacyEncoding|MessageText"` and the full `ctest --preset linux-x64` (comparing failures against the known environmental ones), and confirm `git diff --stat ps2 tools/kazarov` shows no changes from this change

## Workflow follow-up

- Expand this change with the later stages as we go, each as its own task group with its spec requirements:
  1. equip and unequip, active items, and use on a character;
  2. throw away and repair;
  3. weapon build-up, spectrumize and attachments;
  4. Ridepod, monster form and fishing.
- Archive after `ui-hub-screens-themes`, since this change relies on its hub and navigation.
