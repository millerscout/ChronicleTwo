# Tasks

## 1. Core transfer API

- [x] 1.1 Add `AbsBankTransferToWeapon` / `AbsBankTransferFromWeapon` and `AbsBankWeaponRefundFloor` in `abs_bank.cpp` (headers in `abs_bank.hpp`); unit tests for add cap, refund cap, no level-down, level-up loop on add (`ctest -R AbsBank`).

## 2. Weapon panel input (A)

- [x] 2.1 In `abs_bank_menu.cpp`, handle R2/L2 on `ITEM_MENU_PANEL_WEAPON` when banking is on and extend ask mode is none; wire `KeyStepLocal` (remove or disable stats-panel analog/`PushKey` spend path per design).
- [x] 2.2 Optional: trigger `MenuLevelUpMan` when R2 transfer causes level-up; verify in-game on a test weapon.

## 3. UI and docs

- [x] 3.1 Draw R2/L2 hint on main weapon inspect panel when banking is on; remove stats-panel spend overlay if still present.
- [x] 3.2 Document controls in `docs/PC.md` under `game.abs_bank`.

## 4. Verification

- [x] 4.1 Run `ctest -R 'AbsBank'`; manual check: bag weapon on main panel, R2 fills and levels, L2 refunds without lowering level, evolution “View build-up” ignores R2/L2.

## 5. Party page input (port)

- [x] 5.1 Restrict `CMenuItemInfo::LRCheck` to L1/R1 for party paging under `#ifdef PORT` in `menusys.cpp`.
- [x] 5.2 Document L1/R1 party paging and freed L2/R2 in `docs/PC.md` (item menu section).
