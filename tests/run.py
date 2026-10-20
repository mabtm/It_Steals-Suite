#!/usr/bin/env python3
"""Build and run portable production-logic tests without the game SDK."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


MANAGED = {
    "fullbright_tests": ["SceneBrightness.cs"],
    "replay_renderer_tests": ["ReplayRendererScope.cs", "ReplayPlaybackClock.cs"],
    "replay_visual_tests": [
        "ReplayColliders.cs",
        "ReplayColliderCapture.cs",
        "ColliderWireGeometry.cs",
        "Replay3DOverlay.cs",
        "ReplayTrailVisual.cs",
        "ReplayWireVisual.cs",
        "ReplayFollowOrbit.cs",
        "ReplayCoverage.cs",
        "ReplayExperienceData.cs",
        "ReplayFadeMaterial.cs",
        "ReplaySceneVisibility.cs",
        "ReplayAudioFile.cs",
    ],
    "replay_audio_capture_tests": ["ReplayAudioCapture.cs"],
    "clip_channel_tests": [
        "ReplayClipChannel.cs",
        "ClipGpuReadback.cs",
        "ClipPixelLayout.cs",
        "ClipCameraPose.cs",
    ],
}


def run_logged(command, log, cwd, env=None, timeout=600):
    with log.open("w", encoding="utf-8") as output:
        output.write("Command: " + " ".join(map(str, command)) + "\n\n")
        output.flush()
        return subprocess.run(
            list(map(str, command)),
            cwd=cwd,
            env=env,
            stdout=output,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        ).returncode


def arguments(name, work, fixtures):
    pairs = {
        "planner_tests": [work, fixtures / "map-wire.bin"],
        "seed_search_tests": [
            fixtures / "seed-batch-wire.bin",
            fixtures / "seed-order-wire.bin",
        ],
        "portable_search_tests": [fixtures],
        "settings_tests": [work],
        "clip_camera_tests": [work],
        "clip_tests": [work],
        "clip_speed_tests": [work],
        "export_profile_tests": [work],
        "physical_route_tests": [fixtures / "physical-map-v13.bin"],
        "movement_native_tests": [fixtures / "movement-wire.bin"],
        "inspector_tests": [fixtures, work],
        "replay_motion_tests": [fixtures, work],
        "replay_experience_tests": [fixtures / "experience", work],
        "replay_health_tests": [fixtures / "experience", fixtures / "input-v5.isinput"],
        "replay_tests": [
            fixtures / "review-wire.bin",
            work,
            fixtures / "outside-review-wire.bin",
            fixtures / "missed-flash-wire.bin",
        ],
        "hide_seek_player_tests": [
            fixtures / "checked-player-navigation-wire.bin",
            work,
            fixtures / "hide-seek-floor.assetmesh",
            fixtures / "hide-seek-outside-floor.assetmesh",
        ],
        "routing_reliability_tests": [
            fixtures / "hide-seek-boxes.routefixture",
            fixtures / "hide-seek-outside-boxes.routefixture",
        ],
        "ui_tests": [work / "draw"],
    }
    return pairs.get(name, [])


def execute(name, managed, args, root, output, fixtures):
    started = time.monotonic()
    folder = output / name
    folder.mkdir(parents=True, exist_ok=True)
    work = folder / "work"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    executable = folder / (name + (".exe" if managed or os.name == "nt" else ""))
    result = {"name": name, "managed": managed, "passed": False}
    try:
        if managed:
            sources = [root / "tests" / (name + ".cs")]
            sources += [root / "Mod" / source for source in MANAGED[name]]
            legacy = root / "Mod/ReplayRestoreScope.cs"
            if name == "replay_renderer_tests" and legacy.exists():
                sources.append(legacy)
            command = [
                "mcs",
                "-langversion:7.2",
                "-optimize+",
                "-out:" + str(executable),
                *sources,
            ]
            if name == "clip_channel_tests":
                runs = [
                    ["mono", executable, mode]
                    for mode in (
                        "bgra-top",
                        "bgra-bottom",
                        "rgba-top",
                        "rgba-bottom",
                        "unsupported",
                    )
                ]
            else:
                runs = [["mono", executable]]
        else:
            flags = ["-std=c++20", "-pthread", "-O0" if name == "ui_tests" else "-O2"]
            if args.sanitize:
                flags = [
                    "-std=c++20",
                    "-pthread",
                    "-O1",
                    "-g",
                    "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer",
                ]
                if sys.platform.startswith("linux"):
                    flags += ["-fno-pie", "-no-pie"]
            stubs = {
                "hud_adapter_tests": "hud_stubs",
                "intro_adapter_tests": "intro_stubs",
                "interface_audio_tests": "audio_stubs",
            }
            if name in stubs:
                flags += ["-I" + str(root / "tests" / stubs[name])]
            flags += ["-I" + str(root / "Trainer"), "-I" + str(root / "Trainer/imgui")]
            sources = [root / "tests" / (name + ".cpp")]
            if name == "ui_tests":
                sources += [
                    root / "Trainer/imgui" / (unit + ".cpp")
                    for unit in (
                        "imgui",
                        "imgui_draw",
                        "imgui_tables",
                        "imgui_widgets",
                    )
                ]
                shutil.copytree(fixtures, work / "managed-data")
                shutil.copytree(fixtures / "experience", work / "experience")
                (work / "draw").mkdir()
            command = [args.compiler, *flags, *sources, "-o", executable]
            runs = [[executable, *arguments(name, work, fixtures)]]
        result["command"] = list(map(str, command))
        result["buildExitCode"] = run_logged(command, folder / "build.log", root)
        if result["buildExitCode"]:
            raise RuntimeError("Compilation failed; see " + str(folder / "build.log"))
        environment = os.environ.copy()
        if args.sanitize:
            environment.setdefault("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1")
            environment.setdefault("UBSAN_OPTIONS", "halt_on_error=1")
        result["runs"] = []
        for index, command in enumerate(runs):
            code = run_logged(
                command, folder / f"run-{index}.log", root, environment, timeout=300
            )
            result["runs"].append(
                {"command": list(map(str, command)), "exitCode": code}
            )
            if code:
                raise RuntimeError(
                    "Test failed; see " + str(folder / f"run-{index}.log")
                )
        result["passed"] = True
    except (OSError, subprocess.SubprocessError, RuntimeError) as error:
        result["error"] = str(error)
    result["seconds"] = round(time.monotonic() - started, 2)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--ui", action="store_true", help="Include production ImGui interactions"
    )
    parser.add_argument(
        "--managed", action="store_true", help="Include C# contract adapters (mcs/mono)"
    )
    parser.add_argument(
        "--sanitize", action="store_true", help="Use ASan/UBSan for native tests"
    )
    parser.add_argument("--only", nargs="+", metavar="TEST")
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument(
        "--source-root", type=Path, default=Path(__file__).resolve().parent.parent
    )
    parser.add_argument("--fixtures", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/tests"))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    root, output = args.source_root.resolve(), args.output.resolve()
    fixtures = (args.fixtures or root / "tests/fixtures").resolve()
    native = {p.stem for p in (root / "tests").glob("*_tests.cpp")}
    if not args.ui:
        native.discard("ui_tests")
    selected = [(name, False) for name in sorted(native)]
    if args.managed:
        selected += [(name, True) for name in MANAGED]
    if args.only:
        names = set(args.only)
        unknown = names - {name for name, _ in selected}
        if unknown:
            parser.error("Unknown or disabled test: " + ", ".join(sorted(unknown)))
        selected = [(name, managed) for name, managed in selected if name in names]
    required = [args.compiler] if any(not managed for _, managed in selected) else []
    if any(managed for _, managed in selected):
        required += ["mcs", "mono"]
    for tool in required:
        if not shutil.which(tool):
            parser.error("Required executable not found: " + tool)
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [sys.executable, root / "generate-layout.py", "--check"], cwd=root, check=True
    )
    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [
            pool.submit(execute, name, managed, args, root, output, fixtures)
            for name, managed in selected
        ]
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            print(
                ("PASS " if result["passed"] else "FAIL ") + result["name"], flush=True
            )
            if "error" in result:
                print("  " + result["error"], flush=True)
            (output / "summary.json").write_text(
                json.dumps(results, indent=2) + "\n", encoding="utf-8"
            )
    passed = sum(result["passed"] for result in results)
    print(f"{passed}/{len(results)} passed; results: {output / 'summary.json'}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
