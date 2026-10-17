# Proposal

## Why

Dungeon chest openings, floor-clear fanfare, and related event scripts spend a lot of time on scripted waits, motion playback, and character-by-character message reveal. Retail offers drama skip only when scripts opt in; dungeon ceremonies often do not, and typewriter text cannot be rushed on demand. Chronicle Two tier-1 QoL calls for optional, faithful-by-default acceleration of these beats without changing combat or mandatory story elsewhere.

## What Changes

- Add a **player-facing setting** (default **Off**) for dungeon ceremony QoL with modes **Off**, **Manual**, and **Auto**, exposed in the **in-game options menu** (retail options) and the **PC overlay Game page**, persisted in `config.json`.
- **Manual:** when a qualifying dungeon ceremony event is running, pressing **confirm / advance** instantly completes the current event message’s text reveal; ceremony waits/motions run at up to **2×** speed.
- **Auto:** same ceremony scope and **2×** pacing, but event message text reveal completes **automatically each frame** (no button press); confirm is still used only where retail already requires it for page advance or choices after text is shown.
- Do **not** use full `EventScript.skip()` across item-grant script regions.
- Add **automated tests** for config modes, reveal-skip/auto behavior, and gating.
- Document behavior in port/game docs when implemented (apply phase).

## Capabilities

### New Capabilities

- `dungeon-ceremony-qol`: Optional faster dungeon ceremonies—manual or auto message reveal skip, scoped events, config/menu surfacing, and safe wait/motion acceleration.

### Modified Capabilities

- _(none — no existing OpenSpec main specs yet)_

## Impact

- **Port:** `Config` (enum or string choice), `platform/config.cpp`, `port_settings`, PC overlay + retail options rows (likely one multi-choice row).
- **Game / event:** ceremony guard, `ClsMes::CompleteReveal`, per-frame auto path in `EventLoop` / `EdEventStep`, optional `runscript.cpp` wait scaling.
- **Tests:** config/registry/options + `dungeon_ceremony_qol_test.cpp`.
- **Data:** No `.stb` edits required for v1.
