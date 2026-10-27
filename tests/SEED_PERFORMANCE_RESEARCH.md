# Seed performance research — 2026-10-27

This branch contains reproducible experiments based on the 7.19.0 search.
It does not enable an experimental algorithm, GPU backend or PGO setting in
ordinary builds. A stronger mathematical bound is useful only when its saved
route evaluations outweigh its own cost.

## Measurement contract

- Use the asset-derived Outside geometry/catalog fixture, 52 crystals, a fixed
  deterministic synthetic RNG descriptor and seed order.
- Compare complete searches after the normal 128-layout warmup, including the
  timed search's remaining preparation, bounds and final route scoring. The
  warmup builds reusable map distances; initial cold-map preparation is excluded.
- Alternate variant order across three pairs. Build before timing and avoid
  concurrent work on the same benchmark host.
- Preserve the final Top 10 seed indices, route lengths and order hashes exactly;
  account for every checked/generated seed.
- A fixture result is not live Unity throughput, game FPS, input latency or OBS
  performance. Absolute rates from different hosts are not directly comparable.

The short `training` benchmark argument reduces only the warmup used by compiler
instrumentation. Timed baseline/optimized comparisons retain the regular warmup
and 10,000-seed workload.

## Contact-aware lower bounds

For the existing symmetric contact-cost model, for every selected node v and
each of its possible contact positions, take the
cheapest incident edge (m1) and the cheapest pair of edges to distinct selected
neighbors (m2). Both incident edges in m2 must use the same contact position.

With fixed spawn s and an unknown final node e, the bound is:

```text
LB = 0.5 * (m1(s) + sum(v != s, m2(v)) + min(e != s, m1(e) - m2(e)))
```

Proof: in any complete open route the spawn and final node have degree one;
every other node has degree two at a single contact. Each local minimum is no
larger than the route's incident cost. Summing those costs counts every edge
twice. Minimizing the correction over the unknown endpoint can only lower the
result. Conservative rounding allowances and nonfinite fallback keep the
experimental pruning conservative.

The degree-balance version adds eight Lagrangian rounds. The decomposition
versions divide each edge cost into a scaled pair minimum and its residual,
then combine the existing tree/dual relaxation with the contact relaxation.

Validation: 756 exact tiny joint order/contact optima, selected subsets and
nonzero spawn IDs, ties/zeros, known larger star cases through 255 goals, mask
boundary 255, cancellation, and costs from 1e-100 to 1e100. Both the normal
suite and AddressSanitizer/UndefinedBehaviorSanitizer passed.

Local paired 6,000-seed searches, one worker, radius 1.35:

| Variant | Median seeds/s | Scored | Pruned |
| --- | ---: | ---: | ---: |
| reference | 1583.022 | 6000 | 0 |
| degree | 1526.694 | 6000 | 0 |
| dual | 1055.552 | 5746 | 254 |
| residual | 1524.192 | 6000 | 0 |
| quarter | 1446.425 | 6000 | 0 |
| half | 1406.613 | 6000 | 0 |
| three-quarter | 1449.316 | 6000 | 0 |

All 21 searches retained the reference Top 10 and seed coverage. Degree and
decomposition bounds did not eliminate route evaluations in this workload.
The stronger dual bound avoided 254 evaluations, but its overhead made the
complete search substantially slower. None is a production winner here.
A separate CI replication (run 40491195866) produced the following medians:

| Variant | Median seeds/s |
| --- | ---: |
| reference | 1864.314 |
| degree | 1841.133 |
| dual | 1329.116 |
| residual | 1880.340 |
| quarter | 1840.180 |
| half | 1821.610 |
| three-quarter | 1832.385 |

The residual variant's ratio of aggregate medians is +0.86%, but its three
paired changes were -0.99%, -0.41%, +0.86%. It eliminated no additional route
scores. This is not a consistent gain and does not justify adoption. The dual
variant remained about 29% slower in CI. Individual logged samples are retained
in the permanent CSV; the complete JSON is in the run artifact.

Reproduce on Linux:

```sh
python3 tests/run.py --only contact_bound_tests gpu_order_kernel_tests route_bound_tests physical_seed_tests
python3 tests/run.py --sanitize --only contact_bound_tests gpu_order_kernel_tests --jobs 1
python3 tests/contact_bound_experiments.py --variants reference degree dual residual quarter half three-quarter --count 6000 --pairs 3
```


