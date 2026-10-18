# Proposal

## Why

With `game.abs_bank` enabled, weapon EXP orbs credit a save-side bank, but the only in-game readout today is a small overlay on the **weapon stats** sub-panel in the item menu. Players cannot see their banked total while exploring a dungeon or on the main weapon page without drilling into that panel. A simple, always-available readout fixes discoverability and answers “how much do I have?” without opening PC settings or the extensions JSON.

## What Changes

- Add a **read-only** bank balance display when banking is enabled and `AbsBankBalance() > 0` (or show `0` while enabled—see design).
- **Dungeon:** draw near the existing status board (same hook pattern as quick loadout in `DrawStatusBord` / `dng_status.cpp`), using retail fonts and semi-transparent plate.
- **Item menu:** show bank total on the main **weapon / fishing rod** view (not only `ITEM_MENU_PANEL_WEAPON_STATS`), so opening a weapon from the bag shows the bank without an extra panel.
- Keep **spend** behavior on the weapon stats panel unchanged (`abs-bank`); this change is visibility only.
- Optional PC setting later deferred; no new config toggle in v1 unless design needs hide-when-zero behavior only.

## Capabilities

### New Capabilities

- `port/abs-bank-balance-ui`: Player-visible read-only display of `banked_abs` in dungeon HUD and item-menu weapon flow when `game.abs_bank` is On.

### Modified Capabilities

- (none in `openspec/specs/` yet; behavior builds on implemented `chronicle_two.abs_bank` / `AbsBankBalance()` from the `abs-bank` change.)

## Impact

- **Code:** `port/src/abs_bank_hud.cpp` (or extend `abs_bank_menu.cpp`), hook `DrawStatusBord` or call from existing port `dng_status.cpp` override; extend menu draw for weapon view; reuse `AbsBankBalance()` / `AbsBankEnabled()` from `abs_bank.hpp`.
- **Tests:** focused unit or presentation test for formatting and gating (banking off → no draw); optional screenshot/gpu test out of scope for v1.
- **Docs:** short `docs/PC.md` note under abs bank describing where the balance appears.
- **Dependencies:** requires `abs-bank` and save extensions; no save format changes.
