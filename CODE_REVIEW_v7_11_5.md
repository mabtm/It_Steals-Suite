# Code review 7.11.5 / mab

UI-only release on 7.11.4. No mutation of Unity objects/materials/colliders, no new hooks or capture passes, no IPC/recording format change. Existing token-bound picker and physical/visual selection are retained.

Presentation work is bounded: animation states use the existing motion table; changed-value storage is capped at 1024 entries and refreshed only for new detail revisions. Selection impulses expire; telemetry and hover generate no sound. Audio uses the existing single private worker with a finite, clamped request gain. Pending Inspector cues expire after 180 ms, respect global/local mute and are discarded when focus/run gates reject playback. Brief post-close cues use the existing focus-gated policy window rather than a new thread.

The object card remains clickable immediately while its presentation fades. Detail reveal uses ordinary ImGui layout and clipping, with disabled closing controls and reversible animation. Explicit selection/detail resets get fresh animation identity. Popups are constrained to the display; Escape consumes the popup before returning to the game. Object/session identity resets the observation cache; Inspector tools remain locked in clean Speedrun/Replay.

Limitations: hover and selection corner frames still describe the existing projected object bounds, not pixel-accurate silhouettes. Actual collider geometry is separately unchanged. Panel translucency is composited normally; this release adds no continuous screenshot/blur pass. Sound adapter tests use WinMM doubles; no actual Windows/Unity test was available.
