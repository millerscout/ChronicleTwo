# Tasks

## 1. Configuration and menu surfacing

- [x] 1.1 Add `ConfigDungeonCeremonyQoL` enum to `Config`, `game.dungeon_ceremony_qol` off/manual/auto in `config.cpp`, default off; extend `platform_config_test.cpp`
- [x] 1.2 Register `port_settings` descriptor (PcOverlay + Retail), PC Game **Choice** row with Off/Manual/Auto; extend `port_settings_registry_test.cpp` and `pc_options_test.cpp`
- [x] 1.3 Wire retail options three-way row and verify each mode persists through in-game options

## 2. Ceremony gating helper

- [x] 2.1 Add `dungeon_ceremony_qol.hpp` with allowlist, current event tracking, `PortDungeonCeremonyQoLMode()`, `PortIsDungeonCeremonyQoLActive()` (mode ≠ Off)
- [x] 2.2 Add `dungeon_ceremony_qol_test.cpp` for Off/Manual/Auto gating and wrong loop/event

## 3. Message reveal: manual and auto

- [x] 3.1 Implement `ClsMes::CompleteReveal()` and unit test (`reveal_num == char_num` after call)
- [x] 3.2 **Manual:** on advance input when mode Manual + ceremony active, `CompleteReveal` on mid-reveal event messages; test handler gating
- [x] 3.3 **Auto:** each event frame when mode Auto + ceremony active, `CompleteReveal` on mid-reveal event messages without pad; unit test auto path

## 4. 2× ceremony pacing

- [x] 4.1 Double-consume `RS_OP_WAIT` when mode Manual or Auto and ceremony guard true; test or harness
- [x] 4.2 2× event object motion step during ceremony; smoke-test chest/clear with Manual vs Auto

## 5. Integration and docs

- [x] 5.1 Run port unit tests for this feature and fix failures
- [x] 5.2 Document Off/Manual/Auto in `docs/ROADMAP.md` or `docs/PC.md` (manual = tap confirm for text, auto = instant ceremony text)

## Workflow follow-up

- Archive after in-game validation of chest + floor clear in Manual and Auto.
- Optional: auto `page_wait` advance if playtest shows stalls.
