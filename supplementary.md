# Supplementary simulation and evaluation specs

The V1-V4 documents describe foraging strategies. This file covers the supporting simulation features and how to compare those strategies.

## Simulation inputs and stopping rule

- `dev_challenge.exe` takes either no arguments (seed `12345`, `15` rows, `15` columns, `8` ants, food density `0.4`) or exactly `seed rows cols ants foodDensity`.
- `foodDensity` is a fraction in `[0, 1]`. The initial number of food cells is `trunc(rows * cols * foodDensity)`. The seed determines terrain, food placement, home position, and initial ant energies. All ants start at home.
- Each `worldStep()` calls the selected `forage()` strategy, updates deliveries/deaths, and checks game over. The executable also stops after **10 consecutive turns in which no ant spends energy**, avoiding an idle loop. The final output includes `Total score`.

## Built-in terminal visualizer

- The visualizer is part of the normal `dev_challenge` executable, implemented in `src/applicant_solution.cpp`; there is no separate GUI or graphics dependency. It displays a frame before foraging and a final frame. With `WAIT_FOR_ENTER=1` (default), it displays every forage turn and waits for Enter. With `WAIT_FOR_ENTER=0`, it displays only the first and final frames. `ENABLE_VISUALIZER=0` disables it for batch runs.
- ANSI backgrounds alternate green and brown by sector; a cell is darker once **at least one** ant has scanned it. This aggregate shading is display-only: ants retain separate scanned-cell and remembered-food memories. It does not grant shared knowledge.
- `H` marks home, `F` food, `P` pheromone, lowercase letters mark ant trails, uppercase letters mark current ant positions, and `@` marks an ant carrying food. Food/pheromones and then ants take precedence over trail symbols. The frame also shows score and each live ant's position, energy, and carrying state. V2/V3 show the active ant and phase.
- A frame represents a world turn, not every intermediate cell in a `move()` route. The display uses ANSI terminal colour/clear-screen sequences; use a terminal that supports them.

## Batch comparison runner

- For the large square-board sweep, `evaluation_grid.json` specifies inclusive seed, size, and density ranges plus the ant-count growth rule. `python .\run_evaluations.py --dry-run` prints the expansion count; `python .\run_evaluations.py --workers 8` runs it and resumes from `evaluation_results.csv` after interruption. The CSV records score, upper bound, initial and remaining energy, steps, stop reason, and runtime for each variant. `--summary-only` compares only worlds with results from every selected variant. The Python runner is the primary tool for this sweep.
- The older PowerShell runner remains available for the small named cases below.

- `evaluation_cases.json` is a top-level array of cases. Each case requires `name`, `seed`, `rows`, `cols`, `ants`, and `foodDensity`; dimensions and ant count must be positive, and density must be in `[0, 1]`. Example:

  ```json
  { "name": "baseline", "seed": 12345, "rows": 15, "cols": 15, "ants": 8, "foodDensity": 0.4 }
  ```

- Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\run_evaluations.ps1` from the repository root. `-CasesPath .\other_cases.json` selects another case file; `-Generator` overrides the default `Visual Studio 16 2019` CMake generator.
- The script builds five Release variants once each in `build-eval-*`: V1, V2, V3 next-sector, V3 home-pheromone, and V4. It disables the visualizer and Enter prompt through compile definitions, then runs every variant against every case using the same input values. It does not edit `CMakeLists.txt` or the strategy files.
- Output columns are case, version, score, energy bound, and percentage of initial food delivered. The runner fails if a simulation exits unsuccessfully, omits a required result, or reports a score above its bound.
- Compare versions within the same toolchain: C++ random-distribution and shuffle behavior can differ between standard-library implementations even with the same numeric seed.

## Energy-only score ceiling

- `src/main.cpp` calculates this once after constructing the world and prints `Energy-only upper bound: X of Y initial food` **before the first forage step**. It also emits a final machine-readable `RESULT` line with score and energy metrics. It uses the full initial food map only for this diagnostic; food coordinates are not passed to the ants' decision logic.
- Compute Dijkstra distances from home once. Each initial food cell has a minimum round-trip energy cost of twice its home distance. Sort these costs ascending. Starting with the sum of all ants' initial energy, pay for the cheapest food items until the next cost cannot fit. The number that fits is `X`; `Y` is the initial food count.
- This is an **upper bound, not a predicted or necessarily achievable score**. It optimistically pools energy across ants and ignores individual budgets, exploration, wasted movement, and scheduling. Under the game's fixed-energy, one-item-carrying movement rules, a higher delivered-food count would require at least the sum of the cheapest corresponding round trips, even if food is relayed. The strategy cannot use this omniscient calculation.
