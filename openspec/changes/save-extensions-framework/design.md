# Design

## Context

See `proposal.md` for motivation. The port already:

- Writes fixed-size `SAVEDATA_FORMAT` images (`dc2Ver4`) with retail check digits over `CSaveData` (`memcard.cpp` `BuildImage`, `save_slots.cpp` `SaveSlotRead`).
- Uses one ad hoc JSON sidecar for town autosave spawn (`autosave.spawn.<counter>.json`) with `schema_version: 1`.
- Keeps profile settings in `config.json` (not per slot); `quick_loadout.txt` is profile-wide.

Save extensions generalize the sidecar pattern for **mod-owned per-save state** with a stable basename-keyed file and a small in-memory API.

## Goals / Non-Goals

**Goals:**

- Basename-keyed `.extensions.json` per flat save and autosave.
- Load after successful `SaveSlotRead`; persist on successful save publication for the bound session.
- In-memory store: namespace id → JSON object; verbatim round-trip for unknown keys.
- Tests and `docs/PC.md` updates for backup and layout.
- C++ API usable by later `weapon-exp-bank` work.

**Non-Goals:**

- Scripting hooks, datapack manifest discovery, or UI for mods.
- Merging extension data into the retail blob or bumping save version strings.
- Copying sidecars during legacy import (document manual copy only for v1).
- Preserving mod keys inside `config.json` (already works informally; not part of this API).

## Decisions

### 1. Sidecar path: `<basename>.extensions.json`

**Choice:** Same directory as the save image; basename derived from the save file path (e.g. `BESCES-51190dkcl3`, `autosave`).

**Alternatives:**

- Key only by `unique_counter` (like spawn files): awkward when multiple files could share history; harder for modders copying files by name.
- Single `extensions.json` for all slots: breaks multi-slot independence.

**Rationale:** Matches how users and tools already name flat saves; optional `unique_counter` field detects stale sidecars.

### 2. Session binding

**Choice:** On `LoadFromMc`, bind the extension store to `SaveSlots.saves[row].path` (and remember basename + counter from the loaded image). On save, write sidecar for that bound path unless the session explicitly rebinds (new game / load other slot clears binding).

**Autosave:** `SaveSlotWriteAutosave` writes `autosave.extensions.json` when autosave runs, using in-memory mod state for the **currently bound** playthrough (same store as the slot loaded from, not a forked copy). If the player never loaded a slot and only uses autosave from title, bind to `autosave` on first autosave write.

**Alternatives:** Always mirror the last manual slot only — fails for autosave-only resume paths.

### 3. Write ordering

**Choice:** Write extensions JSON with atomic replace (same `FilesWrite` / temp-rename pattern as `config.json` where practical), ideally immediately before or after the binary save write in the same code path; if binary write fails, do not update sidecar (avoid orphan metadata ahead of a failed image).

**Spawn precedent:** Spawn metadata is written *before* autosave image; for extensions, **co-write with successful image** is simpler and avoids bank loss when image write fails.

### 4. API shape (C++)

**Choice:** Minimal module, e.g. `save_extensions.hpp`:

- `void SaveExtensionsClear()` — on failed load / new game prep.
- `bool SaveExtensionsLoad(const std::filesystem::path &save_image_path, u64 unique_counter)` — parse sidecar or empty.
- `bool SaveExtensionsSave(const std::filesystem::path &save_image_path, u64 unique_counter)` — serialize store.
- `nlohmann::json *SaveExtensionsMod(const std::string &mod_id)` — get-or-create object for namespace; nullptr if store inactive.
- `const nlohmann::json &SaveExtensionsMods()` — read-only view for tests.

Use existing `nlohmann::json` (already in save spawn code). No retail symbols required.

### 5. Counter mismatch

**Choice:** Load mod data anyway; `stderr` diagnostic once per load (include both counters and basename). No silent discard in v1.

**Alternative:** Strict mode rejecting sidecar — worse UX for manual file surgery.

### 6. Hook points

| Event | Action |
|--------|--------|
| `CMemoryCardManager::LoadFromMc` success | `SaveExtensionsLoad(slot.path, image.unique_counter)` |
| `BuildImage` + `SaveSlotWrite` success (manual) | `SaveExtensionsSave(path, image.unique_counter)` |
| `SaveSlotWriteAutosave` success | `SaveExtensionsSave(autosave path, counter)` |
| Failed load / before overwriting live `CSaveData` from bad read | `SaveExtensionsClear()` |

`SaveSlotWrite` in `save_slots.cpp` is the single binary publisher; memcard paths call it — extension save should be invoked from memcard after successful write (has `unique_counter` in image) to avoid duplicating in every caller.

### 7. Documentation

Add a subsection under PC save layout: `*.extensions.json`, schema sketch, mod namespace convention (`reverse.dns.id`), distinction from `config.json` and spawn files.

## Risks / Trade-offs

- **[Risk] User copies only `.dkcl` file** → mod state missing. **Mitigation:** Document; optional future: copy sidecar on duplicate-save UI.
- **[Risk] Autosave overwrites extensions while testing another slot in same session** → mitigated by clear binding rules and only one active `CSaveData` at a time.
- **[Risk] Large JSON in sidecar** → unbounded mod abuse. **Mitigation:** v1 no cap; log parse failure if file huge (optional size limit in loader, e.g. 1 MiB).
- **[Trade-off] No cross-slot merge** → intentional; each basename owns its mods map.

## Migration Plan

- Ship behind no toggle: absent sidecar is zero cost.
- No changes to existing save files on disk until first save after upgrade writes a sidecar (only if mod data non-empty **or** we choose to always write `{}` — **decision: write sidecar only when store is dirty or any mod namespace non-empty** to avoid cluttering every save).

## Open Questions

- Whether first-party features should write an empty sidecar on every save for visibility (deferred; default non-empty only).
