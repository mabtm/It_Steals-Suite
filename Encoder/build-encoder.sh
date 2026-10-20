#!/usr/bin/env bash
set -euo pipefail
base_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
suite_dir="$(cd -- "$base_dir/.." && pwd)"
cross_prefix="${CROSS_PREFIX:-x86_64-w64-mingw32-}"
compiler="${cross_prefix}gcc-posix"
cxx_compiler="${cross_prefix}g++-posix"
command -v "$compiler" >/dev/null || compiler="${cross_prefix}gcc"
command -v "$cxx_compiler" >/dev/null || cxx_compiler="${cross_prefix}g++"
command -v nasm >/dev/null
command -v pkg-config >/dev/null
printf '%s  %s\n' '7138d28c96d9d3e3af4ee3d8cad72741f8ffb40da90c1112235dea3ecd3178a3' "$base_dir/ffmpeg-8.1.3.tar.xz" '1108b94cd4c6943bc569afff1f8ec8c6034594a80d2baebc2b043bb438847485' "$base_dir/x264-stable.tar.gz" | sha256sum -c -
build_dir="$(mktemp -d "$base_dir/.encoder-build.XXXXXX")"
if [[ ${KEEP_BUILD:-0} != 1 ]]; then trap 'rm -rf -- "$build_dir"' EXIT; fi
tar --no-same-owner -xf "$base_dir/ffmpeg-8.1.3.tar.xz" -C "$build_dir"
tar --no-same-owner -xf "$base_dir/x264-stable.tar.gz" -C "$build_dir"
mkdir "$build_dir/x264-windows" "$build_dir/windows"
cd "$build_dir/x264-windows"
CC="$compiler" AR="${cross_prefix}ar" RANLIB="${cross_prefix}ranlib" \
  "$build_dir/x264-stable/configure" --host=x86_64-w64-mingw32 --cross-prefix="$cross_prefix" \
  --prefix="$build_dir/prefix" --enable-static --disable-cli --disable-opencl --bit-depth=8 --chroma-format=420
make -j"${JOBS:-4}"
make install-lib-static install-lib-dev
export PKG_CONFIG_LIBDIR="$build_dir/prefix/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
# windres otherwise invokes a non-POSIX compiler alias on some distributions.
cat > "$build_dir/windres" <<RC
#!/usr/bin/env bash
exec "${cross_prefix}windres" --preprocessor="$compiler" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED "\$@"
RC
chmod +x "$build_dir/windres"
cd "$build_dir/windows"
"$build_dir/ffmpeg-8.1.3/configure" \
  --arch=x86_64 --target-os=mingw32 --cross-prefix="$cross_prefix" \
  --cc="$compiler" --cxx="$cxx_compiler" --windres="$build_dir/windres" \
  --disable-everything --disable-network --disable-autodetect --disable-doc --disable-debug \
  --disable-ffplay --disable-ffprobe --disable-shared --enable-static --enable-ffmpeg \
  --enable-gpl --enable-libx264 --pkg-config=pkg-config --pkg-config-flags=--static \
  --enable-protocol=file,pipe --enable-demuxer=rawvideo,matroska,avi,mov,wav \
  --enable-decoder=rawvideo,ffv1,utvideo,pcm_f32le --enable-encoder=ffv1,utvideo,prores_ks,libx264,aac,pcm_f32le \
  --enable-muxer=matroska,avi,mov,mp4,null,wav \
  --enable-filter=buffer,buffersink,abuffer,abuffersink,aformat,anull,aresample,format,null,scale,setparams,setpts,settb --enable-swresample --enable-swscale --enable-avfilter \
  --extra-cflags="-I$build_dir/prefix/include" --extra-ldflags="-L$build_dir/prefix/lib -static"
make -j"${JOBS:-4}"
cp ffmpeg.exe "$suite_dir/bin/PracticeSuiteEncoder.exe"
python3 - "$suite_dir" <<'PY'
from pathlib import Path
import hashlib,sys
root=Path(sys.argv[1]);digest=hashlib.sha256((root/'bin/PracticeSuiteEncoder.exe').read_bytes()).hexdigest()
(root/'Trainer/EncoderIdentity.h').write_text('#pragma once\nnamespace suite_clip {inline constexpr char EncoderSha256[]="'+digest+'";}\n')
print('Encoder and matching identity updated. Rebuild the trainer before distributing both files.')
PY
