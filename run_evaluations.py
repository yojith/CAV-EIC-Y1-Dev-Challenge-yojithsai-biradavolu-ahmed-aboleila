"""Run the parameter grid in evaluation_grid.json with resumable CSV output."""

import argparse
import csv
import hashlib
import json
import statistics
import subprocess
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from itertools import islice
from pathlib import Path


ROOT = Path(__file__).resolve().parent
FLAGS = {
    "v1": "",
    "v2": "/DUSE_V2_STRATEGY=1",
    "v3-next": "/DUSE_V3_STRATEGY=1",
    "v3-home": "/DUSE_V3_STRATEGY=1 /DV3_HOME_PHEROMONE=1",
    "v4": "/DUSE_V4_STRATEGY=1",
}
FIELDS = ["run_id", "seed", "size", "ants", "density_percent", "variant", "score",
          "upper_bound", "initial_food", "initial_energy", "remaining_energy", "steps",
          "stop_reason", "elapsed_seconds"]


def values(spec):
    start, stop, step = (int(spec[key]) for key in ("start", "stop", "step"))
    if start < 1 or step < 1 or stop < start or (stop - start) % step:
        raise ValueError(f"Invalid inclusive range: {spec}")
    return range(start, stop + 1, step)


def read_plan(path):
    plan = json.loads(path.read_text(encoding="utf-8"))
    seeds = values(plan["seeds"])
    sizes = values(plan["board_size"])
    densities = values(plan["food_density_percent"])
    ants = plan["ants"]
    if any(d > 100 for d in densities) or int(ants["minimum"]) < 1 or \
            int(ants["maximum_at_smallest_size"]) < int(ants["minimum"]) or \
            int(ants["increase_per_size_step"]) < 0:
        raise ValueError("Invalid density or ant-count rule")
    variants = plan["variants"]
    if not variants or len(variants) != len(set(variants)) or any(v not in FLAGS for v in variants):
        raise ValueError("Variants must be distinct and supported")
    return plan, seeds, sizes, densities, variants


def worlds(plan, seeds, sizes, densities):
    ant_rule = plan["ants"]
    for size_index, size in enumerate(sizes):
        maximum = int(ant_rule["maximum_at_smallest_size"]) + \
                  size_index * int(ant_rule["increase_per_size_step"])
        for density in densities:
            for ant_count in range(int(ant_rule["minimum"]), maximum + 1):
                for seed in seeds:
                    yield seed, size, ant_count, density


def signature(plan, generator):
    digest = hashlib.sha256(json.dumps(plan, sort_keys=True).encode("utf-8"))
    digest.update(generator.encode("utf-8"))
    for name in ("CMakeLists.txt", "src/main.cpp", "src/applicant_solution.cpp", "src/antworld.cpp",
                 "src/useable_functions.cpp", "include/antworld.h", "include/utility_functions.h"):
        digest.update(name.encode("utf-8"))
        digest.update((ROOT / name).read_bytes())
    return digest.hexdigest()[:16]


def build(variant, generator):
    directory = ROOT / f"build-eval-{variant}"
    flags = f"/EHsc /DWAIT_FOR_ENTER=0 /DENABLE_VISUALIZER=0 {FLAGS[variant]}"
    configure = ["cmake", "-S", str(ROOT), "-B", str(directory), "-G", generator,
                 "-A", "x64", f"-DCMAKE_CXX_FLAGS={flags}", "-DBUILD_TESTING=OFF"]
    for command in (configure, ["cmake", "--build", str(directory), "--config", "Release",
                                "--target", "dev_challenge"]):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f"Build failed for {variant}:\n{result.stdout}")
    executable = directory / "Release" / "dev_challenge.exe"
    if not executable.is_file():
        raise FileNotFoundError(executable)
    return executable


def existing_rows(path, run_id):
    if not path.exists():
        return {}
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"Unexpected CSV columns in {path}")
        rows = {}
        for row in reader:
            if row["run_id"] != run_id:
                raise ValueError("Results belong to another config or source revision; use a new output path")
            key = tuple(row[field] for field in ("seed", "size", "ants", "density_percent", "variant"))
            rows[key] = row
        return rows


def run_case(executable, case, variant, run_id, timeout):
    seed, size, ants, density = case
    started = time.monotonic()
    result = subprocess.run([str(executable), str(seed), str(size), str(size), str(ants),
                             f"{density / 100:.2f}"], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout or None)
    if result.returncode:
        raise RuntimeError(f"{variant} failed on {case}:\n{result.stdout[-2000:]}")
    matches = [line.removeprefix("RESULT ") for line in result.stdout.splitlines()
               if line.startswith("RESULT ")]
    if len(matches) != 1:
        raise ValueError(f"Missing RESULT line from {variant} on {case}:\n{result.stdout[-2000:]}")
    metrics = json.loads(matches[0])
    if not 0 <= metrics["score"] <= metrics["upper_bound"] <= metrics["initial_food"] or \
            not 0 <= metrics["remaining_energy"] <= metrics["initial_energy"]:
        raise ValueError(f"Invalid score, bound, or energy from {variant} on {case}: {metrics}")
    return dict(run_id=run_id, seed=seed, size=size, ants=ants, density_percent=density,
                variant=variant, **metrics, elapsed_seconds=f"{time.monotonic() - started:.3f}")


