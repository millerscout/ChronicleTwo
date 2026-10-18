# Proposal

## Why

On the PC port, towns are often played at wider aspect ratios and higher resolutions than retail PS2. Retail per-object far draw distances (for example villagers at 800 world units and map parts with script `MAP_FAR_CLIP` values) were tuned for a 4:3 view and show obvious pop-in when NPCs and geometry enter view late. Players notice this most while walking in Georama towns.

## What Changes

- Add `game.town_draw_distance` on the PC SETTINGS Game page with **Retail** and **Extended** (default **Extended**), persisted in `config.json`.
- When **Extended** is active and the game is in normal town walk play (Georama map, player control, not dungeon, not Georama edit/build mode):
  - Scale positive `far_dist` thresholds used by `CObject::CheckDraw` and `CObject::FarClip` by a fixed port multiplier (see design).
  - Leave `near_dist`, unlimited draws (`far_dist <= 0`), and `SCENE_CHARA_NO_DIST` event draws unchanged.
- When **Retail**, behavior matches stock PS2 distances.
- Unit tests for config, town-context gating, and effective far-distance scaling.
- Document the setting in `docs/PC.md` and `docs/PORT_OPTIONS.md` (apply phase).

## Capabilities

### New Capabilities

- `port/town-draw-distance`: PC config, town-only far-distance scaling for object culling.

### Modified Capabilities

- _(none — no main specs yet)_

## Impact

- **Port:** `town_draw_distance.hpp/.cpp`, hooks in `port/src/object.cpp` (or shared `CObject` draw path), `platform/config`, options registry/rows, tests.
- **Gameplay:** Slightly more geometry and characters drawn in town when Extended; dungeons, menus, events, and Georama build mode unchanged.
- **Performance:** Modest extra draw load in large towns when Extended; multiplier chosen to stay within retail draw-memory budgets on typical hardware.
