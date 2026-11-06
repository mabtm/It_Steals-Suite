# Elite-Suite 7.25 Preview

Captures of the actual production renderer with synthetic test data. The background grid demonstrates transparency; it is not a running game. The editor input display is resized/repositioned for this example.

- 01-independent-splits.png: Default view, only the latest crystal's signed delta as text. No background, border, label or historical list. Pace/theory are off by default.
- 02-split-editor.png: Drag and resize the independent text display, with Undo, Cancel and Apply. The edit frame appears only in the editor.
- 03-split-settings.png: Transparent text or compact panel; comparison basis, text size, colors, opacity, fade/pop/slide, and optional duration or hold-until-next-crystal.
- 04-compact-panel.png: Optional compact panel, still only the latest crystal. Pace and theoretical best are explicitly enabled in this example.

Open Settings > HUD > Splits / pace. The independent display defaults to bottom center. Choose crystal delta vs reference, sector delta vs reference, or sector delta vs the best observed sector. Old on-screen split lists are not retained; history remains available in replay analysis.

Pace is an estimate from a compatible recorded route, adjusted at pickups and overdue sectors. A route change searches for a compatible reference. Theoretical best sums clean segment durations, including the finish sector, on the exact mode/seed/recipe/spawn/layout/route. Live improvements are provisional. Eligible short resets and deaths persist independently of replay retention. Earlier retained clean replays are backfilled; missing old reset timestamps cannot be reconstructed.

Pre-release only; excluded from stable automatic updates.
