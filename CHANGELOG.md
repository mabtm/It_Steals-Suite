# Changelog

Selected release changes are summarized below; the original import remains in Git history.

### v7.12.1 Changelog

- Improved updates by implementing background GitHub release checks, verified downloads, transactional installation, and automatic game and trainer restart.
- Improved release access by implementing optional Windows Credential Manager storage for private builds without bundling access tokens.
- Improved the interface by implementing animated update prompts, a manual Updates workspace, and a complete offline version history after the intro.
- Fixed a bug caused by repeated scene changes showing the same update prompt; restarting the trainer starts a new notification session.
- Fixed a bug caused by version changes excluding replay packages created by the previous trainer.

### v7.12.0 Changelog

- Improved installation by implementing a guided installer with Steam detection, verified MelonLoader downloads, rollback, and branded shortcuts.
- Improved startup by implementing optional one-time game launch with existing-process detection and a manual tray action.
- Improved distribution by implementing verified Windows release packages, individual binaries, checksums, and a redesigned repository page.
- Fixed a bug caused by a version bump excluding replay packages exported by the previous trainer.

### v7.11.5 Changelog

- Removed obsolete change reports, generated galleries, backup artifacts, and redundant helper files.
- Improved maintainability by implementing consistent formatting, grouped adjacent models, compact English documentation, and one portable test runner.
- Improved the Live Inspector by implementing themed object cards, bounded animations, selection feedback, and optional interface sounds.
- Fixed a bug caused by outdated build instructions and machine-specific test paths.

### v7.11.4 Changelog

- Improved large-layout routing by implementing cached checked-path closure, faster neighbor queries, and visit relocation.
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

- Improved export throughput by implementing three bounded frame slots and overlapping GPU readback with encoding.
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

- Improved replay visibility by implementing owned fade materials while retaining original renderer batches.

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
- Improved overlay efficiency by implementing cropped panel readbacks and reusable navigation caches.

### v6.9.6 Changelog

- Improved replay analysis by implementing observed light events and shared navigation checks across modes.

### v6.9.0 Changelog

- Improved seed-search throughput by implementing verified portable generation and cached background route evaluation.

### v5.0.0 Changelog

- Improved overlays by implementing configurable HUD profiles, interpolated telemetry, and scoped menu input ownership.
