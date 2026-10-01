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
