# Changelog

Changes by version. The full development history is in Git.

### v7.25.0 Changelog

- Improved live Set-Seed splits with an independently positioned panel, defaulting to the bottom center. Choose timer HUD, independent panel or both; drag, resize, undo and save the panel in the on-screen editor. Position, text size, colors, animation, display duration, opacity and optional pace displays persist independently of timer metrics. The default is the latest crystal delta as text on a fully transparent background; a compact panel remains optional, also showing only the latest pickup.
- Improved sector timing with separate sector and total deltas, neutral equal times and gold indicators for new observed sector bests. Sector comparisons require the same pickup prefix; same-crystal total deltas remain available when pickup order changes.
- Improved Set-Seed pace with a checkpoint-based finish forecast that updates at crystals and increases when the current sector runs late. A changed route suspends the forecast; completed runs show their observed finish time. Forecasts are estimates, not guaranteed finish times.
- Improved theoretical-best timing by summing the best observed sector durations on the exact seed, recipe, spawn, layout and route, including the finish sector. Zero-duration simultaneous pickups and finishes remain valid. In-progress improvements stay provisional until final eligibility checks pass.
- Improved sector history with independent, checksummed storage and atomic replacement, last-good recovery and future-version preservation. Eligible reset/death attempts contribute even below replay-retention thresholds; existing retained clean replays are backfilled. Practice, invalid attempts and missing historical timestamps do not invent bests.
- Improved background-only sector persistence and bounded per-pickup lookups, with shutdown flushing and visible storage failures. The preview remains outside stable automatic updates.

### v7.24.0 Changelog

- Improved category visibility in the live speedrun HUD, recorded-run verification and clip information. Run setup now shows the best time for its draft mode/category, including separately stored, explicitly unranked Practice bests.
- Improved Set-Seed timing with automatic crystal splits against the fastest matching completed recording, preferring valid PBs. Mode, selected seed, recipe, spawn position and crystal layout must match. Deltas follow the same crystal even when pickup order differs; no route or hidden-object hints are exposed.
- Improved clip composition with default-on optional Set-Seed splits, independent position, scale and opacity, replay-time animations and a frozen export reference. Older clip projects retain their original layouts.
- Fixed a bug caused by skinned replay renderers reusing matrices and motion history across multiple export seeks inside one Unity frame. Recorded arm rigs now refresh skinning matrices per render and disable stale motion vectors. Visual confirmation of the reported arm flicker still requires an in-game export.
- Improved the native replay library with compact animated recording rows, clearer category/time/status hierarchy, quick sorting and an optional detailed table. Existing filtering, favorites, metadata and recordings are preserved.
- Improved preview isolation: this development update remains a GitHub pre-release and is excluded from stable automatic updates.

### v7.23.0 Changelog

- Improved speedrun organization with a permanent attempt archive, sortable PBs, local Top 5, history and statistics across all eight modes. Random Seed and Set-Seed stay separate; exact seed/recipe and session/date filters do not change run setup. Earlier retained attempts, scalar PBs and lifetime counters remain intact, with missing historical evidence clearly labelled.
- Fixed a bug caused by extra replay cameras holding transforms between samples while the view and flashlight interpolated. Arm-camera poses now follow the requested replay/export time, including backward seeks; new recordings preserve stationary endpoints before movement. Capture gaps remain discontinuous. The reported arm alignment still requires visual confirmation in the game.
- Improved replay retention so valid PB/Top-5 candidates bypass ordinary duration/crystal thresholds, receive storage priority and keep a retention marker. Stable identities link archive rows to committed recordings; storage backpressure and unavailable replays remain visible.
- Improved replay organization with recorded-category filtering/sorting, distinct from editable library labels. Mode/category/status/favorites filters and sort order survive restart.
- Improved replay analysis with observed pickup/segment splits and selectable reference recordings. Split deltas require matching mode, category, seed recipe, layout and pickup order; divergent routes remain unpaired.
- Improved capture diagnostics with bounded CPU profiles for collider, audio, scene/camera/UI, effects, rig-pose and view stages. Helper CPU measurements include camera callbacks, and duplicate capture callbacks within one render frame are avoided. All PCs use the same recording policy.
- Improved the native Runs workspace with short tab transitions, responsive record tables and collapsible details. This version remains a pre-release outside stable automatic updates.

### v7.22.0 Changelog

