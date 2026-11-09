# Elite-Suite 7.26 Full-Game Preview

Pre-release only. Stable automatic updates remain on 7.25.0 until the owner approves promotion.

These images are captures of the actual production renderer with synthetic test data. The dark background is not a running game. Example times demonstrate the UI and are not real speedruns.

- [Run setup](01-full-game-setup.png): choose Individual level, RSFG or SSFG under Runs. SSFG accepts a separate seed and spawn recipe for each level. Browsing or editing never starts a run; Start full game launches Classic.
- [Full-game HUD](02-full-game-hud.png): loadless total, category and validity, eight level sectors, PB sectors and cumulative red/green deltas. The active level moves automatically. Position, width, scale and opacity are adjustable; hiding the table keeps the clock and validity visible.
- [Full-game result](03-full-game-result.png): final category, total, PB comparison and all eight completed sectors, including retries.
- [Replay folders](04-full-game-replays.png): expand a campaign to access its eight successful level attempts, export the set or copy an SSFG seed plan. Sector time includes failed attempts; the child replay contains the successful attempt only.

Level order: Classic → Camera → Hide & Seek → Timer → Ghost → Classic Bonus → Camera Bright → Hide & Seek Outside.

Quick restart **T** retries the current level and keeps total gameplay time. Full reset **Ctrl+Shift+T** returns to Classic at zero. Both are rebindable under Settings → Hotkeys, including **M1-M5** mouse buttons and Ctrl/Alt/Shift combinations. M1-M3 keep operating menu controls while a menu is open; M4/M5 shortcuts also work from the result screen. Deaths retry the current level, and loading time is excluded. The aggregate finishes after the final bonus level. Unexpected scene changes interrupt the campaign.

PBs, Top 5, sortable history and statistics are separate from individual-level categories. SSFG PBs match the complete eight-level seed/recipe plan. Previous PB sectors form the comparison reference; a new plan starts without a reference.

To share a finished campaign, choose **Export replay folder**. Keep its eight numbered ZIP packages and `full-ID.isfg` together. On another installation, open Replays → Import and select that `.isfg` file. The import verifies each level and preserves existing files that differ. These checks are local consistency checks, not tamper-proof proof of gameplay.

Windows game testing is still needed for real loading latency and live performance. The preview changes timing, orchestration and archive/UI handling; it preserves the existing replay camera/export rendering path.
