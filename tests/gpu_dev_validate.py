"""Validate actual Windows GPU probe exports, including partial/checkpoint reports."""
import json
import pathlib
import sys

folder = pathlib.Path(sys.argv[1])
reports = {p.stem: json.loads(p.read_text(encoding="utf-8-sig")) for p in folder.glob("*.json")}
assert len(reports) == 9, list(reports)
for report in reports.values():
    assert report["schema"] == "practice-suite.gpu-dev.v2"
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
assert {p["batch"] for p in sweep["pairs"]} == {16, 64, 128, 256}
assert [p["gpuFirst"] for p in sweep["pairs"]] == [False] * 4 + [True] * 4
assert sweep["pairs"][0]["batch"] != sweep["pairs"][4]["batch"]
partial = reports["partial-stop"]
assert partial["status"] == "Stopped" and partial["checked"] == 16
assert not partial["pairs"][0]["complete"]
checkpoint = json.loads((folder / "checkpoints/latest.json").read_text())
assert checkpoint["checked"] == 136 and checkpoint["status"] == "Checking results / saving"
assert not list(folder.rglob("*.tmp"))
assert list((folder / "automatic").glob("*.json"))
print("Nine GPU dev JSON reports, strict sampled routes, timestamp validity, rotating sweep, partial stop and replacement checkpoints: PASS")
