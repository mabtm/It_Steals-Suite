# Changelog

Changes by version. The full development history is in Git.

### v7.16.0 Changelog

- Improved seed search on larger routes by reusing edge costs inside 2-opt and filling pickup lower bounds in one pass. Every starting route, improvement pass and seed remains covered.
- Improved clip transfers by copying top-down pixel buffers in one block. Flipped images and color conversion retain their existing output.
- Fixed a bug caused by relying on a separate Start hook to skip the intro. Skipping now waits for scene initialization, after the game's volume and first-launch quality setup.

### v7.15.0 Changelog

- Improved seed search by sharing distance tables and reusing worker buffers. Layout scans and exact route scoring do less repeated work; seed coverage, route quality and Top 10 ordering stay the same.
- Improved replay recording by skipping repeated pose calculations for stationary objects. Capture rates and stored poses stay the same. Diagnostics now include the OBS transfer work on the game thread.
- Improved startup with a saved Skip game intro option under Settings > System. The game still initializes its sound and first-launch settings before opening the main menu.
- Fixed a bug caused by the Windows launch-test fixture exiting during cleanup, which could incorrectly mark a successful launcher test as failed.

### v7.14.2 Changelog

- Improved OBS capture with a shared DX11 texture for the complete trainer UI, including menus, Clip Studio and run verification. Busy capture frames are skipped without waiting; the UI is rendered once.
- Fixed a bug caused by scenes with an unassigned BodyCam reference. Replay recording now finds the actual hand camera in the player's camera rig.
- Improved attempt statistics by ignoring empty resets under four seconds.

### v7.14.1 Changelog

- Fixed a bug caused by Recorded game sending the world camera to a different texture from the hands and game UI. The view and video export now keep the game's camera chain intact.
- Fixed a bug caused by resetting the audio clock during recorder setup. Each run now keeps one PCM timeline, so setup delays cannot create artificial audio gaps.
- Improved movement recording from the start of a run. A stationary setup interval is recognised only when both body observations agree and no physics step occurred. Missing samples still appear in Recording check.
- Improved Recheck replay to reopen the same animated run verification shown at a finish, with all seven checks and their technical evidence. New retained runs store those original values; missing historical values are labelled.
- Improved full-run videos with the same validation window and an eight-second closing sequence. Replay packages keep the saved verification values and check their identity when imported or rechecked.

### v7.14.0 Changelog

- Improved OBS mode by keeping menus on the trainer's normal render path. The capture option sends the compact and speedrun HUD to the game; record menus through a separate trainer-window source.
- Improved new replay recordings with player arms, body/UI/radar cameras, game UI and particle snapshots. This includes the crystal counter and game messages such as RUN. Existing recordings keep their available tracks.
- Improved replay checks with Recheck replay and a Run check tab in Clip Studio. Full-run video exports check the files again and finish with a five-second result card and a smooth transition. Stored validity and warnings stay unchanged.
- Improved audio capture with integer sample clocks, larger bounded queues and exact PCM gap diagnostics. Recording check now separates missing timeline samples from listener and bypass queue overruns.
- Improved mouse movement with the correct vertical direction and circular scaling that preserves diagonals.
- Improved long exports by reusing visibility snapshot buffers and sending one cancellable encoder write per frame. Export reports track speed, stage timings, output size and memory every 500 frames; release checks now include a 3,601-frame 1080p120 AVI.

### v7.13.1 Changelog

- Fixed a bug caused by slow shared-memory reads delaying the OBS menu. Capture now uses bulk copies, releases its lock before GPU uploads, and reuses the rendered menu instead of drawing it twice.
- Improved Clip Studio with its own Full run video button. It exports the original camera at normal speed, keeps your video output settings, and preserves clip edits.
- Improved video export transfers with a bounded bulk copy instead of byte-wise writes between the game and trainer.
- Improved native replays with recorded mesh material changes, including eye and door-indicator materials, plus world-sprite poses. Missing visual assets are reported instead of silently ignored.
- Improved Recording check with explicit replay limits for particles, the original game UI, procedural material properties, and objects created after scene discovery.

### v7.13.0 Changelog

- Improved OBS capture with an optional in-game copy of the trainer's HUD and menu. Enable it under Settings > System, then capture the It Steals window in OBS.
- Improved 3D replays with a separate material clock for animated effects such as crystal holograms. Effects follow playback, pause, seeks, and exports.
- Improved full-run exports with a 1080p60 preset using the original camera and normal speed. Export reports identify rendered replays.
- Improved the release checks with GPU transfer, capture fallback, shader timing, and full-run export tests.

