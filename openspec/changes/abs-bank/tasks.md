# Tasks

## 1. Core bank logic

- [x] 1.1 Add `port/src/abs_bank.hpp` / `abs_bank.cpp` with mod id `chronicle_two.abs_bank`, `AbsBankEnabled()`, `AbsBankBalance()`, `AbsBankCredit`, `AbsBankDebit`, reading/writing `banked_abs` via `save_extensions_mod.hpp` only in `.cpp`; verify with unit tests in `port/src/tests/abs_bank_test.cpp` (`ctest -R AbsBank`).

## 2. Config and pickup

- [x] 2.1 Add `game.abs_bank` to `Config`, `config.cpp`, PC settings rows/registry (default off); document in `docs/PC.md`; verify option appears in registry test or config parity if present.
- [x] 2.2 Intercept `PULL_ITEM_WEAPON_EXP` collection when enabled to credit the bank instead of `AddExpWeaponParam`; add test that credits bank and does not change equipped ABS (mock or fixture battle info).

## 3. Item menu spend UI

- [x] 3.1 On weapon stats panel with banking on, show bank total and let the player set apply amount (analog/stick) and confirm to transfer ABS from bank to the viewed weapon, calling retail-consistent level-up; hook via port menu override (new `abs_bank_menu.cpp` following `quick_loadout_menu.cpp` patterns); verify with focused test or input regression where feasible.
- [x] 3.2 Add any required `message_text.cpp` entries for labels; verify strings load without missing-message fallback in a smoke test if available.

## 4. Integration

- [x] 4.1 End-to-end test: enable banking, credit via API, apply to weapon in menu logic, save extensions round-trip through `SaveExtensionsSave`/`Load`; run `ctest -R 'AbsBank|SaveExtensions'`.

## Workflow follow-up

- Archive `abs-bank` after review; sync `port/abs-bank` spec when shipping.
