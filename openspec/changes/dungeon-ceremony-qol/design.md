# Design

## Context

Dungeon ceremonies use `DNG_STATUS_EVENT`, `CRunScript`, and `ClsMes` typewriter reveal (`draw_speed` / `reveal` / `reveal_num`). Retail `EventScript.skip()` is unsafe for item grants. See `proposal.md` for motivation.

## Goals / Non-Goals

**Goals:**

- Three modes: **Off**, **Manual** (confirm finishes text + 2×), **Auto** (per-frame text complete + 2×).
- Retail options + PC Game overlay, persisted `game.dungeon_ceremony_qol` (string enum).
- Tests for modes, gating, `CompleteReveal`, auto path.

**Non-Goals:**

- Auto-selecting yes/no message choices or auto-skipping entire scripts to `END`.
- Menu clock exclusion, town map chests, `.stb` edits, combat speed changes.

## Decisions

### 1. Config shape (revised)

- `enum class ConfigDungeonCeremonyQoL { Off, Manual, Auto }` on `Config`, JSON key `game.dungeon_ceremony_qol` with values `"off"` | `"manual"` | `"auto"` (default `"off"`).
- One **Choice** row on PC Game page; one retail options row with three labels (mirror `georama_poly_multiplier` or bool+sub-choice pattern if retail rows are tight).
- Help text: Manual = faster ceremonies + tap confirm to finish text; Auto = text appears instantly during ceremonies.

Previously considered a single bool—replaced by enum to include auto-skip without a second toggle.

### 2. Ceremony detection

Unchanged: allowlist 1100, 1500, geostone, floor-start; `PortIsDungeonCeremonyQoLActive()` requires dungeon + event mode + mode ≠ Off + allowlist.

Add `PortDungeonCeremonyQoLMode()` for Manual vs Auto branches.

### 3. Message reveal: shared `CompleteReveal()`

`ClsMes::CompleteReveal()` sets `reveal` / `reveal_num` to `char_num`, clears char `wait`, consistent with a finished page for `_CHECK_MES_WAIT`.

**Manual:** on `PadCtrl` advance edge when mid-reveal → `CompleteReveal` on `GetEventMessage(0/1)`.

**Auto:** each event frame when mode is Auto and ceremony active → if any event message has `draw_speed > 0` and `reveal_num < char_num`, call `CompleteReveal` (no pad input).

Do **not** auto-call `GoNextPage()` or synthesize confirm unless a follow-up task proves a script stalls without it; v1 auto is **text only**.

### 4. Ceremony time acceleration (2×)

When mode is Manual or Auto and ceremony guard true: 2× `RS_OP_WAIT` and 2× event motion step (same as before). Off disables both.

### 5. Tests

- Config parse/serialize all three modes.
- `CompleteReveal` unit test.
- Gating: Off/Manual/Auto × ceremony active/inactive.
- Auto: after one `PortCeremonyQoLStepMessages()` (or equivalent), `reveal_num >= char_num` without pad.

## Risks / Trade-offs

- **[Risk] Auto mode feels too fast to read** → Mitigation: Auto is opt-in; Manual remains default when enabling QoL if we use Off as global default (not “default Manual”).
- **[Risk] Script waits on partial reveal** → Mitigation: CompleteReveal matches manual skip outcome.
- **[Risk] Auto + voice** → Mitigation: same as manual complete; voice may cut short (retail SkipEvent clears stream—acceptable for QoL).

## Migration Plan

- Missing JSON field → Off. Unknown string → Off.

## Open Questions

- Floor-start in allowlist (unchanged).
- If playtest shows ceremonies stall after auto text, add bounded auto-advance for `page_wait` only (separate task).