### v7.12.6 Changelog

- Improved the exit graphic with an extra second on screen.
- Improved export reports with the actual average data rate, calculated from file size and duration.
- Improved AVI verification with moving-image tests at 1080p60 and 1080p120, including frame timing and exact recorded-audio checks.

### v7.12.5 Changelog

- Improved Hide & Seek routing in both variants with captured collision floors, so gaps in the enemy NavMesh no longer cut off walkable connections.
- Improved recorded-audio exports with one encoding pass, RGB UtVideo, and an AVI timing check before the file is saved. Export reports now include the container's frame count, FPS, and audio duration.
- Fixed a bug caused by animation changes forcing pose samples across the whole scene; unchanged rotations now compress correctly, and the bounded visual recording can hold longer runs.
- Improved audio capture at run start with a short PCM buffer and support for filterless 2D bypass sources. Remaining missing routes include source names and reasons in Recording check.
- Improved the replay input panel with separate keyboard, mouse, controller, and player tabs.
- Improved HUD customization with a preview drawn by the actual overlay renderer, using the selected fields and current settings.
- Improved diagnostics with route, recording, export, and mod-version checks, clearer evidence, and suggested next steps.
- Improved the Speedrun menu with focused setup and statistics cards, a counter reset that preserves personal bests and replays, and an exclusion for empty resets under one second.
- Removed the Recent runs table from the Speedrun menu.
- Improved route previews by keeping the full route visible when the selected seed changes; animation is optional and starts disabled.
- Improved shutdown so the trainer closes with the game and can show its exit graphic afterward.

### v7.12.4 Changelog

- Improved code readability with named replay jobs and smaller menu and camera sections.
- Removed duplicate validation code, redundant interface work, and an unused preview; related timing and input helpers now share fewer files.
- Improved replay-package maintenance by keeping exporter and annotation compatibility in one table.

### v7.12.3 Changelog

- Fixed a bug caused by a PowerShell startup error that stopped automatic game detection.
- Improved game detection by checking saved paths, Windows installation entries, running processes, and Steam libraries.
- Improved setup with a Detect button to search again without restarting the installer.

### v7.12.2 Changelog

- Removed outdated WIP labels from both Hide & Seek modes.
- Improved the GitHub page with feature cards, an interface tour, and shorter guides.
- Improved releases with consistent notes and checks that keep downloads and documentation on the same version.
- Improved project credits to explain both the AI-generated code and the reverse engineering, testing, and direction behind it.
- Fixed a bug caused by GitHub page formatting appearing in the in-game changelog.
- Fixed a bug caused by the version change excluding compatible 7.12.1 replay packages.

### v7.12.1 Changelog

- Improved updates by implementing background GitHub checks, verified downloads, file rollback, and automatic restart.
- Improved release access by implementing Windows Credential Manager storage that keeps private-release tokens on the user's PC.
- Improved the interface by implementing animated update prompts, a manual Updates tab, and offline changelogs after the intro.
- Fixed a bug caused by repeated scene changes showing the same update prompt; restarting the trainer starts a new notification session.
- Fixed a bug caused by a version change rejecting compatible replay packages.

### v7.12.0 Changelog

- Improved installation by implementing a guided installer with Steam detection, verified MelonLoader downloads, rollback, and branded shortcuts.
- Improved startup by implementing optional one-time game launch with existing-process detection and a manual tray action.
- Improved distribution by implementing verified Windows release packages, individual binaries, checksums, and a redesigned repository page.
- Fixed a bug caused by a version bump excluding replay packages exported by the previous version.

### v7.11.5 Changelog

- Removed obsolete change reports, generated galleries, backup artifacts, and redundant helper files.
- Improved maintainability by implementing consistent formatting, grouped adjacent models, compact English documentation, and one portable test runner.
- Improved the Live Inspector by implementing themed object cards, bounded animations, selection feedback, and optional interface sounds.
- Fixed a bug caused by outdated build instructions and machine-specific test paths.

### v7.11.4 Changelog

- Improved large-layout routing by implementing a cached route graph, faster neighbor queries, and improved visit ordering.
- Fixed a bug caused by incomplete navigation data hiding otherwise checked route segments.

### v7.11.3 Changelog

