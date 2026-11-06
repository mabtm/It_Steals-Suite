# Elite-Suite 7.25 Preview

These captures use the actual production ImGui renderer and synthetic test data, not a running game. The editor example also moves/scales the input display to keep its demonstration layout clear.

- 01-independent-splits.png: Separate bottom-center Set-Seed panel, sector and total deltas, gold-sector marker, projected finish and provisional theoretical best.
- 02-split-editor.png: Independent drag/resize placement with Undo, Cancel and Apply; timer and input panels keep separate positions.
- 03-split-settings.png: Timer / independent / both, pace and theory options, rows, positioning, size and opacity.

Open Settings > HUD > Splits / pace. The independent panel defaults to bottom center. Only Set-Seed runs display it; stale/disconnected data is hidden.

Pace uses a compatible recorded route and responds to late sectors. Different pickup order requests a matching reference; otherwise the estimate is unavailable. Theoretical best sums segment durations, including the finish sector, on the exact mode, seed recipe, spawn, layout and route. In-progress improvements are provisional. Eligible short resets and deaths retain their sectors independently of replay retention. Earlier retained valid replay splits are imported; missing historic resets cannot be reconstructed.

This remains a pre-release outside the stable updater.
