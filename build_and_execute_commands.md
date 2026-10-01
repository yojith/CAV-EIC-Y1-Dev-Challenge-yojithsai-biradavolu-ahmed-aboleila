# Build and execute commands

Run these from the repository root in PowerShell. These commands assume the Visual Studio CMake generator, which places executables in `Debug`.

## Default: visualizer with distance-ranked sectors

```powershell
cmake -S . -B build
cmake --build build --target dev_challenge
.\build\Debug\dev_challenge.exe
```

The visualizer is part of `dev_challenge`: sectors, explored cells, food, ant trails, and current ant positions are shown every step. Press Enter to advance one forage step. In `src/applicant_solution.cpp`, set `USE_SMART_SCANNING` to `0` for every indexed sector cell or `1` to skip cells covered by the ant's 7-by-7 scan.

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
