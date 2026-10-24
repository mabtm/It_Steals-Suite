import argparse
import hashlib
import json
import math
import shlex
import shutil
import struct
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(
    description="Validate new recorded-audio exports with a matching FFmpeg build and independent ffprobe/ffmpeg."
)
parser.add_argument(
    "fixtures", type=Path, help="fresh output folder from replay_experience_tests.cpp"
)
parser.add_argument("ffmpeg", type=Path, help="encoder built from Encoder sources")
parser.add_argument(
    "--avi-only",
    action="store_true",
    help="check 1080p60 and 1080p120 exports with recorded audio",
)
args = parser.parse_args()
ff = args.ffmpeg.resolve()
out = args.fixtures.resolve()


def chunks(data, start, end):
    while start + 8 <= end:
        tag, size = struct.unpack_from("<4sI", data, start)
        body = start + 8
        yield tag, body, size
        start = body + size + (size & 1)


def wave_data(path):
    data = path.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE", path
    return next(
        data[body : body + size]
        for tag, body, size in chunks(data, 12, len(data))
        if tag == b"data"
    )


def verify_clock(plan):
    width, height, fps, frames = (
        plan[key] for key in ("width", "height", "fps", "frames")
    )
    assert (width, height) == (1920, 1080)
    assert (fps, frames) in ((60, 653), (120, 587))
    label = f"clock-1080p{fps}"
    samples = round(frames * 48000 / fps)
    pcm = b"".join(
        struct.pack("<ff", math.sin(n * 0.037) * 0.25, ((n % 997) / 997 - 0.5) * 0.3)
        for n in range(samples)
    )
    wave = out / f"{label}-audio.wav"
    wave.write_bytes(
        struct.pack(
            "<4sI4s4sIHHIIHH4sI",
            b"RIFF",
            36 + len(pcm),
            b"WAVE",
            b"fmt ",
            16,
            3,
            2,
            48000,
            48000 * 8,
            8,
            32,
            b"data",
            len(pcm),
        )
        + pcm
    )
    final = out / f"{label}.avi"
    command = [
        str(ff),
        *shlex.split(plan["input"]),
        "-i",
        str(wave),
        "-map",
        "0:v:0",
        "-map",
        "1:a:0",
        *shlex.split(plan["output"]),
        str(final),
    ]
    hashes = []
    with (out / f"{label}-encoder.log").open("wb") as log:
        process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=log
        )
        try:
            for n in range(frames):
                # Moving bars exercise spatial detail as well as exact frame order.
                row = bytes(
                    value
                    for x in range(width)
                    for value in (
                        (x + n) & 255,
                        (x // 4 + (n >> 8)) & 255,
                        (x // 16 + n * 37) & 255,
                        255,
                    )
                )
                frame = row * height
                hashes.append(hashlib.md5(frame).hexdigest())
                process.stdin.write(frame)
            process.stdin.close()
            assert process.wait(timeout=120) == 0, (
                out / f"{label}-encoder.log"
            ).read_text()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    with final.open("rb") as file:
        header = file.read(1024 * 1024)
    assert header[:4] == b"RIFF" and header[8:12] == b"AVI "
    streams = []

    def scan(start, end):
        for tag, body, size in chunks(header, start, end):
            if tag == b"LIST" and header[body : body + 4] != b"movi":
                scan(body + 4, min(body + size, end))
            elif tag == b"strh":
                scale, rate, first, count = struct.unpack_from(
                    "<IIII", header, body + 20
                )
                streams.append((header[body : body + 4], scale, rate, first, count))

    scan(12, len(header))
    video = next(s for s in streams if s[0] == b"vids")
    audio = next(s for s in streams if s[0] == b"auds")
    assert video[2] / video[1] == fps and video[3] == 0 and video[4] == frames, video
    assert audio[3] == 0 and audio[4] * audio[1] / audio[2] == samples / 48000, audio
    decoded = out / f"{label}-decoded.wav"
    subprocess.run(
        [
            str(ff),
            "-hide_banner",
            "-nostdin",
            "-loglevel",
            "error",
            "-y",
            "-xerror",
            "-i",
            str(final),
            "-map",
            "0:a:0",
            "-c:a",
            "pcm_f32le",
            "-f",
            "wav",
            str(decoded),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    assert wave_data(decoded) == pcm, (
        "Recorded PCM changed or the audio track was truncated"
    )
    independent = bool(shutil.which("ffprobe") and shutil.which("ffmpeg"))
    if independent:
        probe = json.loads(
            subprocess.check_output(
                [
                    "ffprobe",
                    "-v",
                    "error",
                    "-select_streams",
                    "v:0",
                    "-count_frames",
                    "-show_streams",
                    "-show_packets",
                    "-show_entries",
                    "stream=avg_frame_rate,nb_read_frames:packet=pts_time,duration_time",
                    "-of",
                    "json",
                    str(final),
                ]
            )
        )
        assert probe["streams"][0]["avg_frame_rate"] == f"{fps}/1"
        assert int(probe["streams"][0]["nb_read_frames"]) == frames
        packets = probe["packets"]
        assert len(packets) == frames
        for n, packet in enumerate(packets):
            assert abs(float(packet["pts_time"]) - n / fps) < 0.000001
            assert abs(float(packet["duration_time"]) - 1 / fps) < 0.000001
        decoded_hashes = subprocess.check_output(
            [
                "ffmpeg",
                "-v",
                "error",
                "-xerror",
                "-i",
                str(final),
                "-map",
                "0:v:0",
                "-pix_fmt",
                "bgra",
                "-f",
                "framemd5",
                "-",
            ]
        ).decode()
        actual = [
            line.rsplit(",", 1)[1].strip()
            for line in decoded_hashes.splitlines()
            if not line.startswith("#")
        ]
        assert actual == hashes, (
            "RGB frames were repeated, skipped, changed, or reordered"
        )
    report = {
        "frames": frames,
        "fps": fps,
        "width": width,
        "height": height,
        "seconds": frames / fps,
        "audioFrames": samples,
        "audioRate": 48000,
        "pcmExact": True,
        "independentFrameDecode": independent,
        "bytes": final.stat().st_size,
    }
    return report


if args.avi_only:
    plans = json.loads((out / "clock-arguments.json").read_text())
    if isinstance(plans, dict):
        plans = [plans]
    reports = []
    for plan in plans:
        report = verify_clock(plan)
        reports.append(report)
        print(json.dumps(report), flush=True)
    (out / "verified-export-clock.json").write_text(json.dumps(reports, indent=2))
    raise SystemExit(0)

audit = []
raw = bytes(((x // 4) * 7 + (x % 4) * 47) & 255 for x in range(320 * 180 * 4))
raw = raw * 30
for fmt, ext in [(2, "mkv"), (4, "avi"), (5, "mov"), (6, "mp4")]:
    v = out / ("video." + ext)
    final = out / ("mixed." + ext)
    args = shlex.split((out / f"format-{fmt}.args").read_text())
    subprocess.run(
        [str(ff), *args, str(v)],
        input=raw,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    codec = ["-c:a", "aac", "-b:a", "128k"] if fmt == 6 else ["-c:a", "pcm_f32le"]
    subprocess.run(
        [
            str(ff),
            "-hide_banner",
            "-nostdin",
            "-loglevel",
            "error",
            "-y",
            "-xerror",
            "-i",
            str(v),
            "-i",
            str(out / "audio-1x.wav"),
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:v",
            "copy",
            *codec,
            str(final),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    probe = json.loads(
        subprocess.check_output(
            [
                "ffprobe",
                "-v",
                "error",
                "-count_frames",
                "-show_streams",
                "-show_format",
                "-of",
                "json",
                str(final),
            ]
        )
    )
    vs = next(s for s in probe["streams"] if s["codec_type"] == "video")
    a = next(s for s in probe["streams"] if s["codec_type"] == "audio")
    assert vs["avg_frame_rate"] == "60/1" and int(vs["nb_read_frames"]) == 27, (fmt, vs)
    assert int(a["sample_rate"]) == 48000 and int(a["channels"]) == 2
    decoded = subprocess.check_output(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(final),
            "-map",
            "0:a:0",
            "-f",
            "f32le",
            "-acodec",
            "pcm_f32le",
            "pipe:1",
        ]
    )
    expected = (out / "audio-1x.wav").read_bytes()[44:]
    if fmt != 6:
        assert decoded == expected, (fmt, len(decoded), len(expected))
    subprocess.run(
        ["ffmpeg", "-v", "error", "-xerror", "-i", str(final), "-f", "null", "-"],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    audit.append(
        {
            "format": fmt,
            "videoFrames": vs["nb_read_frames"],
            "fps": vs["avg_frame_rate"],
            "audio": a["codec_name"],
            "audioRate": a["sample_rate"],
            "pcmExact": fmt != 6,
            "bytes": final.stat().st_size,
            "duration": probe["format"]["duration"],
        }
    )
# Repeat the production two-pass job with the audio bit budget included.
cap = 256000
seconds = 0.45
bitrate = int((cap - 65536) * 8 / seconds - 128000)
target = out / "size-limit-audio.mp4"
for pass_id in (1, 2):
    args = [
        str(ff),
        "-hide_banner",
        "-nostdin",
        "-loglevel",
        "error",
        "-y",
        "-xerror",
        "-i",
        str(out / "video.avi"),
    ]
    if pass_id == 2:
        args += [
            "-i",
            str(out / "audio-1x.wav"),
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:a",
            "aac",
            "-b:a",
            "128k",
        ]
    else:
        args += ["-map", "0:v:0", "-an"]
    args += [
        "-c:v",
        "libx264",
        "-preset",
        "medium",
        "-b:v",
        str(bitrate),
        "-pass",
        str(pass_id),
        "-passlogfile",
        str(out / "size-pass"),
        "-frames:v",
        "27",
        "-r",
        "60",
        "-enc_time_base",
        "1:60",
        "-pix_fmt",
        "yuv420p",
        "-threads",
        "4",
    ]
    args += (
        ["-f", "null", "/dev/null"]
        if pass_id == 1
        else ["-t", "0.450000", "-movflags", "+faststart", "-f", "mp4", str(target)]
    )
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
probe = json.loads(
    subprocess.check_output(
        [
            "ffprobe",
            "-v",
            "error",
            "-count_frames",
            "-show_streams",
            "-show_format",
            "-of",
            "json",
            str(target),
        ]
    )
)
assert len(probe["streams"]) == 2 and target.stat().st_size <= cap
assert probe["streams"][0]["nb_read_frames"] == "27"
assert abs(float(probe["format"]["duration"]) - 0.45) < 0.001
audit.append(
    {
        "preset": "size-limit",
        "bytes": target.stat().st_size,
        "cap": cap,
        "audio": probe["streams"][1]["codec_name"],
        "frames": probe["streams"][0]["nb_read_frames"],
        "duration": probe["format"]["duration"],
    }
)
(out / "verified-audio-exports.json").write_text(json.dumps(audit, indent=2))
print(json.dumps(audit))
