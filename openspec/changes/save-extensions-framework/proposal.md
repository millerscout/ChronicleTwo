# Proposal

## Why

Chronicle Two keeps all story and inventory state in a fixed-size retail save image (`SAVEDATA_FORMAT`) and explicitly avoids embedding PC-only fields in that blob. Modders and first-party QoL features (for example, banked weapon experience from blue orbs) need **per-save-slot** persistent JSON that does not change checksum layout or break imports from older saves. Today the only precedent is ad hoc sidecars (town autosave spawn JSON); there is no shared contract, API, or documentation for mod-owned save data.

## What Changes

- Introduce a **save extensions** sidecar file per flat save image and for the autosave, keyed by save basename with optional `unique_counter` validation.
- Add an in-memory **extension store** loaded after a successful save read and written when the port publishes a save image (manual slot save, autosave, and any path that calls `SaveSlotWrite` for the active session).
- Expose a small C++ API for first-party code: get/set opaque JSON subtrees by mod namespace id (reverse-DNS style).
- Document the JSON schema and modder rules in port docs (profile toggles stay in `config.json`; per-playthrough state stays in extensions).
- Add unit/integration tests for round-trip, missing sidecar defaults, counter mismatch policy, and coexistence with retail checksum validation.

**Out of scope for this change:** weapon orb banking UI, gameplay routing, datapack discovery manifests, or changes to `sizeof(SAVEDATA_FORMAT)` / `CSaveData`.

## Capabilities

### New Capabilities

- `port/save-extensions`: Per-save-slot JSON sidecars, load/save lifecycle tied to the PC save manager, mod namespace isolation, and compatibility rules for legacy saves and imports.

### Modified Capabilities

- (none)

## Impact

- **Code:** `port/src/save_slots.cpp`, `port/src/memcard.cpp` (load/save hooks), new `port/src/save_extensions.*` (or equivalent), tests under `port/src/tests/`.
- **Docs:** `docs/PC.md` (save root layout, backup instructions alongside spawn sidecars).
- **Compatibility:** Existing `.dkcl` saves load unchanged; missing sidecar means empty mod data. Copying or sharing saves requires both binary and `.extensions.json` when mod state matters.
- **Downstream:** Planned `weapon-exp-bank` (or similar) will store `banked_abs` under a `chronicle_two.*` mod id using this framework.
