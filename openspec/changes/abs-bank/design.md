# Design

## Context

See `proposal.md`. Save extensions (`chronicle_two.abs_bank`) are implemented. Retail applies orbs in `CPullItem::Step` via `AddExpWeaponParam` (`ps2/src/dng_object.cpp`). Item ABS items use `ITEM_USE_FLAG_FILL_ABS` for a full gauge only (`menucls1.cpp`). Weapon ABS editing for debug uses analog input on `ITEM_MENU_PANEL_WEAPON` in `menusys.cpp` (L1/R1 + stick) — not player-facing.

## Goals / Non-Goals

**Goals:**

- Toggle-gated bank on orb collect; retail when off.
- `banked_abs` float in extensions; dirty store marks save on change.
- Spend UI integrated into existing item menu / weapon stats flow.
- Tests for bank math, toggle routing, extension round-trip.

**Non-Goals:**

- A reusable mod GUI framework or new overlay toolkit.
- BepInEx-style plugins for banking.
- Changing monster `reward_exp` or orb counts.

## GUI: is there a framework?

**No separate GUI framework.** Chronicle Two port QoL that touches menus follows a repeated pattern:

| Piece | Role |
|--------|------|
| **Retail `menusys` / `CBaseMenuClass`** | Layout, panels, commands, drawing |
| **PORT_OVERRIDE hooks** | `GetItemCommandMsg`, `MenuItemCommandSelect`, panel input (`quick_loadout_menu.cpp` is the reference) |
| **`message_text.cpp`** | New command strings (`ITEM_MENU_COMMAND_MSG_BASE + id`) |
| **PC SETTINGS (`options/rows.cpp`)** | Toggle and labels |
| **Port-only drawing** | Quick loadout panel, 3D item icons — bespoke, not a shared library |

**abs-bank UI** should use the same approach: one or two overrides in a dedicated `abs_bank_menu.cpp` (or extend weapon stats handling in a port unit), plus messages — not a new widget system.

## Decisions

### 1. Mod extension shape

```json
"mods": {
  "chronicle_two.abs_bank": {
    "banked_abs": 0.0
  }
}
```

Access via `SaveExtensionsMod("chronicle_two.abs_bank")`; helper `AbsBankBalance()` / `AbsBankCredit(float)` / `AbsBankDebit(float)` in `abs_bank.hpp` (no JSON in headers included from `dng_object`).

### 2. Pickup interception

**Choice:** Port override or thin hook in `port/src/dng_pickup.cpp` if collect logic is duplicated there; otherwise `PORT_OVERRIDE` wrapper calling retail collect then banking, or branch before `AddExpWeaponParam` in a port copy of the collect branch.

**Verify at apply:** single choke point in `CPullItem::Step` for `PULL_ITEM_WEAPON_EXP`.

When banking: `AbsBankCredit(exp)`; play collect SE; free pickup — do not call `AddExpWeaponParam`.

### 3. Spend UX (v1)

**Choice:** Weapon **stats panel** (`ITEM_MENU_PANEL_WEAPON_STATS` / weapon page) when banking is on:

- Show bank total (text near ABS gauge, reuse EXP gauge box art where quick loadout does).
- Left/Right or analog (reuse `MenuCommonInfo->CheckAnalogKey` pattern from debug ABS edit) sets **apply amount** capped by `min(bank, weapon.abs.max - weapon.abs.now)`.
- Confirm (Cross / retail decide) applies: `weapon.abs.now += amount`, `AbsBankDebit(amount)`, `LevelUpWeapon` if needed (call same path as `CGameDataUsed` / userdata helpers used in battle ABS).

**Alternative considered:** New item command “Apply experience” (`ITEM_MENU_COMMAND` id 91+, message in `message_text.cpp`) opening a sub-menu — more retail-faithful command list, more message work. Defer unless stats-panel flow is too hidden.

**Alternative rejected:** Fake bag item for bank — inventing `CGameDataUsed` rows risks save edge cases.

### 4. Config

`game.abs_bank`: `off` | `on` in `config.json`, PC Settings Game page, default `off`. Mirror `dungeon_ceremony_qol` registration in `config.cpp` / `options/registry.cpp`.

### 5. Ridepod / Monica transform

Bank spend applies only to `USED_ITEM_TYPE_WEAPON` on weapon page in v1. Orbs while on ridepod still bank when toggle on; retail ridepod direct ABS when off unchanged.

### 6. Autosave / session

Bank lives in extension store; credit/debit sets dirty. No separate profile file.

## Risks / Trade-offs

- **[Risk] Including JSON headers from pickup code** → keep `abs_bank.hpp` free of nlohmann; only `.cpp` touches extensions.
- **[Risk] Menu override conflicts with mouse item tests** → extend `menu_mouse_item_test` or add `abs_bank_test` for command visibility.
- **[Trade-off] Stats panel UX is less discoverable than a bag item** → document in PC.md; optional v2 command.

## Migration Plan

- Default off; no sidecar field until first orb banked or first spend.
- Existing saves: `banked_abs` absent treated as 0.

## Open Questions

- Whether v1 should add a dedicated **Apply experience** command on weapons for discoverability (can ship stats panel first per tasks).
