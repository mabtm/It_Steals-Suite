# Development

The Suite's own code is AI-generated. I direct development, reverse-engineer the game, and test it in game. The aim is straightforward: useful tools, readable code, and changes we can check. Third-party code keeps its original license.

## Code map

| Location | Owns |
| --- | --- |
| **Mod/** | Unity hooks, scene capture, recording, game controls, and 3D replay rendering |
| **Trainer/** | Native C++20/ImGui overlay, seed search, routing, replay tools, Inspector, and exports |
| **Installer/** | Setup, verified file installation, updater, rollback, and Windows packages |
| **tests/** | Logic tests, UI interactions, platform contracts, and fixed compatibility data |
| **assets/** | Editable branding and the small set of images used on GitHub or in Setup |
| **Encoder/** | Video encoder source archives, build script, and licenses |
| **bin/** | Supplied trainer, compatible game mod, and encoder |
| **protocol-layout.json** | Main shared-memory layout; generates the C++ and C# definitions |
| **repository.py** | README version spotlight, documentation checks, and release-page text |

The game mod talks to the trainer through bounded shared-memory channels. Unity calls stay on the game thread. Native workers use captured data and cached geometry for search and analysis.

Follow the owning component rather than adding a new layer for every change. Keep separate models, file formats, algorithms, and platform code where that separation makes them easier to understand or test.

**ReplayArchive.h** queues named disk operations on one worker; **ReplayPackage.h** owns package formats and exporter compatibility. **ReplayLibraryModel.h** caches the immutable archive's searchable labels and sorted/filter indices without reordering the archive itself. **TrainerHotkeys.h** owns the eight native shortcut bindings and capture/conflict rules. **TrainerUI.h** groups menu pages and settings, while **ClipUI.h** groups camera, layer, keypoint, and export controls. These sections share the existing state and UI identifiers.

**PerformanceTiming.h** contains frame-cost and search-progress measurements. **MovementTiming.h** contains recorded movement analysis and travel-time estimates. Input colors live with their drawing helpers in **InputModules.h**.

Map format 15 adds explicit run-category and seed-recipe evidence; format 14 added walkable collision-floor triangles alongside the existing NavMesh and obstacle capture. Input format 8 adds bounded snapshots for game UI, extra cameras and particles. All recording channels share the 128 MiB input-file budget; omitted observations remain visible in Recording check. Audio format 1 uses checksummed reserved header bytes for PCM gap timestamps and queue diagnostics. Both readers still accept previous formats; their original limits remain in place.

<details>
<summary><strong>Where to find the main systems</strong></summary>

| System | Start here |
| --- | --- |
| Main UI and settings | TrainerUI.h, SuiteDesign.h, TrainerSettings.h |
| Seed search and routing | SeedSearch.h, PracticeRouting.h, PhysicalNavigation.h, PickupRouting.h |
| Replay files and playback | ReplayArchive.h, ReplayPackage.h, Mod/Replay3D.cs |
| Clip editing and export | ClipUI.h, ClipRuntime.h, ClipEncoding.h, Mod/ReplayClipChannel.cs |
| Live Inspector | InspectorUI.h, InspectorModel.h, Mod/LiveInspector.cs |
| Updates | SuiteUpdateModel.h, SuiteUpdater.h, SuiteUpdateUI.h, Installer/Update.ps1 |
| Protocol | generate-layout.py, Mod/SharedLayout.cs, Trainer/SharedProtocol.h |

</details>

## Build

On Windows, install Visual Studio C++ Build Tools with a Windows SDK, .NET Framework 4.7.2 targeting support, and MelonLoader in your game installation:

```powershell
.\build.ps1 -GameDir 'C:\Games\It Steals'
```

The script finds MSBuild and the loader/Harmony references, then builds both projects into **bin/**. Use `-PlatformToolset v143` if needed; the project defaults to v145. Explicit `-MelonAssemblyPath` and `-HarmonyAssemblyPath` overrides are also supported.

Game, Unity, MelonLoader, and Harmony assemblies are local build inputs. Keep them out of Git. Rebuild the encoder on Linux/WSL with **Encoder/build-encoder.sh**; update its identity and keep its source and licenses with any redistributed binary. See [Encoder/NOTICE.md](Encoder/NOTICE.md).

## Check a change

### Stable speedrun release: 7.25.0

Stable publication requires `SUITE_PREVIEW 0` on `main`; development branches still publish only pre-releases. The workflow builds and tests Windows packages before publishing the complete `v7.25.0` asset set and marking it latest. Installed previews can adopt the stable build with the same numeric version.

`Mod/RunSplitFile.cs` reads only bounded v15 Set-Seed metadata and verifies the entire payload checksum with a reused 64 KiB buffer. It seeks past geometry, navigation, traces and monster tracks instead of creating those managed objects during a split scan. The independent full archive decoder still handles playback and export. Background startup warm-up yields to runs; file signatures and immutable layout identities are cached. An unchanged recording is observed once per sector-book generation, while transient file-sharing failures retry. No run-start, pickup or rendering path performs archive IO.

`SplitDeltaUI.h` is the shared live/clip text renderer. It emits glyphs only, draws the latest pickup and uses an explicit animation age; Clip Studio derives that age from source time for deterministic seeking and export. Clip project 9 stores the appearance; versions 1–8 remain readable and retain their panel appearance. Composition snapshots freeze the reference, split rows, font and appearance for exports. Old recordings do not embed historical sector bests, so a copied best-sector metric uses the recorded reference-sector delta in clips.

`ReplayReferenceUI.h` caches matching archive row labels and search indices. Selection queues a bounded load on the archive worker without opening or seeking the active recording; full layout validation remains authoritative. Manual loading/errors are separate from automatic PB lookup, stale auto requests are generation-checked, and choosing Automatic clears a manual override. Reference rows require observed pickup timing; export waits for a pending selection.

The native golden fixture is decoded independently by the full native reader and lightweight managed reader. A synthetic 360866-byte recording with 30000 floor vertices allocates about 2.6 KiB per split read, compared with at least the entire file size for the previous byte-array read alone. This measures allocation, not game FPS. UI tests compare exact live/clip glyph vertices, reverse seeks, fade boundaries and frozen style; 10000 warmed live text frames perform zero ImGui heap allocations. Tests also exercise startup backfill, repeated resets, direct reference changes and incompatible/unavailable choices. The reported arm export correction was confirmed by the user in game and its Unity render path is unchanged in this release.

### Previous preview development: 7.22–7.24

The development branch publishes tags of the form `v7.23.0-preview.RUN_NUMBER`, with GitHub's prerelease flag and latest=false. `SUITE_PREVIEW` also forces preview publication if this source is built from another ref. Stable update metadata rejects both prerelease rows and non-stable tag syntax. Promotion to a stable release is a separate change; an installed preview may accept the same numeric version once it is actually stable. File names, registry identities, IPC names, installation discovery, replay paths and credential bindings retain their existing identities.

Timer's 59/1932 pause is the transition from source corridors to repair/refinement. The baked Timer navigation can separate duplicate bunker starts from the remaining catalog even after the movable door is omitted. Preparation now reuses checked component bridges and exact duplicate projection queries; physical repair expands the reachable frontier instead of unrelated unreachable corner pairs. Closure joins only real checked polylines and tests its shortcuts. Synthetic tests cover the full 59-node/1932-task preparation, one east bunker exit, closed-door rejection, cancellation/retry and preparation through CPU scoring. The supplied scene's baked/captured navigation was also reconstructed outside Git for end-to-end checks; this is not a live-game FPS measurement or proof of an unknown tester capture. Diagnostics can now save the exact active stage-2 packet and metadata without filesystem work on the render thread.

Set-Seed is selected with Speedrun=1, Practice=0 and SeededPractice=1; the complete RNG hooks remain required and all other aids stay restricted. Category, seed and recipe are latched before the run and changes remain invalid even if reverted. Stats format 9 stores separate Set-Seed PBs/rows and makes a permanent exact pre-upgrade backup before replacing older data. Map/review format 15 records the declaration, selected/observed seed and recipe; old records never inherit the new validity exception. Three readonly fields occupy previous padding at 5618, 5619 and 5620, preserving payload size and all existing offsets. The history validity/category markers are separated from warning bits. Canonical replay manifests retain the old representation for historical exporters and add explicit category fields only from 7.22 onward.

The native ImGui shell uses compact top tabs, bounded time-based transitions, explicit start actions and separate draft/category state. At narrow widths the same eight destinations wrap above the workspace; no navigation is hidden. Training uses normal checkboxes and native section separators, and Run keeps its mode list beside category/seed setup where space allows. Replays retain their canvas space and tiny windows keep the main actions visible. Font discovery runs on a bounded worker; filtered font indices are cached and visible rows clipped. Atlas rebuilding after a deliberate font/DPI change still happens between render frames. Menu scheduling targets 60 FPS, passive output 30 FPS, and hidden output does no rendering. Diagnostics report measured successful output cadence rather than claiming monitor refresh. The animation primitive benchmark and headless whole-UI benchmark exclude game/GPU/presentation work.

The 7.23 run journal (`UserData/ItStealsPracticeSuite/run-ledger-v1.tsv`) is append-only, with FNV64 consistency checks, monotonic identities, session markers and exact backups of incomplete tails before truncation. It supplements the original v9 counters; it does not recreate lost historical attempts. Metadata uses a separate bounded queue on the existing IO worker. Ranked recordings bypass retention thresholds and take priority over ordinary queued reviews, with a `.keep` companion and explicit overload warnings. The trainer polls and incrementally indexes on a worker, publishes immutable query results, and clips visible rows. Its safety limit is one million indexed records / 256 MiB of source; exceeding it preserves the source and reports the limit. Counter reads verify the original SHA256 and allow atomic replacement on Windows. Shared C#/C++ fixtures exercise both formats.

Extra replay cameras interpolate cached poses at the same absolute time as world/light samples. Stationary camera endpoints are retained for new recordings; old capture gaps cannot be reconstructed. Capture CPU profiles use bounded 128-sample percentile windows and full-run means/maxima, without machine-specific recording settings. `.capture.json` diagnostics accompany recordings and are optional. GPU/presentation time is not measured. Split references require the same scene, category, layout, recipe and pickup prefix; loading references uses the replay worker and does not replace the active replay. LiveSplit is deliberately outside this update.

### Community fixes: 7.21.0

Timer geometry capture excludes only the actual `VaultButton.VaultDoorCollider` reference in scene 7. The game disables that collider and its navigation obstacle when the vault opens; fixed bunker walls and frames still participate in physical clearance checks. Physical detour repair now evaluates visibility edges on demand with A*, and evaluates remaining edges after an unsuccessful round before expanding the blocker set. Regression comparisons retain the previous exhaustive implementation as an independent reference. Static geometry checks do not verify live door timing or enemy interactions.

Clip export allocates a temporary scene target at export resolution, binds the full camera chain before rendering, remaps the scene's display images and refreshes Canvas layout. The optional installed post-processing layer resets temporal history for each sample and on return to playback. Game-owned targets, image references, camera aspect and active render target are restored. Camera-contract tests cover mixed resolutions, equal-depth composition and failure cleanup; actual rendered output still needs a Windows game check.

Quick restart is the ninth trainer action, default T. Trainer ownership uses a spare telemetry flag and is advertised only with a compatible mod at least 7.21.0; the mod then suppresses its standalone T fallback. Earlier trainer/mod combinations retain the old fallback. The command layout is unchanged. Font selection retains legacy defaults while storing stable identities for discovered Windows system/user fonts and the local Fonts directory. Only supported outline font faces are listed; unsupported containers and unavailable saved faces use the defaults.

The installer verifies the official pinned archive before repairing an incomplete installation, rejects modified distribution files and conflicting path types, and journals only missing files. Regression coverage uses the real pinned distribution as well as rollback fixtures. Public-repository Windows CI exercises anonymous update listing/download and a revoked saved credential; private CI retains authenticated coverage. Changing repository visibility remains a repository-owner operation.

Python 3 and a C++20 compiler run the native tests. `--ui` adds real ImGui interaction tests; `--managed` adds C# contract programs and requires Mono's `mcs` and `mono`.

```sh
python3 generate-layout.py --check
python3 tests/run.py --ui --managed --jobs 2
python3 repository.py check
```

Use `--only TEST_NAME`, `--output DIRECTORY`, or `--compiler COMPILER` for a focused run. Results go into ignored **build/** directories. Assertions stay enabled. For route changes, also run:

```sh
python3 tests/run.py --sanitize --only physical_edge_tests physical_route_tests routing_reliability_tests --output build/sanitized
```

Tests use fixed captures and synthetic fixtures. Some fixtures contain non-English Unicode text to check file handling. Portable checks cover logic and contracts; live Unity behavior, interactive UAC, device input, and actual performance still need testing in game. The supplied mod is not rebuilt by contract tests; compile it separately against the matching game SDK and loader. The managed statistics storage test executes the supplied DLL and verifies its version, migration and persistence. It downloads and checksums the same official loader pinned by Setup; no proprietary game assemblies are required for that test.

### Mode dashboard and statistics: 7.19.0

**Mod/ModeStatistics.cs** owns 24 fixed rows: eight modes times Normal, Clean and Practice. The existing run tracker owns attempt boundaries and the exclusion of empty resets under four seconds. Initial practice/seed aids determine the attempt category. Normal play contributes time without inventing speedrun attempts. The clock credits consecutive observations in the same playable mode/category, separated by at most 0.5 seconds, with focus, living player and remaining objectives; menu ownership, time-scale pause, replay and loading intervals are excluded. A long stall is excluded conservatively. No per-frame allocation is needed. Time is queued through the existing coalesced atomic writer at most once per five seconds; run events retain their existing save behavior.

Run data format 8 keeps the existing checksums, backup recovery and forward-version read-only protection. Old global totals and PBs survive migration; full per-mode counters start at the displayed tracking date, since the bounded old history cannot reconstruct lifetime totals. PB dates missing from older files remain unknown. Resetting counters clears playtime and tracking origin while preserving PBs, their known dates and replays. Protocol 30 appends 908 bytes without changing preceding payload offsets. Both trainer and mod must be updated together.

**Trainer/RunStatistics.h** caches eight formatted rows once per second or on run revision, process, tracking-origin or filter changes. The last snapshot remains browsable after disconnecting. **RunStatisticsUI.h** renders responsive one/two/three-column cards, a bounded detail modal and an explicit Start footer. Browsing and draft setup never load a level. The existing HUD timer remains unchanged. Entry/hover/PB animations use the existing motion preference; Inspector has a separate vector magnifier. Tests cover all modes/categories, unchanged-frame cache reuse, pause/focus/gap accounting, real DLL save/reload/reset/recovery, actual mouse actions, fixed-seed practice starts, reduced motion and 480/800/1280 viewports at 100/150% DPI. The native statistics model also runs under MSVC.

### Search cancellation and game acknowledgements: 7.20.2

The Stop button in **SeedMapUI.h** calls the existing nonblocking `SeedSearch::Stop()` even while a game command is outstanding. **ItStealsTrainer.cpp** remains the sole issuer of the game stop. The existing busy/serial/pending transition tracker now lives in `SeedSearch::TakeGameStop(commandPending)`, polled only by the command thread. It retains cancellation until the preceding game command is acknowledged, consumes it once, retires an older unsent stop when a newer request starts, and keeps the existing distinction between completed Unity streams and portable searches. Route workers do not access these three command-thread fields. Previously the UI both disabled cancellation during acknowledgements and sent a stop that the lifecycle tracker could send again later.

`navigation_startup_tests` exercises delayed acknowledgement, one-time delivery, restart-before-acknowledgement, lost map context, completed Unity input, cancellation during cold preparation and cleanup after CPU completion. These tests run on Linux, under ASan/UBSan and with the Windows compiler. `ui_tests` activates the actual Stop button with/without a pending command at 480/1280 pixels and 100/150% DPI, checking immediate local cancellation, unchanged outstanding command bytes and one-time deferred delivery. The focused `--seed-stop-only` UI mode runs those cases without external recording fixtures. Navigation/scoring algorithms and their result checks retain the 7.20.1 behavior.

### Seed search startup: 7.20.1

The regular search pipeline is now covered from the actual map-byte decoder and map worker through preparation to the first evaluated seeds. While a live map has `stage == 2`, **PracticeMapWorker.h** publishes its immutable map/floor/outline without also running the full preview route solver. **SeedSearch.h** remains the sole owner of search-distance preparation. A normal `stage == 1` map still calculates its preview. This avoids competing, duplicate all-pairs preparation during a search and does not change Unity sampling or GPU backend selection.

Preparation previously published only `PreparedRoutes` row counters, throttled to 200 ms. Local obstacle repairs between rows and the later pickup-contact tables could therefore leave an old value such as 1/160 visible for a long time. The coordinator now publishes activity from the preparation cancellation checks, identifies navigation geometry/paths and pickup contacts/distances, counts the complete preparation work, and always publishes completion. Stop keeps the existing cancellation/context rules.

**PhysicalNavigation.h** retains at most 65536 exact ordered endpoint keys in a direct-mapped segment cache (under 2 MiB per used graph). Each key contains all six float bit patterns, so a hash collision can only cause recomputation. The immutable geometry owns the cache; a geometry replacement cannot reuse another map's checks. Lookups/publication are mutex-protected and computation occurs outside that lock. Both ordinary visibility and local repairs reuse the same capsule/floor checks. A floor triangle fully covering a segment can finish the existing interval-union test early. Repairs also skip already-added/pending obstacles before running their expensive intersection test. None of these changes changes an accepted path or substitutes an unverified shortcut.

`navigation_startup_tests` submits real-format map bytes to the actual map worker and runs the production CPU coordinator for all eight scene IDs, including Timer and Hide & Seek. It checks cancellation during cold preparation, first-search completion, all candidate counters, cold/warm distance-cache reuse, independent identical drawn Top 10, live preview, Stop/restart and ordinary preview calculation after search. Asset-derived fixture hulls are normalized after float conversion before the IPC decoder, matching live convex-footprint invariants. `navigation_visibility_tests` compares 80005 segments with a frozen 7.20.0 floor-clipping reference, then revisits them with cache collisions, reversed endpoints and parallel access; walls, clipped segments, floor holes, heights and geometry isolation remain checked. These are controlled fixtures, not a reproduction of a supplied live Timer map. Both suites run with MSVC /fp:strict and ASan/UBSan; the visibility suite also passes ThreadSanitizer locally.

Alternating single-core Linux runs against the frozen 7.20.0 headers measured the following median cold preparation times (three pairs per case). Timer exercises the shared pipeline with the Outside fixture geometry; the dense variant substitutes an 80-by-80 NavMesh and adds two broad walkable floor triangles. Each before/after pair retained identical fingerprints of every prepared path/distance/estimate and 128 fully scored/drawn seeds (both recipes); scoring is excluded from the preparation timings. These measurements cover preparation, not live game FPS or sustained seeds/s.

| Controlled geometry | 7.20.0 | 7.20.1 | Time reduction |
| --- | ---: | ---: | ---: |
| Hide & Seek | 1086 ms | 541 ms | 50.2% |
| Timer / Outside fixture | 1426 ms | 908 ms | 36.3% |
| Hide & Seek, dense NavMesh | 2042 ms | 1319 ms | 35.4% |
| Timer / Outside, dense NavMesh | 1394 ms | 989 ms | 29.0% |

### Production CPU/GPU seed search: 7.20.0

**SeedGpu.h** plugs into the existing search coordinator after cached navigation preparation. The automatic backend compares two alternating 1024-candidate CPU/GPU trials on complete verified 16-52-crystal Outside and Classic Bonus captures; GPU must save at least 10% of CPU wall time. The profile stays in memory for this session and is keyed by complete navigation geometry, crystal count, RNG descriptor, recipe, goal, spawn filter, accuracy, search size and CPU budget. Only four profiles are retained. A cached CPU decision avoids subsequent GPU setup. Long shortest-route Hide & Seek searches and exact small tours keep the production CPU solver. Longest-route Hide & Seek can also calibrate GPU, using up to 16384 candidates per trial to exercise parallel pickup scoring. Manual GPU selection also admits Hide & Seek, while unsupported/incomplete captures and unavailable FP64 hardware use CPU.

**SeedGpuDevice.h** owns a separate single-threaded D3D11 compute device/context and embeds **SeedOrder.hlsl / SeedOrderKernel.hlsli** in the trainer. These are the Dev 6 algorithms and arithmetic, with lossless triangular FP64 distances and 8-bit local neighbor IDs. Outside uses the dynamic compact kernel; Classic Bonus and manual Hide & Seek use the fixed-count compact kernel. There are no new downloads or external runtime files. **SeedGpuSearch.h** retains one bounded packet per helper, overlapping GPU scoring with CPU generation, spawn filtering, pickup optimization and ranking. Production uses 4096-candidate packets, at most two helpers, or four for Hide & Seek, capped by the existing worker setting. Production GPU scoring does not use the unhelpful count-adaptive bound policy.

The runtime compares 64 startup layouts with CPU scores before each GPU search, verifies one changing sample from every dispatch, and checks every newly published Top 10 against the CPU. Completion is polled before readback, allowing cancellation and a ten-second stalled-driver timeout. A GPU failure discards its results, clears preview/rate caches, disables GPU for the session, and restarts all candidates on CPU with exactly the same search key and criteria. These transitions preserve the existing live preview, pinning, spawn filters, both goals, distinct-seed accounting, cancellation and TSV export. Backend and calibration details appear in the search UI and export. Search time includes preparation/calibration and failed attempts; the short calibration results are not advertised as sustained rates.

The supplied Dev 6 live summary used a Ryzen 5 3600 and RX 5700 XT, with 1116 completed tests, 114425856 candidate evaluations including repeats, 93005143 GPU score audits and no reported mismatch. Held-out long searches, each with three keys and three rounds, measured median complete-search rates of 27886/s Outside and 57898/s Classic Bonus with the selected two-helper/4096 settings. Relative to the best previous shader configurations in that same file these are +56.0% and +43.7%. Hide & Seek CPU bounds reached 483943/s versus 326022/s for the steadier four-helper compact GPU setup. These frozen-map search measurements include validation; they exclude Unity scene loading/rendering and are not a speed guarantee for other hardware. The production path adds sampled/visible-result checks and bounded completion polling, so its live throughput must be checked separately.

Portable tests cover the shared kernel, lossless catalog packing and CPU/GPU pipeline parity for all three maps, both recipes/goals, helpers 1/2/4/6, filters, failure and cancellation. ASan/UBSan covers those three suites. The Windows release gate executes the actual embedded production shaders, full/partial 4096 batches, both accuracy settings, calibration and the real preview/export/pinning coordinator. A second probe embeds a deliberately corrupted shader that passes startup and the initial visible 64 candidates, then returns a bad length in the next packet; the same-key CPU restart must recover the complete expected Top 10. WARP verifies behavior and correctness, not physical GPU performance.

### Seed algorithm research: 7.19.0

Twelve variants were compared with the frozen 7.18.1 pipeline. None is enabled in production. Sequential CPU-affinity runs used asset-derived Outside geometry, a synthetic deterministic RNG descriptor, 52 crystals and synthetic pickup radii. These are algorithm benchmarks, not live Unity FPS measurements or predictions for another PC. Scoring and complete search are separate workloads; compare only the matching reference within each batch. All timed samples and result fingerprints are in [tests/seed_algorithm_experiments.csv](tests/seed_algorithm_experiments.csv).

| Variant | Matching workload | Reference / candidate, median per second | Result |
| --- | --- | --- | --- |
| Compiler specialization for 53 vertices | 10,000 complete scores, radius 1.35, four pairs | 2,157 / 2,117 | Same fingerprint; no gain |
| Contact-aware 2-opt plus contact DP | 6,000 complete-search seeds, radius 1.35, three pairs | 1,781 / 1,696 | Top 10 unchanged; extra work reduces throughput |
| Exact longest-edge candidate restriction | 6,000 complete scores, radius 1.35, three pairs | 2,254 / 1,701 | Same fingerprint; indexing/mask overhead wins |
| Exact cache of negative edge exchanges | Same scoring workload | 2,254 / 1,059 | Same fingerprint; cache overhead wins |
| Reused catalog-indexed bound penalties | 6,000 search seeds, radius 0 / 1.35, three pairs | 12,813 / 16,135; 2,010 / 1,947 | Center-only gain; contact case loses throughput |
| Center-gated penalty reuse, lazy storage | 6,000 search seeds, radius 0 / 0.5 / 1.35, three pairs | 12,044 / 15,407; 2,832 / 2,733; 1,880 / 1,636 | Same Top 10; no reliable Outside contact gain |
| Six LKH candidate/trial/move configurations | 64 layouts per configuration, radius 1.35 | External CLI 50–61 layouts/s, including process/files | Each worsens 12–53 individual contact routes |

The contact refinement separately rescored 10,000 layouts: 828 routes improved, none worsened, but the aggregate improvement was only 1,995.6 of 4,606,340.9 units (0.043%). The warmed bounds reduced complete center scores from 294 to 255 of 6,000 seeds, retaining identical final Top-10 seed indices, lengths and order hashes; the wide-contact case still scored every seed. Even a branch gated away from contact scoring changed measured throughput, so it is not shipped on the strength of a center-only result.

The LKH research used c4v4/LKH3 commit `5bd63680970445d4a595ca2c966ac593b0b32ad1` (archive SHA-256 `5ff7aff0ea362646d0072e0b83be40002c20d38b8614f5c1f6a61285e058857a`). Its academic/noncommercial solver is an external research input, not bundled code or a dependency. The driver encodes a fixed-start open path as ATSP with a free return to spawn, validates each returned permutation and rescores with original double distances plus verified continuous contact DP. Integer solver objectives alone are not comparable to the game's pickup routes. The best mean configuration improved the mean by 5.33 units, but worsened 12/64 routes by up to 8.67 units. [Per-layout quality data](tests/lkh_algorithm_quality.csv) includes all six configurations; subprocess/file overhead is included in CLI timing, so that timing does not isolate the solver's intrinsic cost.

```sh
g++ -std=c++20 -O2 -pthread -ITrainer tests/algorithm_lab.cpp -o build/algorithm-lab
python3 tests/algorithm_lab.py --driver build/algorithm-lab --solver /path/to/LKH --count 64
# Existing production benchmark; optional final "details" prints each score/order hash.
g++ -std=c++20 -O2 -pthread -ITrainer tests/seed_route_benchmark.cpp -o build/seed-route-benchmark
build/seed-route-benchmark tests/fixtures/hide-seek-outside-boxes.routefixture 10000 52 1 score 1.35 details
```

### GUI changes: 7.17.0 to 7.18.0

The replay library rebuilds display/search text when its immutable archive snapshot changes, and filtered order when data, the query, filters or sorting change. Visible rows still use ImGui's existing clipper. Dates are formatted only when a row becomes visible; notes are searched on demand without keeping a second copy of their full text. Name/date/time/mode/seed/status sorting uses run identity as the stable tie-breaker. Archive order remains newest-first for identity lookups. The replay detail editor reuses one snapshot and a binary identity lookup instead of scanning the full library each frame.

Favorites are local library preferences in `Replays/favorites.json`, written atomically by the existing archive worker. Marking one does not rewrite the recording, notes, validity or verification companions, and does not rescan their metadata. Favorites survive refresh/restart; they are not part of shared replay packages.

Settings format 20 adds the eight native trainer shortcuts and reads previous settings with the original defaults. `TrainerPreferences::FormatVersion` also defines the settings reader's future-version limit. Capture accepts modifier combinations, explains conflicts/reserved keys, supports cancel/unassign, and restores defaults independently of display settings. Native registration follows focus and binding changes; text entry, active GUI widgets and the HUD/clip workspaces suspend it. Reassignment consumes the current press instead of invoking the newly assigned action. Custom trainer bindings take priority over the existing fixed replay keys; F1 Freecam, T restart and replay playback controls remain in the game mod. Button labels and help use the saved bindings. The tray can reopen the menu if its shortcut is unassigned.

The fallback timer now compares all enabled bindings with the registered mask, including the eighth action. Fully registered shortcuts use Windows messages instead of unnecessary 20 ms fallback wakes. Missing registrations retain the focused polling fallback. Render/capture frame limits and the mod remain unchanged.

Five alternating before/after pairs on Linux/GCC 13.3, pinned to one CPU, compared 120 unchanged-list passes per run. The frozen 7.17 filter and current cache return identical indices/checksums for the deterministic fixture. [Raw measurements](tests/replay_library_performance.csv) measure only list preparation, excluding ImGui drawing, disk IO, the game and OBS; they are not a live FPS measurement.

| Library size | 7.17 filter per frame, median | Unchanged 7.18 view per frame, median | One initial 7.18 index pass |
| --- | --- | --- | --- |
| 1,000 recordings | 0.214 ms | <0.001 ms | 1.263 ms |
| 10,000 recordings | 2.672 ms | <0.001 ms | 12.192 ms |

```sh
g++ -std=c++20 -O2 -pthread -ITrainer tests/replay_library_benchmark.cpp -o build/replay-library-benchmark
build/replay-library-benchmark
python3 tests/run.py --ui --only hotkey_tests replay_library_tests settings_tests replay_motion_tests ui_tests --jobs 2
```

The regressions cover search/notes, combined filters, deterministic sorting, snapshot invalidation, favorite persistence without recording edits, settings migration and shortcut conflicts/capture/focus edges. Actual ImGui controls exercise sorting, favorite/filter buttons and shortcut assignment/cancel/default restoration at small/normal viewports and 100/150% DPI. The Windows launcher job also runs the portable shortcut/library models with MSVC. Actual keyboard reservation/focus behavior still needs the live check below.

### Performance comparison: 7.18.0 to 7.18.1

Wide pickup regions can leave every Outside seed requiring the complete 52-crystal heuristic, even with the stronger 7.17 bound. The search now prepares sorted center-distance neighbors once per map, then filters those rows for each generated layout. Descending catalog IDs preserve the original local-index tie order. Small tours, sparse catalogs and noncanonical layouts retain the original sort. The shared table is limited to catalogs of at most 256 candidate points (about 12 KiB for the 71-point Outside fixture). Pickup DP also reads each leg's cost/verification tables directly; predecessor order and tie choices are unchanged.

Sequential Linux/GCC 13.3 measurements used the same benchmark source, asset-derived geometry and synthetic deterministic RNG/pickup regions for both revisions. Three alternating pairs per workload ran with CPU affinity and no concurrent builds/tests. Searches warm the geometry with 128 seeds, so these timings exclude cold preparation. Medians and [all 42 measurements](tests/outside_search_performance.csv) are recorded below; these are CPU benchmarks, not measurements from the user's live game or guarantees of FPS/throughput on another PC.

| Workload | Seeds | Workers | Synthetic pickup radius | 7.18.0 seeds/s | 7.18.1 seeds/s | Change |
| --- | --- | --- | --- | --- | --- | --- |
| Outside, complete route scoring | 6,000 | 1 | 1.35 | 1,764 | 2,142 | +21.4% |
| Outside, complete search | 6,000 | 1 | 1.35 | 1,622 | 1,929 | +18.9% |
| Outside, complete search | 6,000 | 4 | 1.35 | 6,080 | 6,372 | +4.8% |
| Outside, complete search | 60,000 | 4 | 1.35 | 4,921 | 6,265 | +27.3% |
| Outside, complete search | 6,000 | 1 | 0.5 | 2,512 | 2,880 | +14.6% |
| Outside, complete search | 6,000 | 1 | 0 (centers) | 11,956 | 12,678 | +6.0% |
| Inside, complete search | 6,000 | 1 | 1.35 | 81,680 | 86,958 | +6.5% |

Short multicore measurements vary noticeably; the longer workload is included rather than treating a subsecond sample as a stable estimate. All compared final Top-10 seed indices, route lengths and visit-order hashes match within each workload. Complete-scoring fingerprints also match across all 6,000 layouts. Every requested seed is still generated and checked. The wide-contact Outside searches score all 6,000 seeds in the short runs and 59,992–59,998 of 60,000 in the long runs. The improvement comes from cheaper scoring; coverage and route quality are preserved.

The neighbor regression compares 384 cases (plus prepared-matrix calls) with independently sorted route matrices, including 1–64 crystals, exact/heuristic mode, equal-distance ties, shuffled selections and sparse fallback. MSVC runs the same regression in the Windows launcher checks. Existing physical-contact and frozen-order tests remain required. Copy results (TSV) now includes backend, workers, generated/scored/cache counters, elapsed time including preparation, average throughput and preparation time to diagnose live results.

### Performance comparison: 7.16.2 to 7.17.0

Sequential Linux/GCC 13.3 comparisons used the same benchmark source, static game-asset geometry/catalogs and synthetic deterministic RNG descriptor on both revisions. Each workload has three alternating before/after pairs of 6,000 seeds; values are median seeds per second. CPU affinity allows the workers plus one coordinator core. A preceding 128-seed search prepares the shared geometry, so timings include generation, bounds, scoring, ranking and drawing, but exclude cold preparation. Raw measurements and Top-10 fingerprints are in [tests/seed_search_performance.csv](tests/seed_search_performance.csv).

| Workload | Workers | Synthetic pickup radius | 7.16.2 | 7.17.0 | Throughput |
| --- | --- | --- | --- | --- | --- |
| Outside, 52 crystals | 1 | 0, center distances | 1,814/s | 10,807/s | 5.96x |
| Outside, 52 crystals | 2 | 0, center distances | 3,089/s | 19,817/s | 6.42x |
| Outside, 52 crystals | 6 | 0, center distances | 8,515/s | 46,636/s | 5.48x |
| Outside, 40 crystals | 1 | 0, center distances | 5,810/s | 27,809/s | 4.79x |
| Inside, 22 crystals | 1 | 0, center distances | 48,611/s | 123,166/s | 2.53x |
| Outside, 52 crystals | 1 | 0.5 | 1,639/s | 2,457/s | 1.50x |
| Outside, 52 crystals | 1 | 1.35 | 1,655/s | 1,706/s | No material gain |

Pickup contacts matter: wider regions weaken the pairwise lower bounds and can leave every seed requiring a complete route score. These synthetic radii are sensitivity checks, not measured live collector radii. These results do not establish a speedup on a user's live capture or any game-FPS improvement. In the center-distance Outside/52/one-worker case, complete scores fall from 5,521 to 294; all 6,000 seeds are still generated and checked. Seed indices, route lengths and visit-order hashes for the final Top 10 match exactly across all 42 timed runs within each workload, including worker counts where applicable.

The new refinement gives crystal vertices penalties `p`. Every open route consists of a crystal spanning tree and one spawn edge, with one crystal endpoint of degree one and all other crystal degrees two. Therefore `MST(c[i,j]+p[i]+p[j]) + min(c[spawn,i]+p[i]) - 2*sum(p) + min(p)` is a lower bound for any penalties. This requires symmetry, not the triangle inequality, and also applies to pickup-contact pair minima. Eight dense Prim/subgradient rounds use fixed stack arrays. Floating-point error is conservatively subtracted; nonfinite refinement falls back to the previous bound.

Refinement runs only for shortest-route searches above 15 crystals, after a finite Top-10 cutoff exists, when the original bound reaches 75% of that cutoff. After 16 consecutive refinements fail to reject a seed, a worker skips refinement for 256 candidates before retrying. This limits overhead on loose contact bounds. RNG, worker count, the complete route solver, heuristic starts/passes, ranking and cancellation remain intact. Longest-route and layout-only searches retain their existing paths.

The bound regression compares 432 results with exact-DP optima, plus known optimal star routes at 16–64 crystals and scales from `1e-9` to `1e100`. Frozen order/length fingerprints cover 336 matrices, including 52-crystal and directed input. The Windows launcher job also compiles and runs both routing checks with MSVC. The existing physical-contact test compares one/six-worker searches with exhaustive scoring of all 4,096 seeds.

Use the current benchmark source against each revision's headers, replacing `-ITrainer` with that revision's Trainer directory. Run timings sequentially without other builds or tests:

```sh
g++ -std=c++20 -O2 -pthread -ITrainer tests/seed_route_benchmark.cpp -o build/seed-route-benchmark
build/seed-route-benchmark tests/fixtures/hide-seek-outside-boxes.routefixture 6000 52 1 search 0
build/seed-route-benchmark tests/fixtures/hide-seek-outside-boxes.routefixture 6000 52 1 search 0.5
build/seed-route-benchmark tests/fixtures/hide-seek-outside-boxes.routefixture 6000 52 1 search 1.35
python3 tests/run.py --sanitize --only route_bound_tests physical_seed_tests --jobs 2 --output build/seed-sanitizers
```

### Performance comparison: 7.15.0 to 7.16.0

Sequential Linux/GCC 13/Mono comparisons used the same benchmark sources and deterministic data on both revisions, without other builds or tests running. Values are medians. Scoring used five alternating before/after pairs; complete CPU searches used three repeats per worker count. These are synthetic CPU measurements, not live game FPS or a measured Hide & Seek Outside speedup.

| Workload | 7.15.0 | 7.16.0 | Change |
| --- | --- | --- | --- |
| Heuristic scoring, 14 crystals | 63,451/s | 75,325/s | +18.7% |
| Heuristic scoring, 22 crystals | 18,376/s | 23,823/s | +29.6% |
| Heuristic scoring, 40 crystals | 3,245/s | 4,501/s | +38.7% |
| Exact scoring, 14 crystals | 1,423/s | 1,397/s | -1.8%; no material improvement |
| Complete 40-crystal CPU search, 1 worker | 3,138/s | 4,374/s | +39.4% |
| Complete 40-crystal CPU search, 2 workers | 6,033/s | 8,371/s | +38.8% |
| Complete 40-crystal CPU search, 6 workers | 16,897/s | 22,208/s | +31.4% |
| Top-down BGRA layout, 120 frames at 1080p | 31.907 ms | 28.598 ms | 10.4% less copy time |

The 2-opt loop reuses the fixed preceding edge and refreshes its first edge after each accepted reversal. It retains every start, pair, pass, comparison and cancellation check. Pickup lower-bound matrices now use one fill pass instead of writing center costs first. Search ranking, pruning rules, RNG and worker scheduling are unchanged. The exact solver itself is unchanged; it showed no throughput gain in this pass.

Each scoring repeat covers 3,000 unique heuristic layouts per size and 1,000 exact layouts. Complete searches evaluate 12,000 seeds per repeat, including generation, scoring, ranking and drawing. Route lengths/order fingerprints and final winning seeds matched both revisions. A separate golden regression covers 288 tied, non-metric and grid matrices with 1–64 crystals, including exact and heuristic orders. Pixel comparisons check every output byte in both orientations and color formats; RGBA and flipped conversions showed no material overall gain.

The supplied game has four Unity splash logos (8.52 seconds configured in `globalgamemanagers`) before Intro at scene index 0; TitleScreen is index 2. MelonLoader 0.7.3's Mono entry point loads normal mods from the first active-scene change, after Unity's splash. Calling `SplashScreen.Stop` from the game mod is therefore too late; a faster IPC handshake cannot fix that ordering.

`StartupLogos` now prepares the settings before `CreateProcessW`, for both Steam and direct launches. It accepts only the supplied 73,120-byte file with SHA-256 `32bf65a2ebc50fcae74103293bbfff572374e0c507e38b38cc2cea84d807ec55`, changes `PlayerSettings.m_ShowUnitySplashScreen` at offset 4200 and the corresponding `BuildSettings.hasPROVersion` player flag at offset 68052, and keeps an original backup. The complete patched SHA-256 is `de13aa2df1085ae10c0f12d90571682165544f8c2f5e074ec369d4e51d94c0a6`. Writes replace the file atomically. The original returns after the mod handshake, a failed launch or trainer shutdown; the next Suite launch, Setup and uninstall also recover an interrupted launch. Restore accepts only an unchanged original or our exact override, preserving external changes and the backup on a mismatch. Unknown builds and write failures keep the normal game launch available and report the reason through the tray and Diagnostics. PlayerPrefs are unchanged.

Suite launches still pass `--practice-suite-skip-intro` for the later Intro scene and `--melonloader.disablestartscreen` for the loader's own screen. The mod checks the loaded active scene after the game's `Start` methods, once per scene. The launch preference is consumed once; a connected preference takes precedence. Portable tests cover exact byte changes, hash guards, interrupted launches, write failures, external changes, byte-identical restoration, launch arguments, delayed connections and display-setting restoration. The same production patch/restore code was also run against a local copy of the supplied SDK file and parsed with UnityPy; game assets stay out of Git. Windows smoke tests check child-process launch arguments and leave unsupported fixture settings untouched. Actual Unity startup timing and visible logos still require the live check below. The camera chain, OBS GPU-sharing path, capture rates and recording formats remain intact.

Use the current benchmark sources against each revision's headers/sources:

```sh
g++ -std=c++20 -O2 -pthread -DSEARCHLAB_WORKSPACE -ITrainer tests/route_order_benchmark.cpp -o build/route-benchmark
build/route-benchmark
g++ -std=c++20 -O2 -pthread -ITrainer tests/cpu_search_benchmark.cpp -o build/cpu-benchmark
build/cpu-benchmark 12000 route 1 40
build/cpu-benchmark 12000 route 2 40
build/cpu-benchmark 12000 route 6 40
mcs -langversion:7.2 -optimize+ -out:build/clip-pixel-benchmark.exe tests/clip_pixel_benchmark.cs Mod/ClipPixelLayout.cs
mono build/clip-pixel-benchmark.exe 120
```

### Performance comparison: 7.14.2 to 7.15.0

Local Linux benchmarks used the same compiler, deterministic fixtures and interleaved before/after runs. Values below are medians. These measure isolated CPU paths, not game FPS or every map's search speed.

| Workload | 7.14.2 | 7.15.0 | Scope |
| --- | --- | --- | --- |
| Layout-only CPU scan, 6 workers | 4.47M/s | 13.18M/s | 5M layouts per run, 9 runs per build |
| Exact scoring, 14 crystals | 1,071/s | 1,387/s | 48 unique layouts, 3 runs per build |
| Heuristic scoring, 14 crystals | 58,054/s | 64,069/s | 3,000 unique layouts, 3 runs per build |
| Stationary pose sampling | 185 ms | 108 ms | 480,200 observations, 3 runs per build |
| Additional live allocation for prepared search distances | 1,411,264 bytes | 47,248 bytes | 384-node catalog, excludes the shared table and process RSS |

Full 18-crystal heuristic searches stayed around 60,000 seeds/s with two workers; this fixture showed no material overall gain. The 22/40-crystal heuristic scoring benchmarks also stayed effectively unchanged. Route lengths and order hashes matched. All four pose profiles retained byte-identical serialized output and the same adaptive sampling schedules. Heavy route searches retain their existing 64-seed work packets and cancellation checks.

Build each benchmark against the desired revision's headers; use the same current benchmark source on both revisions. Run timings sequentially without compiling or testing in parallel:

```sh
g++ -std=c++20 -O2 -pthread -ITrainer tests/cpu_search_benchmark.cpp -o build/cpu-benchmark
build/cpu-benchmark 500000 layout
g++ -std=c++20 -O2 -pthread -DSEARCHLAB_WORKSPACE -ITrainer tests/route_order_benchmark.cpp -o build/route-benchmark
build/route-benchmark
g++ -std=c++20 -O2 -pthread -ITrainer tests/distance_table_benchmark.cpp -o build/distance-benchmark
build/distance-benchmark
mcs -langversion:7.2 -optimize+ -main:PoseSamplingBenchmark -out:build/pose-benchmark.exe tests/pose_sampling_benchmark.cs tests/adaptive_capture_tests.cs Mod/ReplayVisualData.cs Mod/AdaptivePoseSampler.cs Mod/ReplayCoverage.cs
mono build/pose-benchmark.exe 200
```

The Windows release job also encodes 1080p clips at 60 FPS (653 frames) and 120 FPS (587 and 3,601 frames) with the supplied encoder, checks the AVI stream clocks, and decodes the recorded PCM for an exact comparison. Moving RGB bars exercise frame order and spatial detail. To repeat it, run `python tests/verify_audio_exports.py build/verification/replay_experience_tests/work bin/PracticeSuiteEncoder.exe --avi-only`. If independent FFmpeg and ffprobe are installed, it also checks every video timestamp and decoded RGB frame. These checks verify encoded files; they do not measure playback speed in a user's media player.

## Check in game

Use the release downloads and note the Suite version. Mark each check **Passed**, **Failed**, or **Not tested**. Keep results and diagnostic reports with the test discussion or bug report.

- **Install and start:** Check that Setup finds the correct game folder. Launch the desktop shortcut, open the menu with **F3**, and confirm the game is connected. Alt-Tab away and back; check overlay visibility, cursor control, and windowed/borderless mode.
- **Intro preference:** With Skip game intro enabled, restart through the Suite and check that Unity logos and the loader start screen are skipped, then the main menu opens with working cursor, sound and saved quality settings. Test Steam and direct launches, including fresh PlayerPrefs. After the mod connects, `globalgamemanagers` must match the original SHA-256 above and the `.practicesuite-original` backup must be gone. Disable skipping and restart; normal logos return. Test a failed launch and close the trainer during startup; the original must return. After killing both processes during startup, the next Suite launch or Setup must recover the backup. Unknown or externally modified settings must be preserved, with a tray warning and a diagnostic reason. Opening the Suite after logos started cannot retroactively remove their playback. Run/death scenes must remain unchanged.
- **Map and seed:** Compare walls, player spawn, crystals, and movement with the minimap. In Practice, use **Apply Seed & Reload** twice with the same seed and seed type; compare the player and crystal spawns. Seed numbers need the matching **Practice base seed** or **Captured spawn seed** setting. For Timer, test a subsequent start; its first start forces spawn #5.
- **Run and replay:** Set the **Recording rule** low enough to retain the test attempts before starting. Complete one run and abort another with **ESC**. Check their outcomes, then open each replay and compare movement, pickups, enemy observations, and light flashes with what happened. Pause, seek, and test 3D playback; after leaving it, check game input, camera, and audio.
- **Archive and camera editor:** Name a retained replay, export its package, and reopen the replay after restarting the Suite. Import the package and check that the existing run stays intact. In Clip Studio, add and adjust a camera keypoint, preview the move, and export a short clip. Check the saved file's picture, duration, and sound if recorded.
- **Seed search:** Let each route method fill the Top 10 with the spawn filter set to **Any player spawn**. Check the ranked results and selected preview. **Use best seed** copies the mode, seed, and seed type; load it with **Apply Seed & Reload** and compare the layout. Stop and restart the search, then repeat with a specific spawn filter.
- **Settings and update:** Save one visible HUD setting and restart the Suite. Confirm it and the retained replays remain. When a newer release is available, test **Later**, installation, and the restart; check the version and retained data afterward. Until then, mark update installation **Not tested**.
- **Shortcuts and library:** Reassign the menu/picker to a modifier combination, type into replay search/notes, Alt-Tab away/back and hold/release the shortcut. Check one action per press and no action while typing or in another application. Try duplicate and reserved keys, cancel capture, unassign and recover through the tray, then restore defaults. In Replay, combine mode/status/favorite filters, sort columns both ways, clear filters and reopen an older recording. Favorites must survive restart without changing its name, notes, original files or run verdict.

Repeat the map, seed, and run checks in **Classic**, **Camera**, **Hide & Seek**, **Timer**, **Ghost**, **Classic Bonus**, **Camera Bright**, and **Hide & Seek Outside**. In Timer, check enemy positions during a run. In both Hide & Seek modes, compare walls and replay paths closely. Test flashes only where the relevant light action exists.

For a performance check, stand at the same spot in a fixed-seed Practice run. Compare FPS over 30 seconds with seed search stopped and running. Note the mode, seed type, route method, worker count, active generator (verified CPU or Unity), seeds per second, and any stutter. Keep the game resolution, FPS/VSync limits, and recording settings the same. Treat searches with different random seeds as approximate comparisons.

If a check fails, use the [bug form](.github/ISSUE_TEMPLATE/bug.yml). Include the version, mode, seed and seed type, exact steps, and expected/actual result. Under **Diagnostics**, use **Run diagnostic scan → Copy scan result**. For a replay issue, also use **Recording check → Copy recording check** and include the replay package if available. For search issues, **Copy results (TSV)** keeps the relevant results.

Once the checks pass, capture real footage for the GitHub tour: seed search, replay playback, Inspector selection, and the finish screen.

## Capture and final release checks

Enable **Settings > System > Include trainer in game captures (OBS)**. In OBS, capture **It Steals** with Window Capture (Windows 10 method) or Game Capture. DX11 shares one GPU texture for the complete trainer UI, including F3 menus, Clip Studio, tooltips and run verification. The trainer renders once; a busy consumer drops a capture frame instead of waiting on input. Unity copies the shared image on its rendering thread, without CPU pixels or texture uploads. The native window retains input and stays visible until the game acknowledges its image. Stale frames and disconnects restore the normal presentation. Unsupported GPU sharing falls back to bounded, asynchronous CPU copies of the existing target; it never renders the UI twice.

Use **Passed / Failed / Not tested** for each row. Record the Suite version and OBS capture method. Screenshots or a short clip of a failed check are useful.

| Check | Steps and expected result |
| --- | --- |
| OBS picture | Capture only the It Steals window, enable the capture setting, and close the Suite menu. The HUD, timer and validity badge appear once, with readable text and correct colors/transparency. |
| OBS menu | Open F3, change a HUD color, move a block, and close F3. The HUD style matches after closing F3. The complete menu also appears in the game source, without a second OBS source. Menu buttons, scrolling, dragging and the custom cursor must feel as responsive with capture enabled as disabled. |
| Focus and display | Alt-Tab to OBS and back three times. Resize the game, then try borderless fullscreen. The overlay follows the game, clears outside it, and returns without a frozen copy or duplicate panels. |
| Capture fallback | Toggle capture off and on, then close the trainer while the game is open. The normal overlay works when disabled and no in-game image remains after disconnect. Restarting the trainer reconnects. |
| Capture performance | At the same fixed-seed position, compare 30 seconds with capture off and on. Note game FPS, menu responsiveness, helper work, and stutter; test a menu and a passive HUD separately, including the highest resolution you use. |
| Crystal effects | Record a new run. In native 3D playback, watch an uncollected crystal, pause, seek backward, and resume. Compare its rotation and hologram with the game. Seeking twice to the same time should show the same effect phase. Older files use a replay-relative shader phase. |
| Other animations | Check the player's arms, enemy motion, sprite orientation, lights, a ceiling fan, eye color changes and vault-button materials. Leave 3D playback and verify normal game movement, camera, audio and animations resume. |
| Full-run video | In Clip Studio, set a short range, alter camera/speed and select a video preset, resolution and FPS. Click **Full run video** in the footer. The export covers the whole run with the original camera, normal speed and recorded audio, using the chosen video output settings. Clip edits stay intact. GIF/PNG output settings use 1080p60 MP4 for this button. A fresh file check runs first. Check the beginning, ending, sound and duration in VLC, then the eight-second animated run-verification ending. The window and sidecar must retain original warnings and show partial/missing channels honestly. |
| Game UI and effects | Record crystal pickups and a third monster flash. Compare remaining crystals, RUN, hands, flashlight, pickup particles and phantom particles in the game, replay and exported video. Pause, seek back and repeat in each supported mode. |
| Replay recheck | Click Recheck replay for one completed and one invalid/aborted run. The stored verdict and warnings must remain unchanged. With a copy of the archive, damage an input or audio companion after loading it; a fresh recheck must fail despite the cached preview. |
| Sustained AVI | Export over 3,601 frames at 1080p120 with Best Quality AVI. Note the output size and any slowdown. Keep the JSON sidecar and compare exportProgress entries before/after frame 3,000. |
| 120 FPS video | Export a short 1080p120 lossless clip with recorded sound. Check smooth playback and audio sync in VLC. Check the accompanying JSON for 120 FPS and matching container/audio durations. |
| Statistics and routes | Clear the local counters, complete a run and abort another. Confirm PBs and saved replays remain. Check both Hide & Seek routes and that full-route visibility survives selecting another seed. |
| Install and update | Use the release Setup, confirm the detected game folder, and check versions after updating. Settings and replays must remain. Close the game: the trainer exits and its graphic remains visible for about 1.65 seconds. |

Repeat the mode checks above in every supported mode before marking the public release ready. Keep the broader mode/seed/replay checks in the previous section as part of this pass.

### Replay fidelity

The game assembly and asset metadata were checked alongside capture and playback. Coverage means successful observations at the stated resolution, not every pixel or proof of replay origin.

| Channel | New recordings | Remaining limits / live checks |
| --- | --- | --- |
| Player / animation | Adaptive player, monster and world poses, plus the separate Arms hierarchy, renderer states and blend shapes. | The hand camera is discovered from the actual camera rig even when the game leaves its BodyCam field unassigned. Compare arm/flashlight actions, eye changes, fan motion and crystal effects at normal speed, pause and backwards seeks. |
| Cameras / game UI | Local poses and settings for BodyCam, UICamera and radar cameras; UI text, sprites, textures, layout, colors, groups and known material values at up to 60 Hz. | Compare remaining crystals, RUN messages, battery/breath indicators, radar, masks and multiple export aspect ratios. Unrecorded material properties may differ. |
| Particles | Public particle values, emitter transforms, seeds and renderer states at up to 60 Hz. The original Instantiate call is observed for runtime bursts. Only visual-only particle prefabs can be recreated, with at most 128 runtime effect instances. Their automatic destruction is suspended during playback so backwards seeks remain possible. | Compare crystal pickup bursts and phantom effects. Trails/lines, unsafe or ambiguous runtime prefabs and effects between sampled frames can differ. Native engine playback structs are used only in memory to restore the scene, never loaded from replay files. |
| Audio | Listener PCM plus supported 2D bypass routes, verified companions and sample-based timing. Gap reports identify start/interior/end boundaries and queue saturation. | Compare every mode and audible route. Spatial or filtered bypass routes that cannot be reconstructed retain explicit source warnings. A successful checksum does not erase capture gaps. |
| Recheck / full-run ending | Fresh reads, schema/identity/timeline checks, original-report comparisons and SHA-256 hashes on the archive worker. Full-run videos append the original animated verification window for eight seconds without requesting extra game renders. | A recheck never replaces the original run verdict or scans a past process. Local unsigned files and editable software cannot provide absolute tamper resistance or leaderboard approval. |

Older recordings cannot gain missing channels. Missing asset bindings appear in the 3D status. Compare each mode against a real recording before public release, then leave 3D playback and confirm that cameras, original UI, particles, input and audio restore correctly.

For a reported PCM gap, copy Recording check from a new replay and keep its `.isaudio` file. The audio header distinguishes missing sample ranges from listener/bypass queue overruns. Older audio headers cannot reveal a cause that was never recorded.

For an export slowdown, keep the finished video's JSON sidecar. `exportProgress` records cumulative timings, bytes and working sets every 500 frames, so compare adjacent entries around the slowdown. Encoding/pipe time, waiting for new source frames and scene render/transfer work are separate. Memory samples include the trainer, encoder and game where available. The 3,601-frame AVI check crosses the reported frame count; it does not reproduce a particular PC, disk or live Unity scene.
**Deferred:** automatic PB recording will be considered separately after this capture work. The full-run button currently renders a replay; it does not start or control OBS, automatically save PB videos, upload a run, or certify leaderboard eligibility.

## Ship a release

1. Change **SUITE_VERSION** and **SUITE_VERSION_COMPONENTS** in **Trainer/SuiteBranding.h** together; keep the previous exporter version in **ReplayPackage.h**'s compatibility table and its package-import regression test. When changing the mod, also update **ItStealsMod.Version**, **SUITE_MOD_VERSION**, and the supplied DLL. Packaging checks the DLL's file version against that expected version.
2. Add a short entry at the top of **CHANGELOG.md**, using the format below.
3. Run `python repository.py sync`, check the changes, and push to **main**.
4. Watch **Actions → Build and publish PracticeSuite**. Resolve failures before publishing another version.

```markdown
### vX.X.X Changelog

- Removed [feature/file].
- Improved [feature]: [what works better and how].
- Fixed a bug caused by [issue].
```

Only include bullets that apply. Describe the change someone will notice. Use the method name when it helps explain the result.

Keep artwork, downloads, and installation notes outside the version's changelog section, under their own headings. The trainer reads only `### vX.X.X Changelog`; this keeps GitHub's layout out of the in-game history.

The workflow syncs the README's **What's new** section, checks documentation, runs the Linux logic/UI/mod-contract suite, and builds on Windows. It then checks the installer, updater, game launcher, and real private-release download. A newer push cancels the obsolete build. Only a successful run publishes the seven downloads together. The verified trainer is committed back to **bin/**. Published binaries and tags are not overwritten.

Release descriptions use the same changelog and download template. The workflow also refreshes the presentation of existing release pages without replacing their files. New versions need a new version number. A short-lived Actions token handles publication; no publishing token belongs in the source.

For a local Windows package build, install **NSIS 3**, then run:

```powershell
.\tests\installer_tests.ps1
.\tests\updater_tests.ps1
.\Installer\Release.ps1 -PlatformToolset v143
.\tests\launcher_windows_tests.ps1 -TrainerBinary .\build\release\ItStealsTrainer.exe
```

Packages go into **build/release/**. This builds the native trainer and bundles the supplied compatible mod **7.16.2**. A complete mod rebuild needs the local game SDKs and **build.ps1**.

Game discovery checks saved Suite paths, Steam installation entries, running game/Steam processes, standard Steam folders, and configured libraries. Candidates must contain the supported executable and managed assembly. Discovery skips unreadable metadata and does not scan whole drives. Windows tests also launch the actual discovery command and read its Unicode result through the same INI API used by Setup.

## Keep changes readable

- Write code, comments, UI, and docs in English. Explain the reason for a check or workaround when it is not obvious.
- Prefer descriptive names and small, coherent components. Avoid splitting a simple change across extra files.
- Format first-party C++/C# with **clang-format 18** and the root configuration; use Ruff formatting for Python. Leave vendored ImGui and generated protocol files alone.
- Preserve protocol sizes, file-format checks, compatible recordings, and validation rules. Do not remove a check to make a test pass.
- Keep Unity work on the game thread and analysis on native workers. Restore renderer, camera, timing, audio, and input changes on exit or failure.
- Keep commits focused. In a pull request, describe the behavior change and how it was checked; include a screenshot when the UI changes.
- Keep backup files, change reports, test output, credentials, user recordings, SDKs, and unused previews out of Git.

Bug and feature forms are available in **Issues**. For a bug, include the Suite version, game mode, seed when relevant, reproduction steps, and the observed result. A small diagnostic excerpt or replay is more useful than an unexplained full log.

## Maintain the presentation

Keep the README focused on installation and real features. `python repository.py sync` updates its version spotlight from the changelog. `python repository.py check` checks versions, local links, asset references, and changelog format.

Run **assets/build.py** with Pillow and CairoSVG to regenerate the logo, Setup artwork, and repository visuals. UI images are captures from the production ImGui tests, rendered with **tests/render_ui.py**. The preview labels identify test data; no generated illustration is used as a game screenshot.

When real recordings are ready, replace the tour with a short sequence of seed search, replay playback, Inspector selection, and the finish screen. Keep the cards readable on mobile and check captions against the image. Use real footage for normal-game/collider and editor/export comparisons. Retain only assets used by the page.

The artwork generator also writes a 1280 × 640 social preview to **build/branding/social-preview.png**. Generated previews stay out of Git. Upload it under **Settings → Social preview** when the repository becomes public. The Suite's own licensing should also be decided before public distribution; bundled dependencies retain their existing licenses.

## Update boundaries

Updates only select stable releases from **mabtm/It_Steals-Suite**. The transport checks HTTPS, asset identity, size, and GitHub's SHA-256 digest. Credentials are sent only to **api.github.com**, stored in Windows Credential Manager, and never passed to the installer.

The updater verifies the complete package and exact process identities before applying it. It asks for elevation before closing applications, rolls back failed file replacement, and restarts through its unelevated controller. Settings and replays remain outside the installation. Prompt state belongs to the running trainer/game session; restarting the trainer permits a fresh notification.

These checks are part of the update design. Keep them intact when changing its UI or release packaging.

