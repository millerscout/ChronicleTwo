# Tasks

## 1. Core module

- [x] 1.1 Add `port/src/save_extensions.hpp` and `port/src/save_extensions.cpp` with path helper (`save image path` → `<basename>.extensions.json`), in-memory `mods` map, `Clear` / `Load` / `Save` / `Mod(id)` per design.md; wire into port CMake and verify the port target builds.
- [x] 1.2 Add `port/src/tests/save_extensions_test.cpp` covering valid parse, missing file → empty store, bad `schema_version` → empty store, round-trip save, counter mismatch still loads with mod data, and namespace isolation; verify via `ctest -R SaveExtensions` (or project-equivalent filter).

## 2. Save manager integration

- [x] 2.1 Call `SaveExtensionsLoad` from `CMemoryCardManager::LoadFromMc` after successful `SaveSlotRead` and `memcpy` of `CSaveData`; call `SaveExtensionsClear` on failed load paths; verify with a test that loads a fixture save image and asserts mod JSON is readable when a sidecar is present.
- [x] 2.2 Call `SaveExtensionsSave` after successful manual save publication in `memcard.cpp` (and autosave in `SaveSlotWriteAutosave`) with the written image’s `unique_counter`; skip sidecar write when store is clean and all namespaces empty; verify round-trip by save-then-reload test using temp save root.

## 3. Documentation

- [x] 3.1 Update `docs/PC.md` save-root section with `*.extensions.json` layout, schema_version 1 fields, mod namespace convention, and backup note (binary + sidecar); verify the described paths match `SaveSlotsRoot()` layout.

## Workflow follow-up

- Propose and implement `weapon-exp-bank` as a separate change that stores state under a `chronicle_two.*` mod id using this API.
- Archive this change after review and sync specs when the feature ships.