def summarize(rows, variants):
    grouped = defaultdict(dict)
    for row in rows.values():
        key = tuple(row[field] for field in ("seed", "size", "ants", "density_percent"))
        grouped[key][row["variant"]] = row
    complete = [group for group in grouped.values() if all(v in group for v in variants)]
    print(f"Complete paired worlds: {len(complete)} / {len(grouped)} started")
    if not complete:
        return
    print("Variant     Mean score  Mean gap  Mean score/bound  Wins (ties included)  Mean remaining energy")
    for variant in variants:
        scores = [int(group[variant]["score"]) for group in complete]
        gaps = [int(group[variant]["upper_bound"]) - score
                for group, score in zip(complete, scores)]
        achieved = [int(group[variant]["score"]) / int(group[variant]["upper_bound"])
                    for group in complete if int(group[variant]["upper_bound"]) > 0]
        wins = sum(int(group[variant]["score"]) ==
                   max(int(group[v]["score"]) for v in variants) for group in complete)
        energy = statistics.fmean(int(group[variant]["remaining_energy"]) for group in complete)
        ratio = statistics.fmean(achieved) if achieved else 0.0
        print(f"{variant:<11} {statistics.fmean(scores):>10.2f} {statistics.fmean(gaps):>9.2f} "
              f"{ratio:>17.3f} {wins:>21} {energy:>22.1f}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=ROOT / "evaluation_grid.json")
    parser.add_argument("--output", type=Path, default=ROOT / "evaluation_results.csv")
    parser.add_argument("--generator", default="Visual Studio 16 2019")
    parser.add_argument("--timeout", type=int, default=120, help="seconds per simulation; 0 disables wall-clock timeout")
    parser.add_argument("--workers", type=int, default=1, help="worlds evaluated concurrently")
    parser.add_argument("--max-worlds", type=int, default=0, help="run only this many worlds for a pilot")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--summary-only", action="store_true")
    args = parser.parse_args()
    if args.workers < 1 or args.timeout < 0 or args.max_worlds < 0:
        parser.error("workers must be positive; timeout and max-worlds must be nonnegative")
    plan, seeds, sizes, densities, variants = read_plan(args.config)
    count = sum(1 for _ in worlds(plan, seeds, sizes, densities))
    print(f"Plan: {count} worlds x {len(variants)} variants = {count * len(variants)} runs")
    if args.dry_run:
        return
    run_id = signature(plan, args.generator)
    rows = existing_rows(args.output, run_id)
    if args.summary_only:
        summarize(rows, variants)
        return
    executables = {variant: build(variant, args.generator) for variant in variants}
    selected = list(islice(worlds(plan, seeds, sizes, densities), args.max_worlds or None))
    pending = []
    for case in selected:
        seed, size, ants, density = case
        prior = {variant: rows[tuple(map(str, (seed, size, ants, density, variant)))]
                 for variant in variants if tuple(map(str, (seed, size, ants, density, variant))) in rows}
        if len(prior) != len(variants):
            pending.append((case, prior))
    print(f"Pending worlds: {len(pending)} (workers: {args.workers})", flush=True)

    def run_world(task):
        case, prior = task
        complete = dict(prior)
        created = []
        for variant in variants:
            if variant not in complete:
                row = run_case(executables[variant], case, variant, run_id, args.timeout)
                complete[variant] = row
                created.append(row)
        reference = None
        for variant in variants:
            world_data = tuple(int(complete[variant][field]) for field in
                               ("upper_bound", "initial_food", "initial_energy"))
            if reference is not None and world_data != reference:
                raise ValueError(f"Variants generated different worlds for {case}")
            reference = world_data
        return case, created

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("a", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS)
        if handle.tell() == 0:
            writer.writeheader()
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            for offset in range(0, len(pending), 100):
                for case, created in pool.map(run_world, pending[offset:offset + 100]):
                    for row in created:
                        writer.writerow(row)
                        key = tuple(str(row[field]) for field in
                                    ("seed", "size", "ants", "density_percent", "variant"))
                        rows[key] = row
                    handle.flush()
                seed, size, ants, density = pending[min(offset + 99, len(pending) - 1)][0]
                print(f"Completed {min(offset + 100, len(pending))}/{len(pending)} pending worlds; "
                      f"latest: size={size}, density={density}%, ants={ants}, seed={seed}", flush=True)
    summarize(rows, variants)


if __name__ == "__main__":
    main()
