# PracticeSuite 7.19.0 — GPU Dev 6

Dev 6 tests lossless shader-memory compression and a larger search pipeline.
It first screens all shader variants, then compares the Dev 5 winner with the
fastest compressed candidate using longer searches and different search keys.
Every scored route and every final Top10 must match the exhaustive CPU reference.
This is a separate prerelease; the normal search backend is unchanged.

Close the previous suite, extract this portable build into its own folder and
start it. Its title says **GPU DEV 6**. The 7.19.0 mod/protocol remains compatible.
Open **Practice → Seed Map → GPU Dev**, select the physical graphics adapter,
leave the three-map **Intensive research** defaults selected, and press
**Start automatic test suite** once. Maps, captures, comparisons and export are
automatic. The menu may be closed. The full run is substantially longer than
Dev 5; there is no game-FPS throttle.

| Profile | Screening seeds | Stress seeds: Outside / Classic Bonus | Stress seeds: Hide & Seek | Keys per stage | Rounds | Total tests |
|---|---:|---:|---:|---:|---:|---:|
| Quick validation | 2,048 | 2,048 | 2,048 | 1 | 1 | 138 |
| Intensive research | 16,384 | 65,536 | 262,144 | 3 | 3 | 1,116 |

Totals assume all three maps. Outside + Classic Bonus or the current supported
map can also be selected. Maps with 15 or fewer crystals still require a
separate exact GPU search algorithm and are not enabled in this suite.

## Shader variants

All variants retain FP64 distances, the same route algorithm, tie-breaking,
pickup scoring and strict equality checks. Global catalog IDs remain 32-bit.
Only local neighbor indices, which are at most 51, are packed into 8-bit fields.
The triangular variants store the already-symmetric captured distance matrix
once, including its diagonal, rather than duplicating its two halves. They are
explicitly rejected for directed legacy matrix inputs.

| Variant | Distance tile | Local neighbor bits | Crystal count |
|---|---|---:|---|
| generic | full | 16 | dynamic, capacity 52 |
| fixed | full | 16 | captured count fixed at compilation |
| triangle | triangular | 16 | dynamic, capacity 52 |
| compact | triangular | 8 | dynamic, capacity 52 |
| compact-fixed | triangular | 8 | captured count fixed at compilation |

At 52 crystals the generic shader declares 29,112 shared bytes, triangle 18,088,
and compact 15,384. Compact-fixed declares 9,672 bytes at 40 crystals and 3,464
at 22. These are source-declared group-local allocations, not measured GPU
utilization, VRAM totals or guaranteed speedups. Fixed variants use 32 threads
up to 32 crystals, 64 above; dynamic variants retain 64. All five shaders are
compiled and warmed before search timings. Compilation costs are reported
separately; generic compilation is included in shared device setup time.

## Automatic plan

For each map, **screening** uses three keys and three rounds. Each key starts
with an exhaustive one-worker CPU reference. The comparisons cover six-worker
CPU bounds on/off; all five GPU variants with two CPU helpers, batch 2,048 and
bounds off; and the Dev 5 winner with bounds on and count-adaptive bounds.
There are 84 screening tests per map in the intensive profile.

The long stage always retains the Dev 5 winner: generic for Outside, fixed for
Classic Bonus and Hide & Seek. It selects one compressed candidate from triangle,
compact and compact-fixed by median complete GPU kernel time. This avoids
hiding a faster kernel behind CPU preparation/pickup bottlenecks. If any screening
off-bounds run has incomplete, invalid or unavailable GPU timestamps, every
candidate is compared using full search wall time instead. The chosen variant,
metric, sample count and baseline/candidate medians are exported under
`calibration`. A separate screening bounds recommendation compares actual full
wall times for on/off/adaptive; it is not inferred from rejection counts.
Neither recommendation silently changes the production search backend.

**Stress** uses three additional search keys that were not used for selection.
Each starts with its own exhaustive reference at the full stress count. The
comparisons are:

