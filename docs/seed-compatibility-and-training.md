# Seed compatibility and training review

The UI names are now **Run seed (direct)** and **Suite seed (calculated)**. They replace Captured spawn seed / Raw spawn and Practice base seed respectively. Full-game setup uses **Use run seed** under **Seed types (advanced)**. This is a display change: recipe IDs, input values, RNG calculations, preference keys, replay metadata and existing PB comparisons keep their meanings.

Copy a layout seed and its type from Seeds & routes. The first RNG value observed by the run auditor is diagnostic evidence: the game can reseed before its spawn selection, so it must not be relabelled as the captured layout seed. The selected seed in a controlled run may use either input type.

## Tests and limits

[`seed_reproducibility_tests.cs`](../tests/seed_reproducibility_tests.cs) executes the production `PracticeRandom.DeriveSeed`, `PortableRandom` and `SpawnSelection` from the supplied mod DLL, plus the source `CatalogOrder` matcher with a minimal vector-arithmetic stand-in. It does not simulate Unity scene loading.

- 320 frozen compatibility vectors check seed derivation and selected spawn/crystal indices.
- 3,328 repeated selections cover both input types, all eight scene IDs with synthetic catalogs, signed boundaries and interleaved independent generator states.
- The same test executable also passes against the retained 7.23.0, 7.24.0 and 7.25.0 helper DLLs. The current helper is 7.26.0. These are Suite versions against the fixture's game/RNG contract, not four game versions.
- Reversing the object catalogs changes physical positions in all 3,328 test cases even when the seed and selected indices are unchanged. Aligning to the captured catalog restores every case. Production only applies that alignment when a matching captured launch is available.
- Timer's first-start override produces a different spawn in 367 of 416 tested Timer cases. Crystal selection and random-draw consumption remain identical. The current code reads the local `HasSpawnedInVaultC` preference.
- A changed catalog, missing point or duplicate point fails matching rather than claiming a correct layout.

These tests establish a stable calculation under the specified inputs. They do **not** establish universal, seed-number-only reproducibility across PCs, local first-start states or future game updates. A real two-installation test still needs the same game build, mode, seed, input type and first-start state, followed by a comparison of the actual player/crystal coordinates.

A future portable seed standard needs a fixed catalog identity/order, explicit Timer spawn policy and a versioned compatibility contract. Existing seeds must retain their legacy interpretation; changing object order or calculation silently would change established layouts. This review does not make that compatibility migration.

## Ten proposed improvements

These are proposals, not implemented training features.

1. **Portable seed preset.** Share mode, seed type, value, generator/map identity and necessary start conditions as one preset, with a clear compatibility result. Start from `SeedMapPlanner`, `CatalogOrder`, `SeedTypeLabels` and existing replay metadata. Universal seed semantics require the migration described above.
2. **Three biggest time losses.** Rank comparable crystal sectors against a chosen reference and jump directly to each replay interval. Existing pickup timestamps, reference matching and replay seek controls provide the data. Missing timing or route mismatches stay explicit.
3. **Repeat a sector.** Turn an existing A/B replay marker range into a playable Practice drill. Start from the matching seed and restore all required gameplay state; teleport alone cannot reproduce a monster encounter. This is more work than replay looping, which already exists.
4. **Lightweight reference ghost.** Display an interpolated reference path/marker alongside the live Practice player. Reuse recorded motion and map data instead of running a second simulation. Full replay playback currently reserves the scene; live practice needs a separate visual-only path.
5. **Corner and movement feedback.** Highlight excess distance, long stops and large speed losses in a chosen section. `MovementTiming` already integrates paths and identifies turns/stops. Report observations; avoid claiming an exact cause or a guaranteed counterfactual time save.
6. **Crystal approach drill.** Show the actual pickup boundary and compare repeated approaches, so runners learn how close they need to pass. Build on observed pickup regions and collision capture. Keep these aids in Practice.
7. **Route memorization stages.** Guide the next crystal, gradually remove hints, then test the route without assistance. Use an explicitly chosen route/reference; the planner's shortest checked route does not predict every monster interaction.
8. **Consistency dashboard.** Show recent sector medians, spread, completion rate and improvement, rather than only the best sector. Keep compact future attempt metadata: the current best-sector book cannot reconstruct a complete distribution of old attempts.
9. **Death-location review.** Record compact future death positions and progress, then show recurring problem areas after the attempt. Reuse the death hook, movement capture and ledger; do not invent causes or require failed recordings in full-game replay folders.
10. **Practice a weak full-game level.** Open a completed campaign sector directly as an individual Practice attempt, carrying that level's saved seed and input type. Reuse FullGame records and the existing individual-level launcher; keep official campaign timing and PBs separate.

Analysis should run between attempts or on existing workers. Live hints belong to the explicitly assisted Practice mode; clean categories keep their current restrictions. I would prioritize the seed compatibility contract, ranked time-loss review and one-click full-game level practice before the more complex stateful sector drill.
