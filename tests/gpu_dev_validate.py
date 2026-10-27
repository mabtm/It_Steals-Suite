"""Validate actual Windows GPU probe exports, including partial/checkpoint reports."""
import json
import pathlib
import sys

folder = pathlib.Path(sys.argv[1])
reports = {p.stem: json.loads(p.read_text(encoding="utf-8-sig")) for p in folder.glob("*.json")}
assert len(reports) == 12, list(reports)
for report in reports.values():
    assert report["schema"] == "practice-suite.gpu-dev.v4"
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

summary_files = list((folder / "automatic-suite").rglob("Dev4-Summary.json"))
assert len(summary_files) == 1
summary = json.loads(summary_files[0].read_text())
assert summary["schema"] == "practice-suite.gpu-suite.v4" and summary["status"] == "Completed"
assert summary["completedMaps"] == 2 and summary["completedTests"] == summary["totalTests"] == 24
assert {r["scene"] for r in summary["runs"]} == {13, 14}
assert all(r["top10Match"] and r["audited"] == r["scored"] and r["evaluated"] == r["requested"] for r in summary["runs"])
assert len(list(summary_files[0].parent.glob("hide-seek-outside_*.json"))) == 12
assert len(list(summary_files[0].parent.glob("classic-bonus_*.json"))) == 12
print("Automatic two-map plan, exact score audits, named per-test reports and one summary: PASS")