- Production six-worker CPU with bounds on and off.
- Baseline and selected compressed GPU: 2, 4 and 6 CPU helpers, each at batches
  1,024, 2,048, 4,096 and 8,192, with bounds off.
- Both GPU variants additionally at two helpers / batch 2,048 with bounds on
  and count-adaptive bounds, to check whether larger searches change their value.
- Spawn #1 / JSON index 0 at up to 65,536 seed candidates: exhaustive CPU
  reference, six-worker CPU, baseline GPU (2 helpers / 2,048), compressed GPU
  (6 helpers / 4,096) with bounds off and on.

These are 288 stress/filter tests per map, 372 total per map. Configuration order
rotates and reverses between rounds and shifts between keys. All comparisons
within a map/stage/key share the capture, seed sequence, recipe and objective.
The six actual 64-bit keys are exported as decimal strings. They are distinct
permutation keys, not a guarantee that no individual seed overlaps across keys.

Bounds off scores every unfiltered candidate. GPU count-adaptive bounds retain
the Dev 5 rule (64 checks; skip 512 eligible bounds if fewer than two reject) as
a control. The new timing-based recommendation is reported separately. CPU
bounds-off uses the existing dev exhaustive flag to disable pruning, while
retaining the production worker, scoring and caches. No reference winners are
provided to GPU pruning; its first 64 real candidates establish its own cutoff.
The reference score/layout arrays are held in RAM for the current map and
reused between keys; work packets remain bounded to one per CPU helper.

## One complete export

Each run folder contains exactly one **Dev6-Summary.json**:

`%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev/Dev6-<timestamp>`

Use **Open results / Dev6-Summary.json** and share that one file. No individual
per-test JSON files are produced by the automatic suite. The summary contains
all results, full Top10 seed/layout/route data, stage and actual key, shader
shape/packing/shared bytes, helper/batch/bounds settings, calibration decisions,
evaluated/scored/pruned/filtered/cache/audit counts, wall time, seeds/s, GPU timing
coverage, and shared game/RNG/hardware/driver/preparation metadata. GPU bounds
counters and CPU generation time are null where not instrumented, rather than
presented as measured zero. Schemas are `practice-suite.gpu-suite.v6` and
`practice-suite.gpu-search.v6`. Nested map reports contain preparation metadata.

The file is atomically checkpointed before/after searches and saved on completion,
stop or error. There is no automatic upload. A timeout, selection/route mismatch
or Top10 mismatch stops the queue and retains the results/error. Capture timeout
is three minutes. Stop, Clean mode or process/session disconnection cancels; an
already submitted GPU batch may finish first. Captured geometry stays frozen
through both stages. Each stage/key rebuilds its exhaustive reference; filtered
Top10 has a separate reference and uses the same key's full score/layout data.

Search times include generation, filtering, bounds, route/pickup scoring, strict
audits, Top10 ranking, worker startup/join, GPU buffer resizing and timestamp
collection. Geometry/device/shader preparation and warmup are shared outside
search time. Unity generation, scene loading and preview rendering are excluded.
CPU priority follows production; GPU helpers have normal priority. GPU intervals
can overlap CPU work and must not be added to wall time. Host readback includes
waiting for GPU work and is not a PCIe-only transfer measurement. No controlled
OBS or game-FPS benchmark is included.

Tests cover bit-preserving triangular round trips, 8/16-bit local-index packing,
1024-point catalogs and tie order; production CPU workers 1–6 and complete search
parity; larger packet limits; distinct stage keys, plan counts and calibration
including timestamp fallback. Windows tests the actual embedded shaders at small,
odd, mask-boundary and large counts, rejects compression for directed legacy
inputs, checks full/partial 8,192-layout batches and output guards, and runs the
automatic three-map/two-key-per-stage fixture plan with Unicode paths, one-file
exports and immediate Clean-mode cancellation. Software adapters are permitted
only through explicit test parameters. CI establishes correctness, not physical
GPU throughput or live Unity navigation behavior.
