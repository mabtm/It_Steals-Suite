#!/usr/bin/env python3
"""Compare an external LKH executable with the production open-path/contact model.

The external solver is a research input; it is not bundled with PracticeSuite.
Its wall time includes process/file overhead. All returned tours are rescored
using the original double distances and verified continuous pickup contacts.
"""

import argparse
import csv
import json
from pathlib import Path
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", type=Path, required=True)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("build/algorithm-lab"))
    parser.add_argument("--count", type=int, default=64)
    args = parser.parse_args()
    if not 1 <= args.count <= 512:
        parser.error("Choose 1–512 deterministic layouts")
    solver, driver, output = args.solver.resolve(), args.driver.resolve(), args.output.resolve()
    root = Path(__file__).resolve().parent.parent
    output.mkdir(parents=True, exist_ok=True)
    inputs = output / "inputs"
    exported = subprocess.check_output([driver, "export", inputs, str(args.count)], cwd=root, text=True)
    (output / "baseline.csv").write_text(exported)
    summaries = []
    for candidates, trials, move in [(5, 1, 5), (10, 1, 5), (20, 1, 5), (5, 5, 5), (10, 5, 5), (5, 1, 2)]:
        name = f"lkh-c{candidates}-t{trials}-m{move}"
        target = output / name
        target.mkdir(exist_ok=True)
        elapsed = 0
        errors = []
        for index in range(args.count):
            parameter = target / f"{index}.par"
            parameter.write_text(
                f"PROBLEM_FILE = {inputs / f'{index}.atsp'}\n"
                f"OUTPUT_TOUR_FILE = {target / f'{index}.tour'}\n"
                f"MAX_CANDIDATES = {candidates}\nMAX_TRIALS = {trials}\n"
                f"MOVE_TYPE = {move}\nRUNS = 1\nSEED = 1\nPRECISION = 1\nTRACE_LEVEL = 0\n"
            )
            started = time.perf_counter()
            result = subprocess.run([solver, parameter], cwd=root, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=20)
            elapsed += time.perf_counter() - started
            (target / f"{index}.log").write_text(result.stdout)
            if result.returncode or not (target / f"{index}.tour").exists():
                errors.append(index)
                break
        if errors:
            summary = dict(variant=name, errors=errors, seconds=elapsed)
        else:
            scored = subprocess.check_output([driver, "score", target, str(args.count)], cwd=root, text=True)
            (target / "rescored.csv").write_text(scored)
            rows = [[float(value) for value in row] for row in csv.reader(scored.splitlines())]
            differences = [row[2] - row[1] for row in rows]
            summary = dict(variant=name, count=len(rows), external_seconds=elapsed,
                           external_layouts_per_s=len(rows) / elapsed,
                           reference_scoring_seconds=sum(row[3] for row in rows) / 1e6,
                           improved=sum(d < -1e-6 for d in differences),
                           worse=sum(d > 1e-6 for d in differences),
                           equal=sum(abs(d) <= 1e-6 for d in differences),
                           mean_units_change=statistics.mean(differences),
                           worst_units_increase=max(differences))
        summaries.append(summary)
        (output / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
        print(json.dumps(summary), flush=True)


if __name__ == "__main__":
    main()