- Improved speedrun categories with explicit Set-Seed selection, separately stored personal bests, both deterministic seed recipes and latched start evidence. Other aids remain invalid; seed/category changes invalidate the attempt, and historical Practice runs retain their status.
- Improved the Suite as Elite-Suite while retaining its native dark in-game style: compact top tabs, clearly separated training controls and a mode list beside category/seed setup replace the large sidebar and cards. Explicit Start, Details and context actions remain available; settings, replays and installation identities stay compatible.
- Fixed a bug caused by navigation refinement repeatedly repairing the same disconnected bunker component after the first checked connection. Timer preparation now reports its actual stages and reuses physically verified routes; fixed walls, floors and player clearance remain checked.
- Improved native menu motion with short tab-selection and page transitions, bounded hover feedback and short original interface sounds. The visible menu retains its 60 FPS target; hidden and passive output keep their reduced work budgets, with measured output cadence available in diagnostics.
- Improved mode selection and seed adoption so browsing does not immediately cover the start controls, and starting a mode uses the seed most recently selected in the seed workspace.
- Improved bug reporting with a background navigation-capture export that preserves the exact active search packet and its progress details for reproducible diagnostics.

### v7.21.0 Changelog

- Fixed a bug caused by incomplete official MelonLoader installations blocking Suite Setup: missing files can now be repaired from the checksum-verified distribution while existing mods/settings remain intact. Modified or conflicting loaders are rejected before file changes, and interrupted repairs roll back.
- Improved navigation preparation with on-demand physical detour searches that retain wall, floor and clearance validation. Timer captures distinguish the game's movable vault-door collider from the fixed bunker walls.
- Fixed a bug caused by export cameras observing mixed game/export resolutions: world, arms and scene display now share the export-sized target before rendering, UI layout is refreshed, and temporal post-processing history is reset between export samples. Normal playback and game-owned textures are restored after capture.
- Improved shortcuts with Quick restart in Settings > Shortcuts, with T as its default and support for modifiers, reassignment and removal. Updated trainer/mod pairs avoid duplicate game restarts; older mod versions retain their existing T behavior.
- Improved font selection with searchable installed system/user fonts and a local Fonts folder, including supported TrueType/OpenType collection faces. Saved selections use stable identities and fall back safely when a font is unavailable.
- Improved menu navigation with task groups for Practice, Review and Suite, keeping the existing tools available. Public releases remain available without sign-in, and revoked saved credentials no longer block public update checks; Windows release checks now select anonymous or authenticated transport according to repository visibility.

### v7.20.2 Changelog

- Fixed a bug caused by the Stop seed search button being disabled while a game command awaits acknowledgement. Cancellation now stops native preparation/workers immediately, while the game stop waits for the existing command to be acknowledged.
- Fixed a bug caused by duplicate game stop commands from the UI and command loop. One search lifecycle check now sends the deferred stop exactly once and discards an older pending stop when a newer search starts.
- Improved regression coverage with the actual Stop button under pending/ready commands, both viewport sizes and DPI scales, plus cold-preparation cancellation, CPU completion, completed Unity streams, context changes and restart-before-acknowledgement. The game mod remains 7.19.0.

### v7.20.1 Changelog

- Fixed a bug caused by live seed searches also preparing a full preview route on the map worker, duplicating navigation work before the first scored seeds. Live search captures now publish their map/floor promptly and let the existing search coordinator prepare distances once.
- Fixed a bug caused by preparation progress covering only navigation rows: expensive physical repairs and pickup tables could leave an old counter such as 1/160 on screen. Preparation now publishes activity throughout, reports navigation/contact stages, and completes its full work counter.
- Improved cold navigation preparation with a bounded, thread-safe cache of exact physical segment checks, early acceptance of fully covered floor segments, and removal of repeated obstacle checks during repairs. Route geometry, clearance and pickup scoring retain their existing checks.
- Improved verification with complete live-map-to-seed-search startup, cold/warm caches, identical Top 10, preview, Stop/restart and normal map previews across all eight modes, including Timer and Hide & Seek; 80005 independent floor/segment comparisons and parallel/collision checks also run on Windows and under sanitizers. The game mod remains 7.19.0.

### v7.20.0 Changelog

