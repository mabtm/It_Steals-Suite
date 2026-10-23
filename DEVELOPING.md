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

Map format 14 adds walkable collision-floor triangles alongside the existing NavMesh and obstacle capture. Input format 7 keeps bounded visual tracks and records explanations for missing audio routes. Both readers still accept previous formats; their original limits remain in place.

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

Python 3 and a C++20 compiler run the native tests. `--ui` adds real ImGui interaction tests; `--managed` adds six C# contract programs and requires Mono's `mcs` and `mono`.

```sh
python3 generate-layout.py --check
python3 tests/run.py --ui --managed --jobs 2
python3 repository.py check
```

Use `--only TEST_NAME`, `--output DIRECTORY`, or `--compiler COMPILER` for a focused run. Results go into ignored **build/** directories. Assertions stay enabled. For route changes, also run:

```sh
python3 tests/run.py --sanitize --only physical_edge_tests physical_route_tests routing_reliability_tests --output build/sanitized
```

Tests use fixed captures and synthetic fixtures. Some fixtures contain non-English Unicode text to check file handling. Portable checks cover logic and contracts; live Unity behavior, interactive UAC, device input, and actual performance still need testing in game. The supplied mod is not rebuilt by contract tests.

The Windows release job also encodes 653 distinct 1080p frames at 60 FPS with the supplied encoder, checks the AVI stream clocks, and decodes the recorded PCM for an exact comparison. To repeat it, run `python tests/verify_audio_exports.py build/verification/replay_experience_tests/work bin/PracticeSuiteEncoder.exe --avi-only`. If independent FFmpeg and ffprobe are installed, it also checks every video timestamp and decoded RGB frame.

## Check in game

Use the release downloads and note the Suite version. Mark each check **Passed**, **Failed**, or **Not tested**. Keep results and diagnostic reports with the test discussion or bug report.

- **Install and start:** Check that Setup finds the correct game folder. Launch the desktop shortcut, open the menu with **F3**, and confirm the game is connected. Alt-Tab away and back; check overlay visibility, cursor control, and windowed/borderless mode.
- **Map and seed:** Compare walls, player spawn, crystals, and movement with the minimap. In Practice, use **Apply Seed & Reload** twice with the same seed and seed type; compare the player and crystal spawns. Seed numbers need the matching **Practice base seed** or **Captured spawn seed** setting. For Timer, test a subsequent start; its first start forces spawn #5.
- **Run and replay:** Set the **Recording rule** low enough to retain the test attempts before starting. Complete one run and abort another with **ESC**. Check their outcomes, then open each replay and compare movement, pickups, enemy observations, and light flashes with what happened. Pause, seek, and test 3D playback; after leaving it, check game input, camera, and audio.
- **Archive and camera editor:** Name a retained replay, export its package, and reopen the replay after restarting the Suite. Import the package and check that the existing run stays intact. In Clip Studio, add and adjust a camera keypoint, preview the move, and export a short clip. Check the saved file's picture, duration, and sound if recorded.
- **Seed search:** Let each route method fill the Top 10 with the spawn filter set to **Any player spawn**. Check the ranked results and selected preview. **Use best seed** copies the mode, seed, and seed type; load it with **Apply Seed & Reload** and compare the layout. Stop and restart the search, then repeat with a specific spawn filter.
- **Settings and update:** Save one visible HUD setting and restart the Suite. Confirm it and the retained replays remain. When a newer release is available, test **Later**, installation, and the restart; check the version and retained data afterward. Until then, mark update installation **Not tested**.

Repeat the map, seed, and run checks in **Classic**, **Camera**, **Hide & Seek**, **Timer**, **Ghost**, **Classic Bonus**, **Camera Bright**, and **Hide & Seek Outside**. In Timer, check enemy positions during a run. In both Hide & Seek modes, compare walls and replay paths closely. Test flashes only where the relevant light action exists.

For a performance check, stand at the same spot in a fixed-seed Practice run. Compare FPS over 30 seconds with seed search stopped and running. Note the mode, seed type, route method, worker count, active generator (verified CPU or Unity), seeds per second, and any stutter. Keep the game resolution, FPS/VSync limits, and recording settings the same. Treat searches with different random seeds as approximate comparisons.

If a check fails, use the [bug form](.github/ISSUE_TEMPLATE/bug.yml). Include the version, mode, seed and seed type, exact steps, and expected/actual result. Under **Diagnostics**, use **Run diagnostic scan → Copy scan result**. For a replay issue, also use **Recording check → Copy recording check** and include the replay package if available. For search issues, **Copy results (TSV)** keeps the relevant results.

Once the checks pass, capture real footage for the GitHub tour: seed search, replay playback, Inspector selection, and the finish screen.

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

Packages go into **build/release/**. This builds the native trainer and bundles the supplied compatible mod **7.12.5**. A complete mod rebuild needs the local game SDKs and **build.ps1**.

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
