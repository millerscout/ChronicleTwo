# Proposal

## Why

Weapon EXP banking is implemented, but spending banked ABS only works on the hidden weapon **stats** sub-panel. Players naturally watch ABS on the main **weapon inspect** view (build-up board gauges) and never find spend controls. Moving add/remove to that panel—with familiar shoulder triggers—matches where they already look and closes the gap between the abs-bank spec (“weapon page”) and shipped UX.

## What Changes

- When `game.abs_bank` is On and the item menu is on **`ITEM_MENU_PANEL_WEAPON`** (main weapon inspect for a bag or equipped weapon), the player can:
  - **R2**: transfer ABS from the bank onto the viewed weapon (respecting gauge capacity; auto level-up when thresholds are crossed).
  - **L2**: transfer ABS from the weapon back into the bank (refund), without reducing weapon **level**.
- On-screen hint on the weapon panel (near existing bank readout) documents R2/L2 while banking is enabled.
- Retire or slim down the stats-sub-panel spend overlay (analog + Cross confirm) so one primary spend path remains.
- Optional: trigger retail menu level-up presentation (`MenuLevelUpMan`) when R2 spending causes a level-up.
- On the **port**, item-menu **party page** cycling (Max / Monica / Ridepod / Monster) uses **L1 / R1** only. Retail also maps **L2 / R2** to the same action; the port does not, so L2/R2 stay free for weapon ABS banking and other item-menu uses.

## Capabilities

### New Capabilities

- `port/abs-bank-weapon-panel-spend`: Shoulder-trigger bank transfer on the main weapon inspect panel, refund rules, and level-up behavior.

### Modified Capabilities

- _(none synced to `openspec/specs/` yet; behavior extends the shipped abs-bank feature documented in change `abs-bank`.)_

## Impact

- **Code:** `port/src/abs_bank.cpp` (credit/debit helpers, per-step add/remove, level floor for refunds), `port/src/abs_bank_menu.cpp` (input on `ITEM_MENU_PANEL_WEAPON`, draw hints), `ps2/src/menusys.cpp` (`CMenuItemInfo::LRCheck` under `#ifdef PORT` for L1/R1 party paging only).
- **Tests:** extend `abs_bank_test.cpp` / menu tests for R2/L2 transfer, refund floor, no level-down.
- **Docs:** `docs/PC.md` control summary when banking is on.
- **No** config toggle beyond existing `game.abs_bank`; no save format changes.
