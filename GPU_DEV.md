# PracticeSuite 7.19.0 — GPU Dev 2

GPU Dev 2 measures batch scaling and longer CPU/GPU comparisons inside the
suite, using the same frozen current-map layouts throughout each test. It uses
the existing 7.19.0 game mod; an existing 7.19.0 installation needs no new DLL.

1. Exit the running suite completely. Extract this dev portable ZIP into its
   own folder and start its `ItStealsTrainer.exe`. The title says **GPU DEV 2**.
2. Load a map in **Practice**, open **Seed Map → GPU Dev**, and select your GPU.
3. Select **Batch sweep** and press **Start comparison**. After that, run
   **Endurance** on the same map. Repeat for Hide and Seek Outside and Classic
   Bonus. Each long profile can take several minutes, depending on the hardware.
4. Keep the same scene loaded. Closing the suite menu lets the test continue;
   menu and HUD frame observations are reported separately. A scene/process
   change or clean speedrun requests a stop.
5. Share the generated `gpu-dev-*.json` reports. **Open reports** opens their
   folder; **Export JSON** saves another copy. There is no automatic upload.

| Profile | Unique layouts | Rounds | Batch sizes | Strict paired comparisons |
|---|---:|---:|---|---:|
| Quick check | 1,024 | 4 | 16 | 4,096 |
| Batch sweep | 4,096 | 6 | 16, 64, 128, 256 | 98,304 |
| Endurance | 8,192 | 12 | 16, 64, 128, 256 | 393,216 |

Full-batch warmups precede timing. A 5-second idle frame baseline is recorded
for Quick check/Batch sweep; Endurance uses 10 seconds. Batch order rotates by
round to distribute warm/late runs. Every batch size receives exactly the same
seed selections. CPU/GPU full-pass order alternates, balanced across rounds.
**Reuse previous test's seed key** makes repeated profiles use the same seed
sequence; the source map remains part of the exported identity.

Reports are saved automatically to
`%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev`. One JSON checkpoint
is atomically replaced after each paired pass, and saved again at completion,
on stop, or on error. Checked partial progress is retained. All layouts are
strictly compared, including routes, final lengths, pickup contacts and raw
center lengths. Successful examples are sampled (64 plus warmups), the first
16 failures and up to 8 reproducer matrices are retained, and every compared
final route contributes to per-pass CPU/GPU digests. This bounds report memory
and file size instead of retaining hundreds of thousands of full route pairs.
The digests are FNV-1a diagnostics, not cryptographic proof; strict comparison
still runs for every layout. A zero mismatch count covers all checked layouts,
not just the retained examples.

JSON schema `practice-suite.gpu-dev.v2` includes hardware/driver/build identity,
map/RNG descriptors, setup/generation/warmup/baseline times, full per-pass CPU
and hybrid wall times, actual worker CPU time, host upload/readback times,
packing and CPU pickup times, and GPU timeline timestamps for input updates,
Dispatch and output copies. Timestamp validity counts are explicit; unsupported
or disjoint samples are null rather than zero. Partial timestamp coverage must
not be treated as the duration of a complete pass. Query collection cost is
included in hybrid wall time. The two output copies are queued before blocking
for results, avoiding the old copy/map/copy/map sequence.

Frame histograms cover the whole test, with separate idle, CPU and GPU phases
per batch size. They report mean/p50/p95/p99/max, frame counts and intervals
above 16.67/33.33 ms. Quantiles use fixed-memory 0.1 ms upper-bound buckets;
the overflow bucket uses the actual maximum. Phase measurements discard the
first transition interval. Shared game-FPS snapshots may repeat and do not
constitute independent frame samples. GPU timeline measurements can include
contention with the game. These observations are not a controlled OBS benchmark.

The GPU computes visit order; final pickup-contact scoring stays on the same
CPU implementation as the normal search. Modes with 16–52 crystals and verified
portable RNG are supported. One CPU worker supplies the reference. Map/device
preparation, generation, warmup, baseline, verification and JSON writes are
excluded from scoring rates. Hybrid time includes packing, upload, GPU work,
readback, CPU pickup scoring and timing collection. Buffer sizes match the
active batch; the largest active input/output/staging buffer set is exported
(excluding driver and query allocations). These are
scoring-path rates, not complete multi-worker seed-search rates. Upload/compute
and CPU/GPU passes remain synchronous; pipelining is a later experiment that
should be justified by these measurements.

The first physical-GPU reports (Ryzen 5 3600 / RX 5700 XT, 1,024 layouts,
3 rounds, batch 16) had zero mismatches in 6,144 timed comparisons:

| Map | CPU ms per 1,024 | Hybrid ms per 1,024 | Hybrid extra time |
|---|---:|---:|---:|
| Hide and Seek Outside (`1793111310620`) | 584.80 | 652.20 | +11.53% |
| Classic Bonus (`1793111363997`) | 262.40 | 369.49 | +40.81% |

Those reports combined transfer, dispatch and readback in one metric; they did
not establish a GPU speedup. Dev 2 tests whether larger batches change that.

GitHub marks this as a separate prerelease. The normal search and stable
updater remain on 7.19.0. To return, close this dev suite and start the normal
installation. Main and the working hands/OBS/game-rendering paths are unchanged.
