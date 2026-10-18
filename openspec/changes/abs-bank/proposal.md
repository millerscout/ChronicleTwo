# Proposal

## Why

Weapon experience orbs currently auto-apply to equipped weapons using retail kill routing, which makes it hard to feed the weapon you are building in the bag or to control split gains. The port now has per-save extension storage (`save-extensions-framework`); players need an optional QoL path that banks orb value and spends it deliberately. This change delivers that behavior without altering the retail save blob.

## What Changes

- Add `game.abs_bank` in PC settings (default **Off**): when On, blue `PULL_ITEM_WEAPON_EXP` pickups credit a float bank instead of calling `AddExpWeaponParam`.
- Persist `banked_abs` under save extension mod id `chronicle_two.abs_bank`.
- Add item-menu UI to view the bank (where practical) and apply partial ABS from the bank to a selected bag weapon on the weapon stats flow, including level-up handling consistent with retail.
- When Off, dungeon pickup behavior remains retail.

**Out of scope:** Ridepod ABS routing changes beyond what retail already does; datapack-only mods without code; changing orb spawn counts or `reward_exp` tables.

## Capabilities

### New Capabilities

- `port/abs-bank`: Config toggle, orb-to-bank collection, extension persistence, and item-menu spend UI for banked weapon ABS.

### Modified Capabilities

- (none — `port/save-extensions` is consumed as infrastructure, not changed)

## Impact

- **Depends on:** `save-extensions-framework` (shipped): `SaveExtensionsMod`, load/save on slot/autosave.
- **Code:** `port/src/dng_pickup.cpp` or `dng_object` pickup hook, new `port/src/abs_bank*.cpp`, `config` / PC settings rows, `message_text` strings, item menu overrides (same pattern as quick loadout), tests.
- **GUI:** No new UI toolkit — extends retail item menu and weapon stats panel via existing port menu override hooks.
- **Docs:** `docs/PC.md` (`game.abs_bank`), optional ROADMAP tier-1 note.
