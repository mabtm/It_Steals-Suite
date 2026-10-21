# Development

The suite has two runtimes: **Mod/** runs inside Unity through MelonLoader; **Trainer/** is a native C++20 Win64/ImGui application connected through bounded shared-memory channels.

## Structure

| Location | Responsibility |
| --- | --- |
| Mod/ | Game hooks, capture, scene geometry, seed generation, run storage, and replay rendering |
| Trainer/ | Overlay, settings, background routing/search, Inspector, replay library, and export orchestration |
| Trainer/imgui/ | Unmodified third-party UI sources and their license |
| Installer/ | Guided setup, verified installation/updates, release packaging, and ownership-aware uninstall |
| assets/ | Editable repository branding and pre-generated Windows icon/installer graphics |
| tests/ | Logic tests, platform contract adapters, benchmarks, and fixed compatibility fixtures |
| bin/ | Supplied compatible trainer, mod, and separate video encoder |
| Encoder/ | Encoder build script, corresponding source archives, signatures, and licenses |
| protocol-layout.json | Source of truth for the generated main shared-memory layout |

Models, codecs, algorithms, and platform adapters remain separate where they can be tested independently; small adjacent preference and restore helpers are grouped with their owning models. Build output belongs in ignored directories, not historical result folders.

## Build on Windows

Install Visual Studio C++ Build Tools with a Windows SDK, .NET Framework 4.7.2 targeting support, and MelonLoader in your game installation. From PowerShell:

```powershell
.\build.ps1 -GameDir 'C:\Games\It Steals'
```

The script locates Mono-compatible loader/Harmony references and MSBuild, then builds both projects into **bin/**. If necessary, pass `-MelonAssemblyPath`, `-HarmonyAssemblyPath`, or `-PlatformToolset v143` for your installed tools; the project defaults to v145. A source build replaces the local supplied trainer/mod binaries, so validate it before distributing a release.

Original game, Unity, loader, and Harmony assemblies are local build inputs. Do not commit them. Rebuild the encoder separately with **Encoder/build-encoder.sh** on Linux/WSL; distribute its source/licenses and update the trainer's encoder identity together, as described in [Encoder/NOTICE.md](Encoder/NOTICE.md).

## Release packages

On Windows, install **NSIS 3** alongside C++ Build Tools, then run:

```powershell
.\tests\installer_tests.ps1
.\tests\updater_tests.ps1
.\Installer\Release.ps1 -PlatformToolset v143
.\tests\launcher_windows_tests.ps1 -TrainerBinary .\build\release\ItStealsTrainer.exe
```

The release script builds the native trainer with an embedded icon/version resource and static runtime, bundles the compatible existing mod **7.11.4**, and writes Setup, portable ZIP, individual binaries, corresponding encoder sources, and checksums to **build/release/**. It does not claim to rebuild the SDK-dependent mod. Use the main build script for a complete mod/trainer source build.

The release workflow runs the portable suite on Linux, then installer contracts, the real MSVC build, packaging, controlled Windows launcher/update process tests, and an authenticated download of the existing private release through the production WinHTTP transport. Only then does it create a draft, upload all seven assets, and publish the release. Existing published versions are kept; increment **SUITE_VERSION** in **Trainer/SuiteBranding.h** for a new release. Release notes come from the complete current entry in CHANGELOG.md. No long-lived publishing token is stored in the repository.

Installer tests cover corruption, traversal, rollback, loader preservation, the actual pinned official archive, and uninstall ownership using temporary folders. Launcher tests use an isolated x64 test executable and local settings; they do not launch or certify a live Unity session. Native installation UI/UAC and in-game behavior still need a Windows game check.

The installer keeps complete existing MelonLoader installations. New installs use the official **0.7.3 x64** archive with a pinned SHA-256, bounded download and extraction, and transactional file writes. Uninstall preserves modified files, other mods, shared MelonLoader, and user data.

Branding uses the existing Suite route mark. Normal builds use committed assets; to regenerate icons/installer bitmaps, install Pillow/CairoSVG and run **assets/build.py**. README screenshots are production ImGui captures using synthetic test fixtures.

## Update contract

**SuiteUpdateModel.h** owns bounded release parsing, semantic version comparison, asset identity, and prompt rules. **SuiteUpdater.h** owns one background WinHTTP/hash worker, immutable UI snapshots, Windows Credential Manager, and staging. **SuiteUpdateUI.h** renders the Updates workspace, prompt, and embedded changelog. Windows resources embed the current updater, installer, and full CHANGELOG.md, so individual trainer downloads retain the trusted bootstrap.

Only stable releases from **mabtm/It_Steals-Suite** are selected. Downloads require the exact portable asset name, a bounded size, and GitHub's SHA-256 digest. HTTPS requests retain certificate verification; redirects are handled explicitly, and Authorization is sent only to api.github.com. Missing access or an incomplete package never triggers installation.

**Installer/Update.ps1** verifies and extracts the package, checks exact process image paths and creation times, requests elevation before closing applications, and reuses **Install-Suite** for transactional replacement and rollback. Its original controller restarts the trainer without elevation; **--restart-after-update** requests game startup even when ordinary auto-start is disabled. No GitHub token is passed to the helper. Update preferences contain only the auto-check choice and last-read changelog version; prompt suppression is per running trainer/game session.

The Windows transport test reads a short-lived Actions token from its explicit test environment, exercises real Credential Manager and private release/CDN downloads, verifies the complete package, and deletes the credential. It refuses to replace an existing saved credential. Process fixtures and filesystem fault injection test the production apply step without interacting with a real Unity game. Interactive UAC acceptance, live game shutdown, and restart still need an in-game test.

## Reproducible checks

Python 3 and a C++20 compiler are sufficient for the native tests. Add `--ui` for the real ImGui interaction suite and `--managed` for five C# contract test programs using Mono's `mcs` and `mono`:

```sh
python3 generate-layout.py --check
python3 tests/run.py --ui --managed
python3 tests/run.py --sanitize --only physical_edge_tests physical_route_tests routing_reliability_tests --output build/sanitized
```

The runner writes builds, logs, and a machine-readable summary to **build/tests/**; `--output`, `--jobs`, `--compiler`, and `--only` select another directory or a focused run. Assertions remain enabled. The GPU-channel contract program runs all five readback layouts/fallback variants.

Fixtures preserve current and legacy wire formats independently of generated results. The two `.assetmesh` files contain extracted NavMesh triangle positions and indices for the Hide & Seek regression tests; `.routefixture` files contain captured world geometry used by the routing reliability tests. Other fixtures are synthetic protocol, input, audio, and Inspector samples, not real run evidence. Some codec tests intentionally include non-English Unicode strings as data.

SDK-dependent C# integration tests and benchmarks remain available as sources in **tests/**; the portable runner does not claim to execute those without their original game/Unity/loader references. Platform adapters test production logic against controlled contracts, not Windows drivers, live Unity pixels, real input devices, or in-game performance. A Win64 cross-build also requires a real Windows/game session for live validation.

Optional tools: `tests/render_ui.py` renders ImGui draw captures using NumPy/Pillow; `verify_*` scripts validate actual exported media using the encoder and independent FFmpeg/ffprobe. Use fresh output folders, never real user replay directories.

## Runtime boundaries

- Unity APIs, hooks, scene capture, and restoration stay on the game thread; bounded queues move storage and analysis work away from it.
- Native workers operate on immutable captures and cached geometry rather than querying Unity for every search candidate.
- IPC, settings, and replay readers keep version, identity, size, and malformed-data checks; compatible older formats remain readable.
- Replay scenes restore owned renderer, camera, timing, audio, and input state when leaving or when setup fails.
- Run validity records observed aid and warning flags; presentation and recording completeness do not replace those checks.

## Data ownership

| Contract | Authoritative definitions |
| --- | --- |
| Main IPC | protocol-layout.json → generate-layout.py → Mod/SharedLayout.cs and Trainer/SharedProtocol.h |
| Maps and run reviews | Mod/PracticeMapPacket.cs, Mod/PracticeMapArchive.cs, Trainer/PracticeMapData.h |
| Input and motion | Mod/ReplayMotion.cs, Trainer/ReplayMotion.h |
| Captured visual/audio tracks | Mod/ReplayVisualData.cs, Mod/ReplayExperienceData.cs, Mod/ReplayAudioFile.cs, native Replay* readers |
| Inspector | Mod/InspectorProtocol.cs, Trainer/InspectorModel.h and InspectorClient.h |
| Clips and frame channel | Trainer/ClipProject.h, Trainer/ClipFrameWire.h, Mod/ReplayClipChannel.cs |

## Maintenance

Use English for code, comments, UI, and documentation. Format first-party C++/C# with **clang-format 18** and the root configuration; leave generated protocol files and vendored ImGui unchanged. Python follows Ruff formatting.

Keep README focused on installation and actual features. Add short entries to **CHANGELOG.md** using `### vX.X.X Changelog` and `Removed`, `Improved`, or `Fixed a bug caused by` bullets. Do not add backup files, per-version reports, generated previews, credentials, user recordings, or downloaded SDKs to Git. The import and earlier implementation details remain available in commit history.
