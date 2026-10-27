"""Validate actual Windows GPU probe exports, including partial/checkpoint reports."""
import json
import pathlib
import sys

folder = pathlib.Path(sys.argv[1])
reports = {p.stem: json.loads(p.read_text(encoding="utf-8-sig")) for p in folder.glob("*.json")}
assert len(reports) == 12, list(reports)
for report in reports.values():
    assert report["schema"] == "practice-suite.gpu-dev.v6"
    assert report["mismatches"] == 0
    assert report["caseRetention"]["stored"] == len(report["cases"])
    assert len(report["cases"]) <= 84
    for row in report["cases"]:
        assert row["orderMatch"] and row["scoreMatch"] and row["centerLengthMatch"]
        assert row["cpu"] == row["gpu"]
        assert row["cpuCenterLength"] == row["gpuCenterLength"]
    for pair in report["pairs"]:
        if pair["checked"]:
            assert pair["cpuDigest"] == pair["gpuDigest"]
        if pair["timestampBatches"]:
            assert all(pair[k] is not None and pair[k] >= 0 for k in ("gpuUploadMs", "gpuKernelMs", "gpuCopyMs"))
        else:
            assert all(pair[k] is None for k in ("gpuUploadMs", "gpuKernelMs", "gpuCopyMs"))

for name in ("hide-seek-outside-boxes", "hide-seek-boxes"):
    report = reports[name]
    assert report["status"] == "Completed" and report["checked"] == 24
    assert report["warmupChecked"] == 4 and report["sourceKind"] == "asset_fixture"
    assert all(p["complete"] for p in report["pairs"])

sweep = reports["batch-sweep"]
assert sweep["checked"] == 136 and len(sweep["pairs"]) == 8
assert {p["batch"] for p in sweep["pairs"]} == {256, 512, 1024, 2048}
assert [p["gpuFirst"] for p in sweep["pairs"]] == [False] * 4 + [True] * 4
assert sweep["pairs"][0]["batch"] != sweep["pairs"][4]["batch"]
partial = reports["partial-stop"]
assert partial["status"] == "Stopped" and partial["checked"] == 16
assert not partial["pairs"][0]["complete"]
control = reports["unprofiled"]
assert control["checked"] == 3 and not control["request"]["gpuTiming"]
assert not control["adapter"]["timestampsSupported"] and control["pairs"][0]["queryCollectMs"] == 0
assert reports["catalog-kernel"]["adapter"]["dynamicInputBytesPerLayout"] == 216
assert reports["legacy-input"]["adapter"]["dynamicInputBytesPerLayout"] == 33292
assert [p["gpuDigest"] for p in reports["legacy-input"]["pairs"]] == [p["gpuDigest"] for p in reports["hide-seek-boxes"]["pairs"]]
checkpoint = json.loads((folder / "checkpoints/latest.json").read_text())
assert checkpoint["checked"] == 136 and checkpoint["status"] == "Checking results / saving"
assert not list(folder.rglob("*.tmp"))
assert list((folder / "automatic").glob("*.json"))
print("Twelve GPU dev JSON reports, legacy/catalog parity, timestamp/control runs, rotating sweep, partial stop and replacement checkpoints: PASS")

