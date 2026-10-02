# Build and execute commands

Run these from the repository root in PowerShell. These commands assume the Visual Studio CMake generator, which places executables in `Debug`.

## Default: V1 with distance-ranked sectors

```powershell
cmake -S . -B build
cmake --build build --target dev_challenge
.\build\Debug\dev_challenge.exe
```

The no-option build compiles `src/applicant_solution.cpp` (V1) and prints the score, energy bound, and machine-readable `RESULT` line. There is no interactive visualizer. In V1 and V4, set `USE_SMART_SCANNING` to `0` for every indexed sector cell or `1` to skip cells covered by the ant's 7-by-7 scan. V2 and V3 always skip scanned cells.

## Build a named solution

```powershell
cmake -S . -B build-v4 -G "Visual Studio 16 2019" -DSOLUTION=v4
cmake --build build-v4 --config Debug --target dev_challenge
.\build-v4\Debug\dev_challenge.exe
```

Replace `v4` in the three commands with `v1`, `v2`, `v3_1`, or `v3_2` to build that source. V3.1 is the next-sector handoff; V3.2 is the home-pheromone handoff. Use a separate build directory for each selection because CMake caches `SOLUTION`. Only the selected source file is compiled into `dev_challenge`; the ordinary `build` directory remains on `applicant` unless explicitly reconfigured. All modes stop after 10 consecutive turns without energy use.

V4 uses V1's sector scan queue but allows an ant to pursue affordable food it has scanned outside its own sector.

## Evaluate the square-board parameter grid

[`evaluation_grid.json`](evaluation_grid.json) defines seeds 1–10, square sizes 10–50 by 5, food densities 10%–90% by 10%, and ant counts 1–(10 plus one per size step). The Python runner includes both V3 modes, so four strategies produce five variant measurements per world.

```powershell
python .\run_evaluations.py --dry-run
python .\run_evaluations.py --max-worlds 20 --workers 4
python .\run_evaluations.py --workers 8
python .\run_evaluations.py --summary-only
python .\make_evaluation_report.py .\evaluation_results_by_solution.csv
```

The full run is long. Results are written incrementally to ignored `evaluation_results_by_solution.csv`; rerunning the same command resumes missing worlds. The old `evaluation_results.csv` remains untouched, and `python .\make_evaluation_report.py` still renders that historical run. `--summary-only` reports completed, paired worlds without rebuilding. The report script writes a self-contained `evaluation_report.html` with graphs by board size, density, and ant count. The runner refuses to mix results after source or grid settings change; choose a new `--output` path in that case. Each row includes score, energy-only upper bound, initial and remaining energy, steps, stop reason, and elapsed time. `--timeout` changes the per-simulation limit (default 120 seconds); `--timeout 0` disables it if the computer may sleep during a run.

## Compare predefined cases with the older PowerShell runner

Edit [`evaluation_cases.json`](evaluation_cases.json) to add cases with `name`, `seed`, `rows`, `cols`, `ants`, and `foodDensity` (a fraction from `0` to `1`). Then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run_evaluations.ps1
```

The runner builds V1, V2, V3.1 next-sector, V3.2 home-pheromone, and V4 once each, then prints each case's score, energy-only upper bound, and percentage of initial food delivered. The bound uses initial food positions and pooled ant energy but ignores search energy and per-ant limits; it is a benchmark, not information given to the strategy. It does not modify source files or the normal no-argument run. To use another JSON file, pass `-CasesPath .\my_cases.json`.

## Angle-ranked sectors

```powershell
cmake -S . -B build-angle -DSOLUTION=v1 -DCMAKE_CXX_FLAGS="/DUSE_ANGLE_RANKING=1"
cmake --build build-angle --target dev_challenge
.\build-angle\Debug\dev_challenge.exe
```

## Tests

```powershell
cmake --build build --target antworld_tests
ctest --test-dir build -C Debug --output-on-failure
```
