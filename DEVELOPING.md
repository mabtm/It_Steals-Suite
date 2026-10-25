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

**ReplayArchive.h** queues named disk operations on one worker; **ReplayPackage.h** owns package formats and exporter compatibility. **TrainerUI.h** groups menu pages and settings, while **ClipUI.h** groups camera, layer, keypoint, and export controls. These sections share the existing state and UI identifiers.

**PerformanceTiming.h** contains frame-cost and search-progress measurements. **MovementTiming.h** contains recorded movement analysis and travel-time estimates. Input colors live with their drawing helpers in **InputModules.h**.

Map format 14 adds walkable collision-floor triangles alongside the existing NavMesh and obstacle capture. Input format 8 adds bounded snapshots for game UI, extra cameras and particles. All recording channels share the 128 MiB input-file budget; omitted observations remain visible in Recording check. Audio format 1 uses checksummed reserved header bytes for PCM gap timestamps and queue diagnostics. Both readers still accept previous formats; their original limits remain in place.

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

Tests use fixed captures and synthetic fixtures. Some fixtures contain non-English Unicode text to check file handling. Portable checks cover logic and contracts; live Unity behavior, interactive UAC, device input, and actual performance still need testing in game. The supplied mod is not rebuilt by contract tests; compile it separately against the matching game SDK and loader.

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

Packages go into **build/release/**. This builds the native trainer and bundles the supplied compatible mod **7.16.0**. A complete mod rebuild needs the local game SDKs and **build.ps1**.

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