Long-search confirmation: 60,000 seeds, four workers, radius 1.35, three
alternating pairs on AMD EPYC 9V74 (GCC 13.3, affinity CPUs 0–4):

| Variant | Median seeds/s | Scored range |
| --- | ---: | ---: |
| reference | 6302.546 | 59992–59998 |
| degree | 6233.826 | 59994–59995 |
| dual | 4495.720 | 51532–51535 |

All nine searches retained the exact Top 10 and accounted for every seed.
The degree variant's paired changes were -4.49%, -1.79%, +2.93%, so it has no
consistent net gain. The dual variant avoided about 14.1% of scores but remained
16–29% slower within each pair. Thread scheduling changes when a shared cutoff
is published, so small differences in pruning counts are expected; final
coverage and results remain the comparison contract.

Only copies of production headers under build/ are modified by the experiment
driver. Source hashes accompany its JSON results.

## MSVC profile-guided optimization

PGO trains the compiler's branch layout and inlining decisions without changing
the search's mathematical model. The experiment builds baseline, instrumented
and optimized binaries using MSVC v143, /O2, /GL, /LTCG and the static C++ runtime.
Training includes Outside radii 1.35, 0.5 and 0, and Inside with 22 crystals.

Three earlier standalone search-binary comparisons used the longer training
schedule:

