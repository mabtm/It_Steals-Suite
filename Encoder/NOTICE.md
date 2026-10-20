# PracticeSuite video encoder 7.8.0

`bin/PracticeSuiteEncoder.exe` is a separate minimal Windows x64 **FFmpeg 8.1.3** executable, statically linked to **x264 stable (core 165)**. This encoder build is licensed under the **GNU GPL version 2 or later**. See `COPYING.GPLv2`, `COPYING.x264`, and the complete corresponding unmodified source archives in this directory. No nonfree configuration is enabled. FFmpeg source retains its own component notices, including `COPYING.LGPLv2.1`.

Enabled video encoders: FFV1, UtVideo, ProRes KS, libx264. Enabled audio encoders: AAC and Float-PCM, with bounded WAV-based recorded-audio input and resampling filters. Local file/pipe protocols, rawvideo/Matroska/AVI/MOV/WAV demuxers, rawvideo/FFV1/UtVideo/Float-PCM decoders, Matroska/AVI/MOV/MP4/null/WAV muxers, and format/scale/setparams/setpts/settb filters support streaming and two-pass compression. Network, device capture, external codec autodetection, ffplay and ffprobe are disabled. This program receives already composed replay frames or reads a private lossless cache. It does not capture game, desktop, audio or inputs.

The trainer runs the encoder as a separate child through local pipes/files; it is not linked to the FFmpeg/x264 libraries. The Windows Media Foundation H.264 profile remains available. GIF/PNG use the suite writers. Checksum validation, restricted inherited handles and a Windows Job Object provide local integrity and cleanup, not tamper-proof run verification.

## Corresponding source and rebuild

- FFmpeg: https://ffmpeg.org/releases/ffmpeg-8.1.3.tar.xz ; SHA-256 `7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3`.
- x264 stable: https://code.videolan.org/videolan/x264/-/archive/stable/x264-stable.tar.gz ; bundled snapshot SHA-256 `1108b94cd4c6943bc569afff1f8ec8c6034594a80d2baebc2b043bb438847485`.
- Shipped Windows encoder SHA-256 `3ad27ac545b7602bf34f231cba25929144ed8b011fda137e7227e132376f6bd4`.

The FFmpeg archive signature and release key are included; release-key fingerprint `FCF986EA15E6E293A5644F10B4322F04D67658D8`. x264's mutable branch URL is informational; rebuild from the bundled hash-pinned archive.

On Linux/WSL install MinGW-w64 POSIX GCC/G++, binutils, NASM, make, pkg-config, tar and Python 3. Run `./Encoder/build-encoder.sh`, then rebuild the trainer with `build.ps1`. The script creates separate x264/FFmpeg build directories, builds static 8-bit 4:2:0 x264 with SIMD, and updates `Trainer/EncoderIdentity.h`. Redistribute the new trainer, encoder, this directory and corresponding source together. Compiler/toolchain/path differences can change the binary hash; matching source/configuration does not promise a byte-identical executable. The shipped binary was built using MinGW-w64 GCC 13 (POSIX runtime) with the same enabled components.

7.8.0 added WAV/Float-PCM input and Float-PCM/AAC output. Game audio is processed from a user recording; the suite contains no original audio assets. Video and audio muxing use the same separately bundled encoder.
