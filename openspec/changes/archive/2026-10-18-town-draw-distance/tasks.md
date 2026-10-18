# Tasks

## 1. Configuration and menu

- [x] 1.1 Add `TownDrawDistance` to `Config`, `config.cpp` load/save for `game.town_draw_distance` (`retail` / `extended`, default `extended`); extend `platform_config_test.cpp` for round-trip and default
- [x] 1.2 Add PC Game row and `port_settings` descriptor (string enum **Retail** / **Extended**); extend `port_settings_registry_test.cpp` and `pc_options_test.cpp`

## 2. Draw-distance helpers

- [x] 2.1 Add `town_draw_distance.hpp/.cpp` with mode query, `PortTownDrawDistanceApplies()`, and `PortTownEffectiveFarDist(float)` (1.5× when applies); add `town_draw_distance_test.cpp` for multiplier and gating cases
- [x] 2.2 Hook `CObject::CheckDraw` and `CObject::FarClip` in `port/src/object.cpp` to use `PortTownEffectiveFarDist(far_dist)` for comparisons; verify culled-at-900 / visible-at-1000 villager scenario via unit test

## 3. Documentation

- [x] 3.1 Document `game.town_draw_distance` in `docs/PC.md` and `docs/PORT_OPTIONS.md` (behavior, default Extended, performance note)
