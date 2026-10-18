# Proposal

## Why

The hub's Item entry opens a placeholder. Players can't see what they carry without leaving the hub for the game's own item menu, whose look and controls differ from the hub's. A read-only Item screen over the live save is the first step toward replacing that menu. It proves out reading the save, names, descriptions and icons before any action changes the save.

## What Changes

- **Item screen (read-only ledger):** the hub's Item entry opens a ledger page in place of the placeholder:
  - tabs across the top, a scrolling list of entries on the left, and a details panel on the right;
  - built from the save in use: the dungeon's copy inside a dungeon, the town save elsewhere;
  - nothing on it changes the save.
- **Tabs:**
  - Equipped: Max's and Monica's equipment and active items, and the Ridepod's parts.
  - Weapons, Clothing, Ridepod, Attachments, Items, Materials, Fishing, and Key items: the bag sorted by item type.
  - Bag entries keep their bag order. Entries in the overflow places are marked.
- **List rows:** the item's icon, its name (a renamed weapon shows its new name), and its count when it stacks.
- **Details:** the focused entry's large icon, its name, what it is, and its description.
  - Weapons add their level, attack, durability, ABS gauge, the eight attribute values and synthesis points.
- **Header:** the player's Gilda, and an ABS Bank button that opens the existing ABS Bank screen.
- **Empty states:**
  - With no save loaded, the screen says so.
  - An empty tab says it is empty.
- **Game text as UTF-8:** a decoder from the game's text encoding to UTF-8, so the hub's TrueType text can draw item names and descriptions in every game language.
- **Later stages:** actions are left out of this change, which we expand as we go. They are, in order:
  1. equip and unequip, active items, and use on a character;
  2. throw away and repair;
  3. weapon commands: build-up, spectrumize and attachments;
  4. Ridepod, monster form and fishing.

## Capabilities

### New Capabilities
- `port/ui-hub-item`: the hub's Item screen. It covers which save it reads, the tabs and their item types, list rows, the details panel, empty states, and the ABS Bank button. Later stages add its actions.
- `port/game-text-utf8`: decoding game text into UTF-8 for port text. That covers names stored as bytes with `[UNI00xx]` letter tags, and messages stored as 16-bit glyph codes with control and insertion codes, in the PAL languages.

### Modified Capabilities
None. The hub navigation and ABS Bank requirements this screen relies on come from the in-flight change `ui-hub-screens-themes`, and they don't change. Item still leads to ABS Bank, now through a button on the Item screen.

## Impact

- **New code:**
  - `port/src/ui/item_ledger.{hpp,cpp}`: the view structs, the save adapter and the screen.
  - An `ItemRow` list widget in `port/src/ui/widgets.*`.
  - The decoder in `port/src/platform/legacy_encoding.*`.
  - i18n keys for tab names, labels, attribute names and empty states, in English and the bundled fr/de/it/es catalogs.
- **Changed code:** `port/src/ui/hub_preview.cpp`. `Screen::Item` builds the ledger instead of `Placeholder`, and ABS Bank stays reachable from it.
- **Retail code read, not changed:** `CUserDataManager` (bag, `chara_data`, `robo_data`, `money`), `CGameDataUsed` (`GetName`, `GetNum`, `GetStatusParam`, weapon data), `GetItemDataType`, `GetItemMessageNo` with `CDC2Mes::MakeMsg`, and `GetMenuItemIconTexInfo` with `PortIconUV`.
- **Constraints:** no `ps2/` changes and no Kazarov edits.
- **Depends on** `ui-hub-screens-themes`: the hub, its navigation, the themes and i18n.
