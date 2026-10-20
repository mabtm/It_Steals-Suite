# It Steals PracticeSuite

**v7.11.5 · mab**

A Windows trainer and MelonLoader mod for practice, seed routing, speedruns, replay analysis, and clip creation.

## Install

1. Install [MelonLoader](https://github.com/LavaGang/MelonLoader.Installer/releases) for your installed **It Steals.exe**, then launch the game once so the loader creates its folders; install any prerequisites requested by the official installer.
2. Close the game and copy **bin/ItStealsHelper.dll** from this repository into the game's **Mods** folder; this is the included PracticeSuite mod.
3. Keep **bin/ItStealsTrainer.exe** and **bin/PracticeSuiteEncoder.exe** together, with the **Encoder** folder alongside them for its source and licenses.
4. Start the game and trainer, then press **F3** to open the menu; use windowed or borderless mode for the overlay.

No compiler is needed to use the supplied binaries. Trainer **7.11.5** intentionally uses the compatible mod **7.11.4**. Game assets and Unity/MelonLoader assemblies come from your local installation and are not included.

If the trainer does not connect, check that MelonLoader loaded the mod, then open **Diagnostics** for the connection status and suggested fixes.

## Features:

- **Fullbright:** Brighten scenes with reversible unlit texture shaders for practice and replay analysis.
- **Freecam:** Explore the scene independently of the player without collecting crystals, or move the player to the camera for practice.
- **Actual colliders:** Display captured collider shapes for the player, monsters, crystals, and scene geometry.
- **Fixed-seed practice:** Apply a chosen seed with its seed type and spawn settings for repeatable layouts.
- **Scene controls:** Load supported game modes directly for practice and refresh captured geometry.
- **Seed search:** Rank layouts using spawn filters, complete routes, background workers, and verified portable generation where available.
- **Pickup routing:** Plan checked walkable paths that account for player clearance and captured crystal collection regions.
- **Route timing:** Estimate travel time using continuously learned straight, turning, starting, and slope movement when compatible data is available.
- **Seed preview:** Inspect layouts, preview spawn choices, reveal full optimal paths, and transfer selected seeds into practice settings.
- **Minimap:** View physical wall contours, crystals, player trails, monster paths, and optional route guidance.
- **Clean speedruns:** Track timer, splits, personal bests, seed, and observed validity with a mandatory status badge.
- **Run statistics:** Retain attempts, resets, completions, deaths, local bests, and daily/session progress.
- **Run results:** Keep a dismissible finish screen with check details, personal-best comparison, seed, and replay access.
- **HUD customization:** Edit HUD placement, scale, opacity, colors, fonts, metrics, and camera heading.
- **Input overlay:** Show selectable keyboard, mouse, controller, and observed player-state information in game or replay clips.
- **Replay library:** Retain eligible completed or aborted runs with status, mode, seed, time, names, categories, and notes.
- **2D replay analysis:** Compare recorded and planned routes alongside pickups, monster movement, and flashlight/camera flashes.
- **3D replay:** Play recorded movement and camera data through recorded, follow, or free views with original or simplified scene graphics.
- **Replay controls:** Seek, pause, loop, adjust playback speed, and add named timeline markers.
- **Replay layers:** Toggle captured colliders, speed trails, ceiling visibility, fullbright, and supported scene overlays.
- **Recording check:** Inspect available movement, camera, input, collider, animation, visual, and audio coverage before analysis or export.
- **Recorded game events:** Review observed monster states, game queries, animation data, and recorded game audio when captured.
- **Replay packages:** Import or export supported replay packages while preserving identity and compatible companion tracks.
- **Cinematic keypoints:** Author camera cuts and eased moves, switch camera modes, and animate playback speed and scene-layer settings.
- **Clip Studio:** Preview selected timeline ranges and customize information, input graphics, composition, and export settings.
- **Video export:** Export MP4, lossless MKV/AVI, editing MOV, GIF, or PNG sequences with purpose-specific presets and supported 30/60/120 FPS settings.
- **Size-limited export:** Re-encode a rendered clip against a file-size budget using two-pass compression.
- **Live Inspector:** Pick visible layers, inspect object and collider properties, watch changes, compare baselines, and probe paths in a dedicated workspace or floating card.
- **Diagnostics:** Scan known connection and performance conditions and copy reports or save local diagnostic snapshots without uploading them.
- **Appearance:** Customize theme colors, fonts, animation, quiet interface sounds, and startup graphics.
- **Display controls:** Configure overlay refresh and temporary game FPS/VSync or borderless-window settings without rewriting saved game preferences.

Supported modes: **Classic, Camera, Hide & Seek, Timer, Ghost, Classic Bonus, Camera Bright, and Hide & Seek Outside**. Map geometry and seed search in both Hide & Seek modes remain **WIP**.

Route quality labels describe the captured model: larger layouts and pickup-region ordering are heuristic, and time estimates do not predict enemy interference or player mistakes. Recorded tracks cannot recover information missing from older runs. Local validity checks detect observed violations; they are not tamper-proof certification.

## Shortcuts

| Key | Action |
| --- | --- |
| F1 | Toggle Freecam in game |
| F2 / F3 | Toggle overlay visibility / open or close the menu |
| F4 | Toggle the compact HUD when allowed |
| F5 / F6 | Refresh map data / change the game window mode |
| F7 | Start or cancel the Inspector picker |
| F8 | Analyze the finished run while the prompt is available |
| Shift+T / T | Toggle speedrun mode / restart in game |

[Development and tests](DEVELOPING.md) · [Changelog](CHANGELOG.md) · [Encoder source and licenses](Encoder/NOTICE.md)