| Actions run | Radius 1.35 | Radius 0.5 | Radius 0 |
| --- | ---: | ---: | ---: |
| [40483062268](https://github.com/mabtm/It_Steals-Suite/actions/runs/40483062268) | +7.42% | +3.71% | -6.12% |
| [40485926248](https://github.com/mabtm/It_Steals-Suite/actions/runs/40485926248) | +21.62% | +14.61% | +0.09% |
| [40489905924](https://github.com/mabtm/It_Steals-Suite/actions/runs/40489905924) | +6.71% | +3.22% | -4.16% |
| 40491195866 (shorter training) | +5.30% | +4.49% | -3.94% |

Each value compares medians of three alternating pairs of 10,000-seed complete
searches on the same host. All 18 final Top 10 fingerprints matched in each
run. The second host reported AMD EPYC 9V45; the shorter-training replication used
AMD EPYC 7763. The optimized import check passed. These results show that the gain
depends on both hardware and workload; a standalone binary cannot establish
the gain in the complete trainer.

The `-Trainer` experiment therefore embeds the same benchmark entry in the
actual trainer image behind the opt-in `SuitePgoResearch` flag, trains controlled
launcher/GUI paths, and repeats the paired searches before testing the optimized
launcher image. These checks exercise controlled startup/shutdown, not visual
interaction with every tab or a live Unity/OBS session. Ordinary builds do not expose that entry. The optimized link reuses
the instrumented /GL objects so a clean step cannot erase the PGD. An import
check rejects a remaining PGORT dependency.

The first complete-image comparison (run 40491195866, AMD EPYC 9V45) measured
+8.29% at radius 1.35, +6.82% at 0.5 and **-9.69% at radius 0**. All 18 Top 10
fingerprints and checked/generated counts matched. Both instrumented and
optimized launcher smoke checks passed; the final image had no PGORT import.
This is not an acceptable general release setting.

That run's linker merged only the four search profiles: the launcher's copied
executables wrote their startup/shutdown profiles into a temporary folder that
the smoke test subsequently removed. The experiment now redirects these files
with [VCPROFILE_PATH](https://learn.microsoft.com/en-us/cpp/build/environment-variables-for-profile-guided-optimizations?view=msvc-170)
and requires all six normal-exit profiles before linking. The next complete
image experiment also increases radius-0 training from 128 to 8192 seeds.
That first balanced-training attempt (40492735156, Intel Xeon Platinum 8573C)
timed out in the first Outside training invocation, before producing comparison
samples. It is not performance evidence for the balanced profile.

The final experiment uses the compiler's documented NOEXACT profiling counters
to avoid synchronized-counter overhead. Those counters guide compilation;
the search arithmetic and exact result/coverage checks are unchanged. Standard
release builds still do not enable PGO. The final complete-image run **40494231974** passed, including all six retained
launcher profiles (ten PGC files merged), exact Top 10 parity and complete seed
coverage in all 18 searches. The optimized image had no PGORT import. Both
instrumented and optimized Windows launcher/native-contract checks passed.

| Radius | Baseline median seeds/s | PGO median seeds/s | Change |
| --- | ---: | ---: | ---: |
| 1.35 | 2187.302 | 2211.967 | +1.13% |
| 0.5 | 3608.731 | 3454.506 | -4.27% |
| 0 | 16379.692 | 15211.228 | -7.13% |

CPU: AMD EPYC 9V74. The three paired changes at radius 1.35 were +1.13%,
+25.09%, -0.29%; the aggregate median gain is small and inconsistent.
The narrower-radius and point-only cases were slower in every pair.
Consequently this corrected training configuration is also **not adopted**.

The final standalone NOEXACT run, on the same CPU model, measured +5.75%,
+3.96%, -2.03% for the three radii. It likewise does not demonstrate a universal
improvement. A standalone gain must not be presented as a complete-trainer gain.

Reproduce on Windows with Visual Studio C++ tools:

```powershell
.\tests\algorithm_windows_lab.ps1
.\tests\algorithm_windows_lab.ps1 -Trainer -CenterTrainingSeeds 8192
```

## FP64 GPU order-kernel prototype

The shared C++/HLSL kernel reproduces the production order solver's multi-start
nearest-neighbor, 2-opt and relocation rules, including deterministic tie order.
It supports 1–52 goals and uses double precision and strict IEEE compilation.
This is the order-solving stage, not a complete GPU implementation of map
preparation, contact-aware scoring, seed generation or the search scheduler.

- 440 shared-kernel/production comparisons passed with GCC and MSVC, including
  directed/nonmetric costs, ties, the 32-bit mask boundary and values close to
  the 1e-6 decision threshold.
- The compiled shader passed 45 order/length comparisons and output guard checks.
- The observed CI adapter was **Microsoft Basic Render Driver**
  (vendor 0x1414, device 0x008c). D3D11's creation type said hardware and the
  DXGI software flag was false. Neither establishes a physical graphics card.
- No speedup on a physical user GPU or during game/OBS rendering has been
  measured. FP64 support is queried explicitly; unsupported execution returns 77.
- No GPU backend is enabled in the trainer.

Reproduce:

```powershell
.\tests\gpu_order_windows_lab.ps1
# With the artifact files in the same directory:
.\gpu_order_windows_lab.exe .\gpu_order_lab.hlsl .\result.json --hardware
```

`--hardware` restricts the D3D11 creation type. Inspect the adapter description
and IDs in addition to that type before interpreting a result as physical GPU
performance.

## Recorded runs

The [research workflow](../.github/workflows/seed-research.yml) retains raw JSON
and logs for 14 days. Permanent rate snapshots and final decisions are below.

- [Earlier standalone rates](research/2026-10-27/pgo-earlier-rates.csv), extracted from the corresponding Actions logs.
- [CI mathematical comparisons](research/2026-10-27/math-ci-40491195866.json), including raw result lines and source hashes.
- [60,000-seed comparisons](research/2026-10-27/math-long-60000.json), including the benchmark host and source hashes.
- [Standalone PGO, run 40491195866](research/2026-10-27/pgo-standalone-40491195866.json).
- [Standalone PGO, run 40492735156](research/2026-10-27/pgo-standalone-40492735156.json).
- [Complete-trainer PGO, run 40491195866](research/2026-10-27/pgo-trainer-40491195866.json).
- [Complete-trainer corrected profile](research/2026-10-27/pgo-trainer-40494231974.json) and [standalone NOEXACT comparison](research/2026-10-27/pgo-standalone-40494231974.json).
- [Final workflow run](https://github.com/mabtm/It_Steals-Suite/actions/runs/40494231974): all four jobs passed.
- [GPU correctness metadata](research/2026-10-27/gpu-40494231974.json); the compiled Windows prototype is in [the GPU artifact](https://github.com/mabtm/It_Steals-Suite/actions/runs/40494231974/artifacts/12730496337) (14-day retention).

Final decision: retain the stable 7.19.0 search and normal release settings.
The tested bounds and PGO configurations have no reliable general net advantage.
The research stays on this branch; no experimental runtime change or updater
release is published. The GPU prototype needs physical-hardware and complete
pipeline measurements before an integration decision.

These results do not establish a global optimum. They identify which specific
approaches did not earn their complexity in the measured workloads, while
preserving reproducible experiments for later hardware or algorithm work.
