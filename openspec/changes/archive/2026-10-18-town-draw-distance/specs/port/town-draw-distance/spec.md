# port/town-draw-distance

## Purpose

Lets the PC port extend how far town objects stay visible before distance culling, reducing NPC and geometry pop-in without changing dungeon play.

## ADDED Requirements

### Requirement: Persisted town draw distance mode

The port SHALL store the town draw distance mode in `config.json` as `game.town_draw_distance` with values `retail` or `extended`.

#### Scenario: Config round-trip

- **WHEN** `config.json` contains `"game": { "town_draw_distance": "extended" }`
- **THEN** the loaded configuration reports extended town draw distance

#### Scenario: Default on fresh config

- **WHEN** no `game.town_draw_distance` key is present
- **THEN** the port defaults to `extended`

### Requirement: PC SETTINGS control

The PC SETTINGS Game page SHALL expose town draw distance as **Retail** and **Extended**, writing `game.town_draw_distance` when changed.

#### Scenario: Player selects Retail

- **WHEN** the player sets town draw distance to Retail on the Game page
- **THEN** `config.json` contains `"town_draw_distance": "retail"` and object culling uses stock far distances in town

### Requirement: Extended far-distance scaling in town walk

When the mode is `extended` and the player is in normal Georama town walk (town loop, player control, not Georama edit/build mode, not a dungeon), the port SHALL treat each object's positive `far_dist` as multiplied by **1.5** for distance culling and far fade (`CheckDraw` / `FarClip`).

#### Scenario: Villager visible farther away

- **WHEN** extended mode is active, a villager has retail `far_dist` 800, and camera distance is 1000 world units
- **THEN** the villager is considered in range for drawing (effective far limit 1200)

#### Scenario: Retail mode unchanged

- **WHEN** the mode is `retail` and camera distance is 900 with `far_dist` 800
- **THEN** the object is culled as on PS2

#### Scenario: Unlimited far distance unchanged

- **WHEN** extended mode is active and an object's `far_dist` is not positive
- **THEN** culling behavior matches retail for that object

#### Scenario: Near distance unchanged

- **WHEN** extended mode is active and camera distance is inside the object's `near_dist`
- **THEN** the object is culled with the same rules as retail

### Requirement: No effect outside town walk

Extended scaling SHALL NOT apply in dungeon field play, title/menus, scripted events that disable distance (`SCENE_CHARA_NO_DIST`), or Georama part placement/edit modes.

#### Scenario: Dungeon unchanged

- **WHEN** extended mode is active and the player is on a dungeon battle floor
- **THEN** object far-distance culling matches retail
