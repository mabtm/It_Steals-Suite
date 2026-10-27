# PracticeSuite 7.19.0 — GPU Dev 1

This is a separate prerelease build for testing the GPU order prototype inside
the suite with a frozen snapshot of the current game map. It uses the existing
7.19.0 game mod and protocol; no new mod installation is needed for an existing
7.19.0 setup.

1. Exit the running suite completely, then extract the dev portable ZIP into
   its own folder and start its ItStealsTrainer.exe. The title says GPU DEV 1.
2. Load the map in Practice, open **Seed Map → GPU Dev**, and select your GPU.
3. Press **Compare live layouts**. The test checks 32–1024 layouts in three
   alternating CPU/GPU passes. One CPU worker is used for the reference.
4. A JSON report is saved automatically, including on stop or unsupported GPU.
   Use **Open reports** to find it, or **Export JSON** to write another copy.

Reports live in `%LOCALAPPDATA%/ItStealsPracticeSuite/Diagnostics/GpuDev`.
Share the generated `gpu-dev-*.json` files for analysis. There is no automatic
upload. Reports include GPU/driver identity, source build, map/RNG descriptors,
seeds and selections, strict order/score/contact comparisons, paired timings,
reproducer matrices, and rendered-frame observations during the test.

The GPU calculates visit order; final pickup-contact scoring uses the same CPU
implementation as the normal search. The prototype currently supports modes
with 16–52 crystals and verified portable seed generation. It creates its own
D3D11 device and runs in a background thread. Closing the menu allows the test
to continue; changing the game context or starting a clean speedrun stops it.

The scoring comparisons exclude cold map preparation, generation, pruning,
comparison and JSON writing. Device/preparation time is reported separately.
The measured GPU hybrid time includes packing, upload, dispatch, readback and
CPU pickup scoring. This is a scoring-path experiment, not a full GPU search
backend or a controlled game/OBS FPS benchmark. The normal search stays on CPU.

GitHub marks this build as a prerelease. The stable updater release remains
7.19.0. To return to it, close this dev suite and start your normal installation.
