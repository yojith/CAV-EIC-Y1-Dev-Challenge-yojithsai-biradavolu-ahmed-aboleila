# Build and execute commands

Run these from the repository root in PowerShell. These commands assume the Visual Studio CMake generator, which places executables in `Debug`.

## Default: visualizer with distance-ranked sectors

```powershell
cmake -S . -B build
cmake --build build --target dev_challenge
.\build\Debug\dev_challenge.exe
```

The visualizer is part of `dev_challenge`: sectors, explored cells, food, ant trails, and current ant positions are shown every step. In `src/applicant_solution.cpp`, set `USE_SMART_SCANNING` to `0` for every indexed sector cell or `1` to skip cells covered by the ant's 7-by-7 scan. Set `WAIT_FOR_ENTER` to `1` to advance one forage step per Enter, or `0` to show only the initial and final maps.

## Compare V1 and V2 without stepping

```powershell
cmake -S . -B build-v1-compare -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/DUSE_V2_STRATEGY=0 /DWAIT_FOR_ENTER=0"
cmake --build build-v1-compare --config Debug --target dev_challenge
.\build-v1-compare\Debug\dev_challenge.exe

cmake -S . -B build-v2-compare -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/DUSE_V2_STRATEGY=1 /DWAIT_FOR_ENTER=0"
cmake --build build-v2-compare --config Debug --target dev_challenge
.\build-v2-compare\Debug\dev_challenge.exe
```

`USE_V2_STRATEGY` defaults to `0`, so the existing build remains V1. With `1`, one ant acts at a time, sweeps its sector in up to three-cell segments, and attempts a pheromone-marked food handoff in the next sector when it cannot return home.

## Compare V3 handoff modes

```powershell
cmake -S . -B build-v3-next-compare -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/DUSE_V3_STRATEGY=1 /DV3_HOME_PHEROMONE=0 /DWAIT_FOR_ENTER=0"
cmake --build build-v3-next-compare --config Debug --target dev_challenge
.\build-v3-next-compare\Debug\dev_challenge.exe

cmake -S . -B build-v3-home-compare -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/DUSE_V3_STRATEGY=1 /DV3_HOME_PHEROMONE=1 /DWAIT_FOR_ENTER=0"
cmake --build build-v3-home-compare --config Debug --target dev_challenge
.\build-v3-home-compare\Debug\dev_challenge.exe
```

All modes stop after 10 consecutive turns in which no ant spends energy. `USE_V2_STRATEGY` and `USE_V3_STRATEGY` cannot both be `1`.

## V4 delivery-first mode

```powershell
cmake -S . -B build-v4-compare -G "Visual Studio 16 2019" -DCMAKE_CXX_FLAGS="/DUSE_V4_STRATEGY=1 /DWAIT_FOR_ENTER=0"
cmake --build build-v4-compare --config Debug --target dev_challenge
.\build-v4-compare\Debug\dev_challenge.exe
```

V4 uses V1's sector scan queue but allows an ant to pursue affordable food it has scanned outside its own sector. Only one of `USE_V2_STRATEGY`, `USE_V3_STRATEGY`, and `USE_V4_STRATEGY` may be `1`.

## Evaluate the square-board parameter grid

[`evaluation_grid.json`](evaluation_grid.json) defines seeds 1–10, square sizes 10–50 by 5, food densities 10%–90% by 10%, and ant counts 1–(10 plus one per size step). The Python runner includes both V3 modes, so four strategies produce five variant measurements per world.

```powershell
python .\run_evaluations.py --dry-run
python .\run_evaluations.py --max-worlds 20 --workers 4
python .\run_evaluations.py --workers 8
python .\run_evaluations.py --summary-only
python .\make_evaluation_report.py
```

The full run is long. Results are written incrementally to ignored `evaluation_results.csv`; rerunning the same command resumes missing worlds. `--summary-only` reports completed, paired worlds without rebuilding. `make_evaluation_report.py` writes a self-contained `evaluation_report.html` with graphs by board size, density, and ant count. The runner refuses to mix results after source or grid settings change; choose a new `--output` path in that case. Each row includes score, energy-only upper bound, initial and remaining energy, steps, stop reason, and elapsed time. `--timeout` changes the per-simulation limit (default 120 seconds); `--timeout 0` disables it if the computer may sleep during a run.

## Compare predefined cases with the older PowerShell runner

Edit [`evaluation_cases.json`](evaluation_cases.json) to add cases with `name`, `seed`, `rows`, `cols`, `ants`, and `foodDensity` (a fraction from `0` to `1`). Then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\run_evaluations.ps1
```

The runner builds V1, V2, V3 next-sector, V3 home-pheromone, and V4 once each, with the visualizer disabled, then prints each case's score, energy-only upper bound, and percentage of initial food delivered. The bound uses initial food positions and pooled ant energy but ignores search energy and per-ant limits; it is a benchmark, not information given to the strategy. It does not modify `CMakeLists.txt` or the normal no-argument run. To use another JSON file, pass `-CasesPath .\my_cases.json`.

## Angle-ranked sectors

```powershell
cmake -S . -B build-angle -DUSE_ANGLE_RANKING=ON
cmake --build build-angle --target dev_challenge
.\build-angle\Debug\dev_challenge.exe
```

## Tests

```powershell
cmake --build build --target antworld_tests
ctest --test-dir build -C Debug --output-on-failure
```
