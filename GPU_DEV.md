# PracticeSuite 7.19.0 — GPU Dev 5

Dev 5 compares the Dev 4 GPU kernel with a shader specialized for the captured
crystal count. It also measures bounds enabled, disabled and adaptive. No winner
is assumed in advance; each candidate must retain the exhaustive CPU results.
This remains a separate prerelease. The normal search backend and stable updater
remain on the released CPU version.

Close the previous suite completely, extract this portable build into its own
folder and start it. The title says **GPU DEV 5**. The existing 7.19.0 mod/protocol
is compatible. Open **Practice → Seed Map → GPU Dev**, select the physical GPU,
leave the three-map/full-comparison defaults selected and press
**Start automatic test suite** once. The suite loads Outside, Classic Bonus and
Hide & Seek, waits for verified captures, runs the plan and exports automatically.
The menu can be closed during the run.

| Profile | Seeds per search | Search keys | Repetitions | Tests for three maps |
|---|---:|---:|---:|---:|
| Quick validation | 2,048 | 1 | 1 | 69 |
| Full comparison (default) | 16,384 | 3 | 3 | 549 |

Full comparison is a larger test than Dev 4. The map scope can also be limited
to Outside + Classic Bonus, or the current supported map. Maps with 15 or fewer
crystals require a separate exact GPU algorithm and are not enabled here.

## What is measured

For every map and search key:

1. Exhaustive production CPU reference, one worker, no Top10 pruning.
2. The production six-worker CPU search as the practical baseline.
3. GPU searches with two CPU preparation/pickup helpers, generic and specialized
   shaders, each at batches 512, 1,024 and 2,048 and all three bounds policies.
4. Separate spawn-filter checks: exhaustive reference, production CPU, and
   specialized GPU/adaptive bounds, using the first 1,024 seeds or fewer.

The CPU baseline and 18 GPU configurations rotate between rounds and keys.
Dev 4 already compared helper counts 1–6; Dev 5 concentrates on its measured
near-optimal two-helper setup instead of repeating that sweep. The three 64-bit
search keys are deterministic offsets of the base key; the actual keys are
stored as decimal strings. All configurations for a map/key use the same
capture, seed sequence, recipe and shortest-route objective. The usual search's
spawn filter and other search settings are not copied into these comparisons.

The generic shader retains 64 threads and capacity for 52 crystals. Specialized
shaders use the exact captured count as a compile-time constant, smaller local
arrays, and 32 threads up to 32 crystals / 64 above that. The algorithm, FP64
precision, tie-breaking, route refinement and pickup scoring stay the same.

| Crystal count | Generic declared shared bytes | Specialized declared shared bytes |
|---|---:|---:|
| 22 (Hide & Seek) | 29,112 | 5,928 |
| 40 (Classic Bonus) | 29,112 | 17,832 |
| 52 (Outside) | 29,112 | 29,112 |

These are source-declared group-local allocations, not measured speedups or
VRAM consumption. Outside's specialization fixes loop bounds but does not
reduce those arrays. Actual full-search rates decide whether a variant wins.
The device and both shaders are prepared and warmed before timed comparisons;
specialization compilation time is reported separately under `shaderPreparation`.
Generic shader compilation is included in the shared device setup time.
The GPU timestamp pool also includes the initial 64-seed bootstrap dispatch,
so the final partial batch is covered even at the smallest batch size.

Bounds **on** reproduces the existing GPU MST/refinement policy. **Off** fully
scores every unfiltered candidate. **Adaptive** evaluates 64 bounds per worker;
when fewer than two reject, it skips bounds for 512 subsequent eligible
candidates, fully scoring them, then samples again. Skipping a bound never drops
a seed. Bounds checks/skips and scored/pruned/filtered counts are exported so
both the cost and usefulness can be evaluated. The CPU baseline is unchanged.
The first 64 real candidates establish the GPU cutoff; reference winners are
never fed into the search's pruning decisions.

## One complete export

Each run folder contains exactly one **Dev5-Summary.json**, under:

`%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev/Dev5-<timestamp>`

Use **Open results / Dev5-Summary.json** and share that one file. It contains
all individual run results, full Top10 seed/layout/route data, actual keys,
mode, shader/thread/shared-memory settings, bounds policy/counters, elapsed
time, seeds/s, GPU timestamp coverage, hardware/driver/game/RNG identity and
shared preparation metadata. No individual per-test JSON files are produced by
the automatic suite. The summary is atomically replaced after checkpoints and
saved on completion, cancellation or error. There is no automatic upload.
Schemas are `practice-suite.gpu-suite.v5` and `practice-suite.gpu-search.v5`.
The nested map reports are preparation metadata, not additional search results.

All scored results are strictly audited against the exhaustive CPU result for
the same seed index, including selection, visit order, final double length,
pickup contacts and result flags. Every completed configuration must produce the
same ordered Top10 as its reference. References are rebuilt for each key and
spawn-filter validation has its own Top10 reference. Mismatches stop the queue
and preserve the results/error. A capture timeout is three minutes. Stop, Clean
mode or disconnection cancels; an already submitted GPU dispatch may finish.

These are complete searches from a prepared frozen map: generation, filtering,
bounds, scoring, audits, ranking, worker startup/join and GPU buffer setup/query
collection are timed. Shared geometry/device/shader preparation and warmup are
reported outside search time. Unity generation, scene loading and preview
rendering are excluded. CPU priority follows production behavior; GPU helpers
have normal priority. GPU intervals can overlap CPU work and must not be added
to wall time. Host readback time includes waiting for GPU work and is not a
standalone PCIe-transfer benchmark. Game FPS and OBS performance are not measured.

Checks exercise production CPU workers 1–6, all bounds modes, strict score and
Top10 parity, filtering, alternate objectives, bad outputs and cancellation.
Windows additionally compiles the actual embedded shaders for small, odd,
32-bit-mask-boundary and large counts; compares generic/specialized results;
rejects wrong-sized inputs and pending shader switches; and runs a three-map,
two-key automatic fixture plan with single-file Unicode-path exports and
immediate Clean-mode cancellation. Software adapters are permitted only by
explicit test parameters. CI checks correctness, not physical-GPU performance
or navigation through a live Unity process.
