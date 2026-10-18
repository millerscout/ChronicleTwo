# Design

## Context

See `proposal.md`. Shipped banking: `AbsBankCredit` / `AbsBankDebit` / `AbsBankApplyToWeapon` in `abs_bank.cpp`; menu hooks in `abs_bank_menu.cpp` target **`ITEM_MENU_PANEL_WEAPON_STATS`** with analog amount + Cross confirm. Main weapon view is **`ITEM_MENU_PANEL_WEAPON`** (`view_form[2]`, `MenuPosFormValueSetWeapon`); Cross still opens the command list. Balance readout already draws on the item menu via `PortAbsBankDrawMenuBalance` / `PortAbsBankDrawItemMenuOverlays` from `menusys.cpp` `MenuItemDraw`.

## Goals / Non-Goals

**Goals:**

- R2/L2 add/remove ABS on **(A)** main weapon inspect panel only.
- Auto level-up on add (existing `LevelUp` loop); no level-down on L2 refund.
- Reuse save extension bank; mark extensions dirty on transfer.
- Hint text on weapon panel; update PC.md.

**Non-Goals:**

- ABS editing on evolution **`MENU_ITEM_EXTEND_BUILD_UP`** screen.
- New config keys beyond `game.abs_bank`.
- ui-hub preview / `ui-menu-framework` widgets.
- Changing orb pickup or sidecar schema.
- Changing Character menu or other non–item-menu shoulder behavior.

## Decisions

### 1. Input hook

**Choice:** Handle R2/L2 in `CMenuItemInfo::KeyStepLocal` (already overridden for stats analog) when `key_arg_no == ITEM_MENU_PANEL_WEAPON`, `mode == MENU_ASK_MODE_NONE`, and not in extend ask modes (`ask_para.ask_mode == MENU_ITEM_EXTEND_NONE`).

Read triggers via `GamePad__2.On(PAD_R2)` / `On(PAD_L2)` (or `Down` for single-step—prefer **On** while held with a per-frame step cap to match “hold to fill” feel).

**Alternative:** Item commands—rejected; user chose shoulder buttons on the weapon panel.

**Alternative:** Steal Cross/Triangle—rejected; retail uses those for command list.

### 2. Step size

**Choice:** Fixed ABS step per frame while R2/L2 held (e.g. 1–5 ABS/frame, tune in implementation), clamped by bank balance, gauge room, and refund floor. Optional later: scale step with analog R stick when not conflicting with menu cursor.

**Alternative:** Reuse pending `AbsBankApplyAmount` + confirm—rejected for this panel.

### 3. Add path (R2)

Call small helper `AbsBankTransferToWeapon(weapon, amount)`:

- `use = min(amount, bank, abs.max - abs.now)`
- `AbsBankDebit(use)`; `weapon.abs.now += use`
- Loop `LevelUp()` while `IsLevelUp()` (cap iterations)
- If level-up occurred, call `MenuLevelUpMan` path used after item-use level effects (mirror `menusys.cpp` level_effect block where feasible)

### 4. Refund path (L2)

Call `AbsBankTransferFromWeapon(weapon, amount)`:

- Compute `floor = AbsBankWeaponRefundFloor(weapon)` — minimum `abs.now` for current level without decreasing level (implementation: inspect retail `IsLevelUp` / level data or derive from weapon level and `CDataWeapon` level-up exp table; spike in apply if needed).
- `refund = min(amount, abs.now - floor)`
- `abs.now -= refund`; `AbsBankCredit(refund)`
- Never call `LevelUp` inverse or mutate `weapon.level` downward.

### 5. Retire stats-panel spend

**Choice:** Remove stats-sub-panel analog overlay and Cross apply (`WeaponStatsBankingActive`, `HandleWeaponStatsInput`, `DrawStatsSpendOverlay`) so R2/L2 on the main panel is the only spend UX. Keep stats panel for retail status editing in debug only.

**Alternative:** Keep both—rejected to avoid duplicate/confusing paths.

### 6. Drawing

Extend `PortAbsBankDrawItemMenuOverlays()` (or weapon-panel-only draw) to show hint when `key_arg_no == ITEM_MENU_PANEL_WEAPON`:

- Example: `Bank: N  R2 +  L2 -` (localized via `message_text` ids if needed).

### 7. Retail bridges

No new `unit_copy` bridges unless a new `PORT_OVERRIDE` is required; extend existing `abs_bank_menu.cpp` + `KeyStepLocal` only.

### 8. Item menu party pages (port input)

**Choice:** In `CMenuItemInfo::LRCheck`, only `MENU_SELECT_KEY_L1` / `MENU_SELECT_KEY_R1` change party inventory pages when built with `PORT`. Remove L2/R2 from the direction logic and from the `switch` cases that perform the page change. Non-`PORT` builds keep retail pairing (L1≈L2, R1≈R2).

**Scope:** Always on the port, not gated on `game.abs_bank`.

**Alternative:** Restrict party paging to L1/R1 only when banking is on — rejected (two behaviors, still confusing when banking is off).

## Risks / Trade-offs

- **[Risk] Refund floor wrong** → under-refund or accidental level-down feel. Mitigation: unit tests with crafted weapon at level 2+; compare to retail gauge after manual level-up.
- **[Risk] R2/L2 conflict with item-menu party cycle** → retail `LRCheck` treated L2/R2 like L1/R1; mitigated by decision §8 (port L1/R1 only for party pages).
- **[Risk] R2/L2 conflict with debug menu** when `menu_debug_flag` → debug already uses shoulders on some panels; gate bank handlers when `AbsBankEnabled()` and non-debug weapon panel, or run bank transfer only when debug isn't consuming L2/R2 on that panel.
- **[Risk] Held R2 drains bank instantly** → use modest per-frame step and optional SE throttle.

## Migration Plan

- Default behavior change only when `game.abs_bank` On; Off unchanged.
- No save migration.

## Open Questions

- Exact per-frame ABS step and whether to add menu SE on each tick or only on level-up.