summary_files = list((folder / "automatic-suite").rglob("Dev6-Summary.json"))
assert len(summary_files) == 2
summaries = [(p, json.loads(p.read_text())) for p in summary_files]
complete_path, summary = next((p, s) for p, s in summaries if s["status"] == "Completed")
stopped = [s for _, s in summaries if s["status"] == "Stopped"]
assert len(stopped) == 1 and stopped[0]["completedTests"] == 0
assert summary["schema"] == "practice-suite.gpu-suite.v6" and summary["status"] == "Completed"
assert summary["completedMaps"] == 3 and summary["completedTests"] == summary["totalTests"] == 276
assert {r["scene"] for r in summary["runs"]} == {5, 13, 14}
assert all(r["top10Match"] and r["audited"] == r["scored"] and r["evaluated"] == r["requested"] for r in summary["runs"])
assert list(complete_path.parent.iterdir()) == [complete_path]
assert len(summary["plan"]["keys"]) == 4
assert {v["crystals"] for v in summary["shaderPreparation"]} == {22, 40, 52}
assert len(summary["shaderPreparation"]) == 15 and len(summary["calibration"]) == 3
references = {(r["scene"], r["stage"], r["key"], r["spawnFilter"]): r for r in summary["runs"] if r["reference"]}
for r in summary["runs"]:
    assert r["top10"] == references[r["scene"], r["stage"], r["key"], r["spawnFilter"]]["top10"]
    assert r["evaluated"] == r["scored"] + r["pruned"] + r["filtered"]
    assert r["key"] == summary["plan"]["keys"][r["keyIndex"]]
    assert (r["keyIndex"] < 2) == (r["stage"] == "screen")
    if r["boundsMode"] == "off": assert r["pruned"] == 0
    if r["backend"] == "GPU pipeline":
        assert r["shader"] in {"generic", "fixed", "triangle", "compact", "compact-fixed"}
        assert r["shaderThreads"] == (32 if r["shaderCrystals"] and r["shaderCrystals"] <= 32 else 64)
        n = r["shaderCapacity"]
        side, per_word = n + 1, 32 // r["shaderNeighborBits"]
        cells = side * (side + 1) // 2 if r["shaderTriangular"] else side * side
        expected = 8 * cells + 12 * r["shaderThreads"] + 4 * n + 256 + 4 * n * ((n + per_word - 1) // per_word)
        assert r["shaderSharedBytes"] == expected
        if r["boundsMode"] == "off":
            assert r["boundsChecked"] == 0
            metadata = next(m for m in summary["mapMetadata"] if m["capture"]["scene"] == r["scene"])
            if r["spawnFilter"] < 0 and metadata["adapter"]["timestampsSupported"]:
                assert r["gpuTimestampBatches"] + r["gpuTimestampInvalid"] == 2
        if r["boundsMode"] == "on": assert r["boundsSkipped"] == 0
assert {r["boundsMode"] for r in summary["runs"] if r["backend"] == "GPU pipeline"} == {"on", "off", "adaptive"}
import statistics
for choice in summary["calibration"]:
    screen = [r for r in summary["runs"] if r["scene"] == choice["scene"] and r["stage"] == "screen" and r["backend"] == "GPU pipeline"]
    field = "gpuKernelMs" if choice["metric"] == "median complete GPU kernel time" else "elapsedMs"
    candidates = {kind: statistics.median(r[field] for r in screen if r["shader"] == kind and r["boundsMode"] == "off")
                  for kind in ["triangle", "compact", "compact-fixed"]}
    assert choice["stressShader"] == min(candidates, key=candidates.get)
    bounds = ["on", "off", "adaptive"]
    walls = [statistics.median(r["elapsedMs"] for r in screen if r["shader"] == choice["baselineShader"] and r["boundsMode"] == b) for b in bounds]
    assert choice["screenBoundsRecommendation"] == bounds[min(range(3), key=walls.__getitem__)]
    assert all(abs(a - b) < 1e-8 for a, b in zip(walls, choice["boundsWallMsOnOffAdaptive"]))
    assert choice["samplesPerVariant"] == 2
    assert abs(choice["candidateMs"] - candidates[choice["stressShader"]]) < 1e-8
    stress = [r for r in summary["runs"] if r["scene"] == choice["scene"] and r["stage"] == "stress" and r["backend"] == "GPU pipeline"]
    assert {r["shader"] for r in stress} == {choice["baselineShader"], choice["stressShader"]}
    assert {r["workers"] for r in stress} == {2, 4, 6}
    assert {r["batch"] for r in stress} == {1024, 2048, 4096, 8192}
print("Two-stage three-map/two-key plan, compressed shaders, held-out keys, strict audits, calibration, single-file Unicode exports and Clean-mode cancellation: PASS")
