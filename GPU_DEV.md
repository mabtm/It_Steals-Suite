# PracticeSuite 7.19.0 — GPU Dev 4

Dev 4 runs the complete benchmark sequence automatically. Select the physical
GPU, leave **Outside + Classic Bonus** and **Full comparison** selected, and
press **Start automatic test suite** in **Practice → Seed Map → GPU Dev**.
The suite loads each map in Practice, waits for a fresh verified capture, runs
all configurations, saves the reports and advances to the next map. The menu
may be closed. No individual worker/batch toggles or manual map changes are
needed. Existing 7.19.0 mod/protocol installations remain compatible.

Exit the previous suite completely before starting this dev portable build in
its own folder. Its title says **GPU DEV 4**. This is a separate prerelease;
the stable updater remains on 7.19.0.

| Profile | Seeds per search | Repetitions | Tests for both maps |
|---|---:|---:|---:|
| Quick validation | 2,048 | 1 | 56 |
| Full comparison (default) | 16,384 | 3 | 152 |

For each map, the sequence is:

1. Exhaustive production CPU reference, one worker, no Top10 pruning.
2. Production CPU searches with 1, 2, 3, 4, 5 and 6 workers.
3. GPU searches with 1–6 CPU preparation/pickup workers at each batch size:
   512, 1,024 and 2,048. There is one GPU owner/context, not six GPUs.
4. Spawn-filter validation: exhaustive reference, six-worker CPU and six-worker
   GPU, using the first 1,024 seeds (spawn #1 in the UI / index 0 in JSON).

The CPU/GPU configuration order rotates between repetitions. All runs within
a map use the same capture, search key, recipe and shortest-route objective.
The normal search settings and any user spawn filter are not copied into the
benchmark: its primary scenario explicitly tests all spawns, followed by its
separate fixed spawn-filter check. Other maps are not part of this sequence.

Reports are stored in a timestamped folder under
`%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev/Dev4-<timestamp>`.
Each completed or interrupted test receives a descriptive file name, for example
`hide-seek-outside_gpu-w6-b1024-r2-all-spawns-n16384.json`. One cumulative
**Dev4-Summary.json** contains every result and the shared map/hardware/setup
metadata. It is atomically replaced after each test and saved on completion,
stop or error. Use **Open results / Dev4-Summary.json** and share that one file.
There is no upload. A timeout, route mismatch or Top10 mismatch stops the queue
and retains earlier results and the error; it never proceeds as if validation
passed. Map capture has a three-minute timeout. Stop may wait for a submitted
GPU dispatch or worker packet. Returning to Clean mode or disconnecting stops
the suite. Captured geometry remains frozen during each map's experiments.

These are **complete searches from a prepared frozen map**, including verified
portable RNG, spawn filtering, safe MST/refined route bounds, route solving,
pickup scoring, duplicate-layout ranking, strict audits and Top10 selection.
CPU runs reuse the actual production RouteWorker implementation and its caches;
a dev-only synchronous entry bypasses IPC/preview rendering and allows an
exhaustive reference. GPU workers generate/filter/bound candidates in bounded
packets; the GPU owner returns visit orders, while CPU workers finish pickup
scoring and rank results in parallel. GPU packets have no per-worker route
cache; cache-hit and pruning counts can legitimately differ from CPU runs.
A real 64-candidate bootstrap establishes the GPU pruning cutoff; the exhaustive
reference's winners are never supplied to the pruning algorithm.

Every scored CPU/GPU result is checked against the exhaustive CPU result for
that seed index, including the selected layout, route order, final double length, pickup contacts,
completion/exact/estimate flags. GPU raw center lengths are also checked before
pickup scoring. Every completed search must produce the same ordered Top10 as
its exhaustive reference. Filter validation has its own reference. Bounds may
skip scoring candidates proven outside Top10; all candidates are still counted
as evaluated, filtered or pruned. Original frozen-score equality and ranking
rules remain unchanged. Parallel scheduling may change how much work is pruned.

Per-run JSON (`practice-suite.gpu-search.v4`) reports the source build, capture
catalog key, mode, worker/batch/round/filter settings, evaluated/generated/
scored/pruned/filtered/cache-hit/audited counts, elapsed time, seeds/s, exact
Top10, GPU timing coverage and errors. Shared map reports include game/RNG/GPU/
driver identity, geometry preparation, shader/device/catalog setup and warmup.
The summary schema is `practice-suite.gpu-suite.v4`; `plan` records the complete
request even if map acquisition fails. Buffer resizing and timing collection
are included in GPU search time; geometry/device/catalog setup and warmup are
shared and reported separately. Repetitions therefore measure warm searches.
GPU query intervals can overlap CPU work and must not be added to wall time.
Live Unity generation, game scene-loading time and preview rendering are not
part of the per-search rate. CPU worker priority follows production behavior.

This benchmark still lives in the dev test tab; the normal search's selectable
backend is unchanged. The full-search results determine whether and how to
adopt the GPU pipeline. Dev 3 scoring rates (16,822/s Outside and 30,031/s Classic
Bonus on RX 5700 XT) were not full-search rates; Dev 4 provides that missing
comparison with the production CPU worker counts.

Automated checks cover real production CPU workers 1–6, strict exhaustive score
and Top10 parity, spawn filtering, alternate objective correctness, bad GPU
outputs, cancellation and bounded queues. Windows additionally exercises the
actual embedded shaders and the automatic two-map orchestration with fixture
captures, stale-capture rejection, named reports and one summary. Software
adapters are allowed only by explicit test parameters; CI functional results
are not physical-GPU performance measurements or a live Unity navigation test.
