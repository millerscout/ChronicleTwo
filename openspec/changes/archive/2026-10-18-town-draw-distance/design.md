# Design

## Context

Town visibility uses retail `CObject::CheckDraw()` and `CObject::FarClip()` (`ps2/src/object.cpp`): when `far_dist > 0` and camera distance exceeds `far_dist`, the object is culled (or faded when `fade` is set). Villagers get `SetFarDist(800.0f)` in `CScene` setup (`ps2/src/scenevillager.cpp`); map parts can set `far_dist` from map scripts (`mapMAP_FAR_CLIP` in `mapload.cpp`) and model `FAR_CLIP` in `mdslist.cpp`. The PC port already gates town-only features with `PortTownLoopMode`, `PortTownControlMode`, and `IsEditMode()` (see `hub_preview.cpp`, `mainloop.cpp`).

## Goals / Non-Goals

**Goals:**

- Config enum `game.town_draw_distance`: `retail` | `extended`, default `extended`.
- Single multiplier **1.5×** on positive `far_dist` only when extended and town walk is active.
- Central hook in the object draw path so villagers, map parts, and other `CObject` derivatives benefit without per-caller edits.
- Tests for multiplier math and town-context gating; docs in apply phase.

**Non-Goals:**

- Changing fog distances, clip planes, or `mgSetRenderInfo` far clip.
- Scaling `near_dist` or draw-memory / polygon budgets (`GetMaxDrawMem`).
- Per-map or per-NPC tuning; a second multiplier tier (e.g. 2×) can be a follow-up.
- PS2 retail build changes.

## Decisions

### 1. Config surface

`enum class TownDrawDistance { Retail, Extended }` on `Config`, JSON string `game.town_draw_distance`, PC Game row with labels **Retail** / **Extended** (mirror `dungeon_ceremony_qol` string enum pattern in registry).

Default **Extended** so new PC installs match player expectation; **Retail** preserves PS2 parity for comparisons.

### 2. Hook location

Implement `PortTownEffectiveFarDist(float retail_far)` in `port/src/town_draw_distance.hpp/.cpp` and call it from `CObject::CheckDraw` and `CObject::FarClip` when comparing `dist` to `far_dist`.

`#ifdef PORT` in `object.cpp` keeps PS2 tree unchanged. Non-port builds inline identity.

Alternative considered: scale at `SetFarDist` in `scenevillager` only — rejected because map parts and other objects would still pop in.

### 3. Town walk detection

`PortTownDrawDistanceApplies()` returns true when:

- `PortTownDrawDistanceMode() == Extended`, and
- `GetNowLoopNo() == LOOP_EDIT`, and
- `!IsEditMode()` (not Georama build/edit), and
- `PortTownLoopMode == EDIT_LOOP_WALK`, and
- `PortTownControlMode == EDIT_CONTROL_PLAYER`, and
- not in dungeon (`GetNowLoopNo()` / dungeon scene checks — use the same `DngMain` guard pattern as other port QoL if loop alone is insufficient).

Reuse existing externs from `mainloop_port.hpp` / edit loop port symbols already used by hub preview and autosave.

`SCENE_CHARA_NO_DIST` already bypasses distance in `DrawChara` by temporarily setting `-1`; no extra work.

### 4. Multiplier

**1.5×** — villagers 800 → 1200, typical map far clips scale proportionally. Chosen over 2× to limit extra draw load in dense towns (2900–4200 draws/tick per `docs/PC.md`).

### 5. Tests

`town_draw_distance_test.cpp`: effective far helper (retail passthrough, extended 1.5×, non-positive unchanged), gating false in edit mode / retail mode (stub loop flags like other port tests).

## Risks / Trade-offs

- **[More draws in town]** → 1.5× is conservative; player can set Retail; monitor frame time on large Georama saves.
- **[Wrong gating applies in events]** → Require `EDIT_CONTROL_PLAYER` and exclude `event_run` if needed (match hub preview eligibility).
- **[Fade curves feel different]** → `FarClip` uses the same scaled threshold; fade speed unchanged (acceptable).

## Migration Plan

Ship with default Extended; existing `config.json` without the key picks up Extended on load. No save migration.
