<picture>
  <source media="(max-width: 720px)" srcset="assets/hero-mobile.svg">
  <img src="assets/hero.svg" alt="It Steals PracticeSuite — Practice. Understand. Improve." width="100%">
</picture>

<p align="center">
  Practice seeds, find routes, study your runs, and turn replays into clips.<br>
  <strong>For It Steals · Windows x64 · MelonLoader</strong>
</p>

<p align="center">
  <a href="https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Setup.exe"><img src="assets/get-setup.svg" width="264" alt="Download Setup — recommended"></a>
  &nbsp;
  <a href="https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Portable.zip"><img src="assets/get-portable.svg" width="264" alt="Download portable ZIP — manual installation"></a>
</p>

<p align="center">
  <a href="#install">Install</a> · <a href="#features">Features</a> · <a href="#downloads">Downloads</a> · <a href="CHANGELOG.md">Changelog</a> · <a href="#about-the-project">About</a> · <a href="DEVELOPING.md">Development</a>
</p>

<img src="assets/showcase.gif" alt="A short tour of the actual Suite interface: practice, route search, replay review, and Clip Studio. All captures use test data." width="100%">

<sub>Actual Suite interface with test data. These previews show the tools and layout; they are not recordings of the game.</sub>

## What's new

<!-- release:start -->
**[v7.16.1](https://github.com/mabtm/It_Steals-Suite/releases/tag/v7.16.1)**

- Fixed a bug caused by applying Skip game intro only after the Unity startup logos and trainer handshake. Suite launches now pass the saved preference directly to the mod, stop the Unity splash and suppress the loader start screen before the first game update.
- Fixed a bug caused by requiring an initial scene-initialized callback to leave the intro. The first game update now checks the loaded scene directly, after the game's original startup setup, with a single menu transition.

[Full changelog](CHANGELOG.md)
<!-- release:end -->

## Install

1. Download **[Setup](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Setup.exe)** and close the game.
2. Run it, confirm your **It Steals** folder, and choose where to install the Suite.
3. Open the desktop shortcut. The trainer starts the game; press **F3** for the menu.

Setup detects Steam libraries, Windows installation entries, and the previously saved game path. It installs the trainer, mod, and encoder, adds the Suite icon to your shortcuts, and installs MelonLoader if needed. Use **windowed or borderless** mode for the overlay. You need Windows 10/11 x64 and an installed Windows Mono copy of It Steals. Setup asks for administrator access; a first-time MelonLoader install also needs internet access.

<details>
<summary><strong>Manual installation</strong></summary>

1. Install [MelonLoader](https://github.com/LavaGang/MelonLoader.Installer/releases) for **It Steals.exe** and launch the game once.
2. Extract the [portable package](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Portable.zip).
3. Close the game and copy **ItStealsHelper.dll** into its **Mods** folder.
4. Keep **ItStealsTrainer.exe**, **PracticeSuiteEncoder.exe**, and **Encoder/** together, then open the trainer.

Steam installs are detected automatically. For another location, create **installation.ini** beside the trainer:

```ini
gameExecutable=C:\Games\It Steals\It Steals.exe
```

Turn automatic game startup off in **Settings → System**, or use **--no-launch** for one launch.

</details>

### Updates

Open **Updates** to check for a release, see **Current / New**, or install it. The in-game **Install / Later** prompt appears once per game session; restarting the trainer allows it again. Updates wait while a run or export is active.

The package is downloaded and checked before anything closes. After you accept the Windows permission prompt, the updater closes the game and trainer, installs the files, and restarts both. Settings and replays stay in place. A new version shows its changelog once after the intro; the full history is also available offline.

<details>
<summary><strong>Access to private releases</strong></summary>

While the repository is private, open **Updates → Connect private releases** and save a GitHub token limited to this repository with **Contents: read** access. It is stored in Windows Credential Manager for your Windows account. **Disconnect** removes it. Public releases need no token.

</details>

## Features:

<img src="assets/features.png" alt="Four Suite workspaces: Practice, Routing, Replay, and Clip Studio, shown with test data" width="100%">

| Workspace | What you can do |
| --- | --- |
| **Practice** | Repeat a seed, explore with freecam and fullbright, show colliders, and inspect live objects. |
| **Routing** | Search seeds, compare layouts, preview spawns, and plan crystal routes with learned movement timing. |
| **Replay** | Record runs, compare paths, follow monster events, and review movement in 2D or 3D. |
| **Clip Studio** | Edit camera moves, inputs, and overlays, then export a finished clip. |

<details>
<summary><strong>All features</strong></summary>

**Practice & inspection**

- **Seed controls:** Repeat a seed with the same seed type and spawn settings.
- **Scene controls:** Load a supported mode and refresh its map capture.
- **Freecam:** Move independently without collecting crystals, with optional player teleport for practice.
- **Fullbright:** Brighten the scene while keeping its textures, then restore it when disabled.
- **Colliders:** Show captured collision shapes for the scene, player, monsters, and crystals.
- **Live Inspector:** Pick objects and overlapping layers, inspect properties, watch changes, compare saved values, and test paths.

**Seeds & routes**

- **Seed search:** Search in the background and rank layouts using spawn filters and complete routes.
- **Route planning:** Find walkable crystal paths using player clearance and crystal pickup areas.
- **Time estimates:** Learn from recorded movement to account for straights, turns, starts, and slopes.
- **Seed preview:** View spawns and full routes, then use the selected seed in practice.
- **Minimap:** Show walls, crystals, player trails, monster movement, and optional route guidance.

**Runs & HUDs**

- **Run timer:** Track time, splits, personal bests, seed, and observed run validity.
- **Statistics:** Keep attempts, resets, completions, deaths, and daily or session progress.
- **Finish screen:** Review the result, run checks, personal-best comparison, and replay.
- **HUD editor:** Change placement, size, opacity, colors, fonts, metrics, and camera heading.
- **Input overlay:** Choose keyboard, mouse, controller, and player-state displays for play or clips.

**Replay & clips**

- **Replay library:** Keep completed or aborted runs with their mode, seed, time, status, name, category, and notes.
- **2D review:** Compare your path with a planned route, including pickups, monster movement, and light flashes.
- **3D playback:** Review movement with recorded, follow, or free cameras in the original or simplified scene.
- **Timeline:** Seek, pause, loop, change speed, and add named markers.
- **Replay layers:** Toggle captured colliders, speed trails, ceilings, fullbright, and scene overlays.
- **Capture checks:** Inspect recorded coverage and recheck each replay and its companions from disk.
- **Game events:** Review captured monster states, animation data, game queries, and game audio. New recordings include player arms, game UI and particle snapshots.
- **Replay packages:** Import and export compatible runs with their companion tracks.
- **Camera editing:** Add cuts and smooth camera moves, switch views, and keyframe playback speed or scene layers.
- **Clip Studio:** Set a range and arrange information, input, and export layers before rendering.
- **Export:** Create MP4, lossless MKV/AVI, editing MOV, GIF, or PNG sequences with supported 30/60/120 FPS presets. Full-run videos include a replay-check card at the end.
- **File-size targets:** Re-encode a clip with two-pass compression to work toward a chosen size.

**Settings & maintenance**

- **Appearance:** Customize themes, fonts, animations, interface sounds, and the intro.
- **Display controls:** Set overlay refresh, temporary game FPS/VSync overrides, and borderless mode.
- **Diagnostics:** Check connection and performance issues and save or copy a local report.
- **Updates:** Check GitHub releases, install verified packages, restart automatically, and read the version history.
- **Game startup:** Start the game with the trainer, reuse a running game, or disable automatic startup.

</details>

### Game modes

| Main modes | Bonus modes |
| --- | --- |
| Classic | Classic Bonus |
| Camera | Camera Bright |
| Hide & Seek | Hide & Seek Outside |
| Timer · Ghost | — |

<details>
<summary><strong>See the tools side by side</strong></summary>

<img src="assets/comparison.png" alt="Route planning, replay review, and object inspection side by side. Actual Suite interface with test data; no gameplay footage." width="100%">

Plan a route, compare it with a recorded path, and inspect the objects behind it. These are interface captures with test data. In-game graphics and collider comparisons will use real recordings when those are ready.

</details>

## Downloads

[Latest release](https://github.com/mabtm/It_Steals-Suite/releases/latest) · [All releases](https://github.com/mabtm/It_Steals-Suite/releases)

| File | Use |
| --- | --- |
| **[Setup](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Setup.exe)** | Recommended. Installs everything and creates shortcuts. |
| **[Portable ZIP](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsSuite-Portable.zip)** | Complete package for manual installation. |
| **[Trainer EXE](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsTrainer.exe)** | The trainer application. |
| **[Encoder EXE](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/PracticeSuiteEncoder.exe)** | Video encoder; keep it beside the trainer. |
| **[Mod DLL](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/ItStealsHelper.dll)** | Game mod; goes in the game's **Mods** folder. |
| **[Encoder sources](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/Encoder-Sources.zip)** | Encoder source, build script, and licenses. |
| **[Checksums](https://github.com/mabtm/It_Steals-Suite/releases/latest/download/SHA256SUMS.txt)** | SHA-256 hashes for the downloads. |

### Shortcuts

| Key | Action |
| --- | --- |
| **F3** | Open or close the menu |
| **F1 / F2 / F4** | Freecam / overlay visibility / compact HUD, when allowed |
| **F5 / F6** | Refresh map data / request borderless mode |
| **F7 / F8** | Inspector picker / analyze the finished run while its prompt is visible |
| **Shift+T / T** | Toggle speedrun mode / restart in game |

<details>
<summary><strong>Help & useful limits</strong></summary>

- **No connection?** Check that MelonLoader loads **ItStealsHelper.dll**, then open **Diagnostics**.
- **No overlay?** Use windowed or borderless mode. **F3** opens the menu; **F6** requests borderless.
- **Game did not start?** Check **Settings → System** and your game path, or use **Start It Steals** from the tray menu.
- **Windows warning?** Builds are unsigned. Download from this repository and use the release checksums to check the files.
- **What does Setup keep?** Settings, replays, other mods, and a complete existing MelonLoader install. A failed file copy rolls back; uninstall keeps modified files and user data.
- **How exact are routes?** Larger layouts use heuristics. Time estimates depend on captured movement and cannot predict enemy interference or mistakes.
- **What can a replay show?** Only the tracks captured during that run. Missing tracks cannot be recreated later.
- **Are run checks official approval?** They describe observed local conditions. Leaderboard rules and moderator decisions still apply.

</details>

## About the project

I'm **mab**, an It Steals speedrunner. I started this project to build the practice and analysis tools I wanted to use myself.

This is a **vibe-coded project**: the Suite's own code is AI-generated. I direct development, reverse-engineer the game, work out how its systems behave, and test the results in game. I also help investigate problems and decide what needs to change. Making that code clear, readable, and maintainable is part of the work.

The source and changes are tracked here. Bug reports and ideas are welcome through [Issues](https://github.com/mabtm/It_Steals-Suite/issues); build instructions, the code map, and contribution notes are in [DEVELOPING.md](DEVELOPING.md).

Unofficial community project. Original game assets and game/Unity/MelonLoader assemblies are not included. Third-party code keeps its own credits and licenses.

[Development](DEVELOPING.md) · [Changelog](CHANGELOG.md) · [Encoder licenses](Encoder/NOTICE.md) · [ImGui license](Trainer/imgui/LICENSE.txt)

