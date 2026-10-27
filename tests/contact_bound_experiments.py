#!/usr/bin/env python3
"""Build isolated contact-bound variants and compare complete searches.

Production headers are copied into build/, never edited. Every result must
retain the reference Top 10 and account for every generated/checked seed.
Run sequentially without concurrent builds, tests, game or OBS workloads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess


def replace(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError("Production source changed; review experiment hook: " + before[:70])
    return source.replace(before, after)


def prepare(root, target, variant):
    target.mkdir(parents=True, exist_ok=True)
    for file in (root / "Trainer").glob("*.h"):
        shutil.copyfile(file, target / file.name)
    if variant == "reference":
        return
    shutil.copyfile(root / "tests/contact_bound_lab.h", target / "ContactBoundLab.h")
    alpha = {"residual": 1., "quarter": .25, "half": .5, "three-quarter": .75}.get(variant)
    file = target / "PickupRouting.h"
    source = replace(file.read_text(), "#include <span>", '#include <span>\n#include "ContactBoundLab.h"')
    source = replace(source, "    bool active = false;", "    bool active = false;\n    std::unique_ptr<ContactBoundLab> bound;")
    cost = "leg.cost[x * contacts[b].size() + y]" + (f" - {alpha} * leg.lower" if alpha else "")
    value = "bound->Tighten(nodes, bound->Lower(nodes, canceled), cutoff, canceled)" if variant == "dual" else "bound->Lower(nodes, canceled)"
    extra = '''    void PrepareBound(const Cancel &canceled)
    {
        if (!active) return;
        std::vector<size_t> counts;
        for (const auto &row : contacts) counts.push_back(row.size());
        bound = std::make_unique<ContactBoundLab>(counts, [&](size_t a, size_t x, size_t b, size_t y) {
            if (a > b) { std::swap(a,b); std::swap(x,y); }
            const auto &leg = Pair(int(a),int(b));
            return leg.cost.empty() ? Unreachable : COST;
        }, canceled);
    }
    double ContactLower(std::span<const int> nodes, double cutoff, const Cancel &canceled) const
    {
        return bound ? VALUE : 0;
    }

'''.replace("COST", cost).replace("VALUE", value)
    source = replace(source, "    bool Estimated(int a, int ca, int b, int cb) const", extra + "    bool Estimated(int a, int ca, int b, int cb) const")
    file.write_text(source)
    file = target / "SeedSearch.h"
    source = file.read_text()
    line = "                    pickups->SetLeg(from, to, table->Between(from, to), estimates[from][to] != 0, canceled);"
    source = replace(source, line, line + "\n        pickups->PrepareBound(canceled);")
    needle = "    RouteResult Score(const Selection &selection, bool fast, const Cancel &canceled, RouteWorkspace &work,"
    extra = '''    double ContactBound(const Selection &selection, double cutoff, const Cancel &canceled) const
    {
        std::array<int,65> nodes;
        int count = int(selection.crystals.size()) + 1;
        for (int i=0;i<count;i++) nodes[i] = Node(selection,i);
        return pickups->ContactLower(std::span<const int>(nodes.data(),size_t(count)), cutoff, canceled);
    }

'''
    source = replace(source, needle, extra + needle)
    marker = "                                // Spend the bounded refinement budget only on"
    if alpha:
        source = replace(source, "contacts ? pickups->Lower(nodes[from], b) : lengths.at(b)",
                         f"contacts ? {alpha} * pickups->Lower(nodes[from], b) : lengths.at(b)")
        source = replace(source, marker,
            "                                double residual = count > 16 ? distances->ContactBound(selection, boundCutoff, canceled) : 0;\n"
            "                                if (!cached) lower += residual;\n" + marker)
        source = replace(source,
            "lower = TightenRouteLowerBound(workspace.matrix.data(), count, lower,\n                                                                   boundCutoff, canceled);",
            "lower = TightenRouteLowerBound(workspace.matrix.data(), count, lower - residual,\n                                                                   boundCutoff - residual, canceled) + residual;")
    else:
        source = replace(source, marker,
            "                                if (count > 16 && lower <= boundCutoff + 1e-6)\n"
            "                                    lower = std::max(lower, distances->ContactBound(selection, boundCutoff, canceled));\n" + marker)
    file.write_text(source)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    choices = ["reference", "degree", "dual", "residual", "quarter", "half", "three-quarter"]
    parser.add_argument("--variants", nargs="+", choices=choices, default=["reference", "degree", "dual"])
    parser.add_argument("--output", type=Path, default=Path("build/contact-bound-experiments"))
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--count", type=int, default=6000)
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--radius", type=float, default=1.35)
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    out = args.output.resolve()
    variants = list(dict.fromkeys(["reference", *args.variants]))
    for variant in variants:
        prepare(root, out / variant, variant)
        if not args.prepare_only:
            subprocess.run([args.compiler, "-std=c++20", "-O2", "-pthread", "-I" + str(out / variant),
                            root / "tests/seed_route_benchmark.cpp", "-o", out / (variant + "-benchmark")], check=True)
            print("Compiled", variant, flush=True)
    manifest = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in (root / "Trainer").glob("*.h")}
    (out / "source-hashes.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if args.prepare_only:
        return
    affinity = set(sorted(os.sched_getaffinity(0))[:args.workers + 1]) if hasattr(os, "sched_getaffinity") else None
    rows, fingerprint = [], None
    for pair in range(1, args.pairs + 1):
        for variant in variants if pair % 2 else variants[::-1]:
            command = [out / (variant + "-benchmark"), root / "tests/fixtures/hide-seek-outside-boxes.routefixture",
                       str(args.count), "52", str(args.workers), "search", str(args.radius)]
            output = subprocess.check_output(command, cwd=root, text=True,
                preexec_fn=(lambda: os.sched_setaffinity(0, affinity)) if affinity else None)
            line = next(line for line in output.splitlines() if line.startswith("search,"))
            fields = line.split(",")
            result = [x for x in fields if x.count(":") == 2 and x.split(":")[0].isdigit()]
            if fingerprint is not None and result != fingerprint:
                raise RuntimeError("Variant changed the final Top 10: " + variant)
            fingerprint = result
            metrics = dict(x.split("=", 1) for x in fields if "=" in x)
            if int(metrics["checked"]) != args.count or int(metrics["generated"]) != args.count:
                raise RuntimeError("Seed coverage changed")
            rows.append(dict(variant=variant, pair=pair, radius=args.radius, seeds=args.count,
                workers=int(fields[2]), rate=float(fields[5]), scored=int(metrics["scored"]),
                pruned=int(metrics["pruned"]), raw=line))
            (out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            print(variant, pair, "rate", round(rows[-1]["rate"], 1), "scored", rows[-1]["scored"], flush=True)
    for variant in variants:
        values = [r["rate"] for r in rows if r["variant"] == variant]
        print(variant, "median", statistics.median(values))


if __name__ == "__main__":
    main()
