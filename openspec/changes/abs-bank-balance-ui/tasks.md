# Tasks

## 1. Shared draw helper

- [x] 1.1 Add `PortAbsBankFormatBalance(char *buf, size_t len)` and `PortAbsBankShouldShowHud()` in `port/src/abs_bank_hud.hpp` / `abs_bank_hud.cpp` (gating: `AbsBankEnabled()`, dungeon field + status bar visibility per design); verify with unit test in `port/src/tests/abs_bank_hud_test.cpp` (`ctest -R AbsBankHud`).

## 2. Dungeon HUD

- [x] 2.1 Call `PortAbsBankDrawDungeonHud(statusbar_rate)` from `dng_status.cpp` after quick loadout draw; position plate to avoid loadout overlap; verify manually in dungeon with banking On or add a focused draw test if a harness exists.
- [x] 2.2 Document dungeon HUD location in `docs/PC.md` under abs bank; verify paragraph mentions status board readout.

## 3. Item menu weapon view

- [x] 3.1 Extend `abs_bank_menu.cpp` to draw bank total on `ITEM_MENU_PANEL_WEAPON` / `ITEM_MENU_PANEL_FISHING_ROD` (read-only, distinct from stats-panel apply overlay); verify via `ctest -R AbsBankHud` or menu test extension.
- [x] 3.2 Ensure stats-panel spend overlay unchanged; run `ctest -R AbsBank`.

## 4. Integration

- [x] 4.1 Run `ctest -R 'AbsBank|AbsBankHud'` and confirm banking Off hides both new readouts.

## Workflow follow-up

- Archive `abs-bank-balance-ui` after review; sync `port/abs-bank-balance-ui` spec when shipping.