- Improved seed previews by implementing persistent route drawing, immediate full-path display, and a directional pulse.
- Fixed a bug caused by refreshing the same layout resetting route visibility.

### v7.11.2 Changelog

- Improved object picking by implementing layered visual hits for meshes, sprites, and colliders.
- Fixed a bug caused by duplicate property groups when opening object details.

### v7.11.1 Changelog

- Improved Inspector access by implementing a picker shortcut, hover feedback, and floating object cards.
- Fixed a bug caused by picking with cursor coordinates from a different frame.

### v7.11.0 Changelog

- Improved debugging by implementing a read-only Live Inspector with watches, comparisons, route probes, and local snapshots.

### v7.10.1 Changelog

- Fixed a bug caused by unreachable crystal centers rejecting valid pickup contacts and thin-wall detours.

### v7.10.0 Changelog

- Improved routing by implementing captured pickup regions and player-clearance checks instead of center-only crystal visits.
- Improved map accuracy by implementing physical wall contours shared across supported modes.

### v7.9.0 Changelog

- Improved export throughput by implementing three frame buffers and overlapping GPU readback with encoding.
- Improved replay analysis by implementing recording coverage checks and adaptive pose capture.

### v7.8.1 Changelog

- Fixed a bug caused by the brightness model obscuring original textures by restoring reversible unlit shaders.
- Fixed a bug caused by crystal pickup audio bypassing the listener recording point.

### v7.8.0 Changelog

- Improved replay fidelity by implementing recorded game audio, observed monster queries, and animation-state tracks.

### v7.7.0 Changelog

- Improved exports by implementing purpose-specific codec presets, lossless formats, and two-pass file-size budgeting.

### v7.6.0 Changelog

- Improved 3D replay fidelity by implementing original scene materials, captured visual poses, and playback-speed keypoints.

### v7.5.2 Changelog

- Improved cinematic editing by implementing exact-time camera-mode keypoints, per-shot layers, and path visualization.
- Fixed a bug caused by keypoint editing retaining a constrained follow-camera pose.

### v7.5.1 Changelog

- Improved lossless playback by implementing a faster-decoding FFV1 profile with explicit frame timing.
- Improved input presentation by implementing compact device layouts and a combined player-activity card.

### v7.5.0 Changelog

- Improved clip creation by implementing camera keypoints, editable overlays, and lossless video output.

### v7.4.0 Changelog

- Improved customization by implementing an on-screen HUD editor, selectable input modules, and Clip Studio previews.

### v7.3.5 Changelog

- Improved replay visibility by implementing separate fade materials that preserve the original render batches.

### v7.3.1 Changelog

- Improved replay readability by implementing clearer collider lines, a speed-colored trail, and an independent follow-camera orbit.

### v7.3.0 Changelog

- Improved 3D analysis by implementing timeline controls, named markers, collider overlays, and stronger scene contrast.

### v7.2.0 Changelog

- Improved replay analysis by implementing recorded movement, camera/input tracks, and a lightweight 3D viewer.

### v7.1.0 Changelog

- Improved interface feedback by implementing animated navigation, event timelines, and personal-best presentation.

### v7.0.3 Changelog

- Improved finish checks by implementing selectable technical scan details and copyable reports.

### v7.0.2 Changelog

- Improved run results by implementing staged validation feedback and a persistent analysis action.

### v7.0.1 Changelog

- Fixed a bug caused by startup visibility and scene transitions dismissing intro or finish feedback too early.

### v7.0.0 Changelog

- Removed the redundant Overview tab and unused seed-search controls.
- Improved usability by implementing larger HUD text, replay retention settings, finish screens, and guided diagnostics.

### v6.9.9 Changelog

- Improved appearance by implementing customizable HUD colors, fonts, and animated startup graphics.

### v6.9.8 Changelog

- Improved responsiveness by implementing optional motion, bounded widget animations, and quiet interface sounds.

### v6.9.7 Changelog

- Removed approximate hitbox rendering in favor of actual supported collider shapes.
- Improved route timing by implementing bounded online movement statistics across five movement classes.
- Improved overlay efficiency by implementing cropped panel readback and reusable navigation caches.

### v6.9.6 Changelog

- Improved replay analysis by implementing observed light events and shared navigation checks across modes.

### v6.9.0 Changelog

- Improved seed-search throughput by implementing verified portable generation and cached background route evaluation.

### v5.0.0 Changelog

- Improved overlays by implementing configurable HUD profiles, interpolated telemetry, and scoped menu input ownership.