- Improved regular seed search with the lossless FP64 GPU scorer validated in GPU Dev 6: Automatic compares CPU and GPU on verified Outside and Classic Bonus maps, keeps CPU bounds for long Hide & Seek searches, and preserves exact CPU solving for small tours.
- Improved search controls with Automatic / CPU / GPU selection, the active GPU/backend and calibration details, session-local geometry/settings profiles, and the measured 4096-candidate batches with bounded CPU helpers.
- Improved GPU reliability with CPU-verified startup layouts, changing per-batch samples and every published Top 10, cancellable GPU completion, and a complete same-key CPU restart after a GPU error; preview, filters, goals, pinning and exports remain available.
- Improved release verification with actual Windows production-shader comparisons, full and partial batches, both recipes/goals/accuracy modes, native coordinator and injected mid-search GPU-failure recovery tests, plus portable and sanitizer parity checks. The game mod remains 7.19.0.

### v7.19.0 Changelog

- Improved the Speedrun page with a responsive eight-mode dashboard, clean personal bests, attempts, finishes and time played. Mode details show completion rates, resets, deaths, average finishes and recent attempts; All, Clean and Practice filters keep the overview focused.
- Improved statistics persistence with bounded per-mode counters and active playtime, recorded from this update onward. Earlier personal bests and lifetime counters remain intact; menus, loading, pauses, replay playback and unfocused intervals do not add playtime. Resetting statistics preserves personal bests and saved replays.
- Improved mode selection with safe browsing, draft practice/seed settings and an explicit Start action. Short entry, hover and personal-best animations respect reduced motion, formatted statistics are cached, and the Inspector now has its own magnifying-glass icon.
- Improved verification with real mod-binary migration/save/recovery tests, dashboard interaction and DPI checks, Windows statistics checks, and documented comparisons of twelve seed-algorithm variants. None offered a reliable improvement for wide-contact Outside searches, so the verified 7.18.1 search remains in use.

### v7.18.1 Changelog

- Improved large-layout seed scoring by preparing the crystal-neighbor order once per map and filtering it for each seed. Equal-distance ties, all starting routes, optimization passes and ranking stay the same. This also reduces work when pickup-contact bounds cannot reject seeds.
- Improved pickup-contact evaluation by reusing each leg's distance and verification tables inside the solver. Continuous contacts and verification priorities are preserved.
- Improved seed-search reports: Copy results (TSV) now includes the active generator, worker count, generated/scored/cache counters and average throughput with preparation time for useful performance comparisons.

### v7.18.0 Changelog

- Improved the replay library with favorites, mode/status filters, searchable notes, recording dates and sortable columns. Favorites are local library preferences; recordings, notes and verification data are preserved.
- Improved GUI efficiency by caching replay search/display text and filtered order until the archive or controls change. Recording dates are formatted only for visible rows, and fully registered shortcuts no longer trigger unnecessary fallback polling.
- Improved trainer shortcuts: assign keys with Ctrl, Alt or Shift, remove assignments or restore defaults in Settings > Shortcuts. Conflicts and reserved combinations are explained, displayed hints follow the saved assignments, and text entry suspends shortcuts. Freecam, restart and replay playback keys keep their existing game controls.

### v7.17.0 Changelog

- Improved large-route seed searches with a stronger, bounded spanning-tree lower bound. Seeds that are provably outside the Top 10 avoid full route optimization; every seed remains generated and checked, and the route solver and ranking rules stay the same. Repeatedly ineffective refinement automatically pauses to limit its overhead.
- Improved performance verification with a repeatable Outside-geometry benchmark, pickup-radius comparisons, exact-optimum bound checks and Windows checks against frozen route orders and lengths, including 52-crystal layouts and directed input.

### v7.16.2 Changelog

- Fixed a bug caused by trying to stop Unity's logos from a mod that loads after their playback. The trainer now prepares the verified game's splash settings before launching the game process, then restores the original file after the mod connects or the trainer exits.
- Improved startup recovery with an original-file backup, complete file checks and visible diagnostics for unsupported builds or write failures. Interrupted launches are repaired on the next Suite launch; externally changed files are preserved.

### v7.16.1 Changelog

- Fixed a bug caused by applying Skip game intro only after the Unity startup logos and trainer handshake. Suite launches now pass the saved preference directly to the mod, stop the Unity splash and suppress the loader start screen before the first game update.
- Fixed a bug caused by requiring an initial scene-initialized callback to leave the intro. The first game update now checks the loaded scene directly, after the game's original startup setup, with a single menu transition.

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

