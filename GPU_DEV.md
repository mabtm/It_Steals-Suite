# PracticeSuite 7.19.0 — GPU Dev 3

Dev 3 targets maximum scoring throughput. It reuses fixed map data on the GPU,
transmits selected point IDs, and submits the next batch before CPU pickup
scoring of the preceding results. It uses the existing 7.19.0 mod and protocol.

1. Exit the running suite completely. Extract the dev ZIP into its own folder,
   start `ItStealsTrainer.exe`, and check that its title says **GPU DEV 3**.
2. Load **Practice**, open **Seed Map → GPU Dev**, and select the physical GPU.
3. Leave **Shared map data (Dev 3)** and **Overlap GPU work with CPU pickup
   scoring** enabled. Run **Batch sweep**, followed by **Endurance**, for Hide
   and Seek Outside and Classic Bonus. Keep the same scene loaded.
4. For a direct input/preparation control, repeat a profile with **Shared map
   data** and **Overlap** disabled, leaving **Reuse previous test's seed key**
   enabled. This uses the Dev 2 matrix/sort path at the same batch sizes.
5. Share the JSON files through **Open reports**. They are saved automatically
   after each paired pass and at completion, stop or error. There is no upload.

| Profile | Layouts | Rounds | Batch sizes | Paired checks |
|---|---:|---:|---|---:|
| Quick check | 1,024 | 4 | 256 | 4,096 |
| Batch sweep | 4,096 | 6 | 256, 512, 1,024, 2,048 | 98,304 |
| Endurance | 8,192 | 12 | 256, 512, 1,024, 2,048 | 393,216 |

All configurations use the same frozen selections. Full batches are warmed;
batch order rotates by round and full CPU/GPU pass order alternates. One CPU
worker supplies the reference. GPU work is not paced to preserve game FPS.
Closing the menu permits the test to continue. Scene/process changes and clean
speedruns stop it. Stop may wait for an already submitted batch to finish.

Previously, each layout needed a CPU matrix fill, neighbor sorts and 33,292
bytes of dynamic GPU input. Dev 3 flattens and uploads the fixed map distances
and global neighbor order once. Each layout then needs only 53 padded node IDs
and a count: **216 bytes**, approximately **154 times less dynamic input**.
Output buffers and the one-time catalog are separate and exported in the JSON.
The GPU gathers the FP64 matrix and filters global neighbors with a selected
point bitmask and population-count ranks. Local neighbor indices are packed
as pairs of 16-bit integers, keeping shared memory below the shader limit;
route distances and calculations remain double precision. Equal-distance ties
preserve the CPU's local-index order. Noncanonical/duplicate/out-of-range point
selections are rejected before dispatch. Verified portable RNG produces the
required distinct descending catalog IDs, with catalogs up to 1,024 points.

After a result is copied to host-owned vectors, the next batch can be submitted
before pickup scoring begins. Ordered GPU commands reuse the same buffers;
there is one pending batch and no extra staging ring. This overlaps GPU work
with CPU pickup scoring. Disabling Overlap restores serial submission/scoring.
The worker now runs at normal CPU priority. Game rendering and normal search
settings are not changed by this experiment.

Reports use schema `practice-suite.gpu-dev.v3` and live at
`%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev`. They include source
build, input mode, overlap setting, per-layout input bytes, fixed catalog bytes,
catalog setup cost, paired CPU/hybrid wall time, worker CPU time, packing,
host submission/readback, CPU pickup and GPU timeline measurements. GPU kernel
time now includes the on-GPU catalog gathering/filtering. GPU/CPU timelines may
overlap and must not be added as if they were serial costs. Hybrid wall time
includes the complete measured host path and query collection. Preparation,
warmup, verification, checkpoints and exports remain outside scoring rates.
Use **GPU timing queries** off for an instrumentation control with the same key.
Unsupported/disjoint GPU timestamps are null, with explicit validity counts.

Every layout is strictly compared: visit order, final length, pickup contacts,
completion/exact/estimate flags and raw center length. Reports retain bounded
successful examples, first failures, reproducer matrices and per-pass digests.
A zero mismatch count covers every checked layout, including omitted examples.
The normal multi-worker seed search still uses its CPU backend. This build is
a scoring-path experiment; its rates are not complete seed-search throughput.

The five physical Dev 2 reports (Ryzen 5 3600 / RX 5700 XT) had zero mismatches
in 1,081,344 timed comparisons. Endurance batch 256 reached 4,720 layouts/s
Outside (3.10x one CPU worker) and 8,438/s Classic Bonus (2.34x). CPU packing
accounted for 51–53% of hybrid wall time; GPU output copy intervals were below
0.1%. The no-query control had similar scoring rates. These results motivate
Dev 3; they do not predict its performance. The new physical-GPU reports decide
whether moving preparation to the GPU and larger batches improve total time.

The Windows probe checks both embedded shader variants, full/tail batches,
1,024-point masks, tied/directed inputs, strict route scores, legacy/catalog
parity, overlapping submission, cancellation and atomic JSON checkpoints.
Functional CI may use Microsoft's software adapter and is not a physical-GPU
performance result.

This is a separate GitHub prerelease. The stable updater remains on 7.19.0.
Close Dev 3 and launch the normal installation to return to the stable suite.
