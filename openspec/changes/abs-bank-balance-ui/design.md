# Design

## Context

See `proposal.md`. `abs-bank` stores `banked_abs` and exposes `AbsBankEnabled()` / `AbsBankBalance()`. Spend UI lives on `ITEM_MENU_PANEL_WEAPON_STATS` in `abs_bank_menu.cpp`. Quick loadout already draws a port overlay from `PORT_OVERRIDE DrawStatusBord` in `dng_status.cpp` after retail status art.

## Goals / Non-Goals

**Goals:**

- Read-only bank total visible in dungeon (field) and on item-menu weapon main view.
- Reuse existing abs-bank helpers; no new save fields or config keys in v1.
- Match retail menu font / `DrawMenuFillBox` plate style used elsewhere in port QoL.

**Non-Goals:**

- New spend UI or changing stats-panel apply flow.
- Town/georama HUD (dungeon only for v1).
- Floating combat popup on orb pickup (optional later).

## Decisions

### 1. Dungeon placement

**Choice:** Small plate under or beside the status board anchor, drawn from `PortAbsBankDrawDungeonHud(float statusbar_rate)` called at the end of `DrawStatusBord` (after `PortQuickLoadoutDraw`), gated like quick loadout: `LOOP_DUNGEON`, `DNG_STATUS_FIELD`, status bar shown, `AbsBankEnabled()`.

**Alternatives:** Message window on orb pickup (too transient); full-width bar (clutter).

### 2. Show when balance is zero

**Choice:** When banking is On, always show `Banked ABS: 0` so players know the feature is active. Hide entire HUD when banking is Off.

**Alternative:** Hide at zero (less clutter) — rejected for discoverability.

### 3. Item menu placement

**Choice:** Draw in `MenuItemDraw` overlay path (extend `abs_bank_menu.cpp`) when `view_mode` is weapon or fishing rod and `key_arg_no` is `ITEM_MENU_PANEL_WEAPON` or `ITEM_MENU_PANEL_FISHING_ROD` (main weapon page), using the same numeric format as dungeon HUD. Keep existing stats-panel overlay for apply amount.

**Alternative:** New item command “Banked ABS: N” in command list — more messages, deferred.

### 4. Formatting

**Choice:** Integer display via `static_cast<int>(AbsBankBalance())` for v1 (matches current stats overlay). Use message id `5095` (“Banked ABS”) prefix where `message_text` lookup is cheap; otherwise literal prefix for parity with stats panel.

## Risks / Trade-offs

- **[Risk] HUD overlap with quick loadout** → position bank plate on opposite side of status board or below loadout panel; tune constants in one file.
- **[Risk] Drawing when pause/subgame** → reuse quick loadout visibility checks (`SubGameRunning`, eye view, pause flags).
- **[Trade-off] Integer truncation** → document; fractional bank still stored correctly.

## Migration Plan

Ship with no migration; default behavior additive when `game.abs_bank` already On.

## Open Questions

- Whether town dungeon entrance should show HUD (defer v2).
- PAL message lookup vs hardcoded English on HUD (prefer lookup if `GetItemMessage` pattern exists for additions).
