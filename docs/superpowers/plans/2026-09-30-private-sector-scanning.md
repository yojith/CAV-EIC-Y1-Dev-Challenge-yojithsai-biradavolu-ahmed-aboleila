# Private Sector Scanning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make each ant privately scan and sweep its assigned sector while showing sectors, explored cells, and trails in the renderer.

**Architecture:** Extend the existing file-local `Plan` in `src/applicant_solution.cpp` with one scanned-cell grid and one trail per ant. Preserve the existing precomputed sector queues; choose the first unscanned reachable entry, record local scan coverage on every stop, and use an aggregated read-only view only for rendering.

**Tech Stack:** C++20, existing AntWorld API, ANSI terminal rendering.

## Global Constraints

- Do not add or modify tests unless the user explicitly asks for tests.
- Ants must not read one another's scan state or food knowledge.
- Visualizer waits for Enter before each forage action.
- Keep the existing 100-step cap.

---

### Task 1: Private scanning and waypoint selection

**Files:**
- Modify: `src/applicant_solution.cpp`

**Interfaces:**
- Consumes: `Ant::foodScan`, `Ant::move`, `Ant::returnHome`, existing `Plan::queues`.
- Produces: private scan grids that drive the next sector waypoint.

- [ ] **Step 1: Add private per-ant scan state to `Plan`**

```cpp
std::vector<MapTemplate> scanned;
std::vector<std::vector<Coord>> trails;
std::vector<int> sectorForAnt;
```

- [ ] **Step 2: Initialize empty grids, trails, and sector ownership in `makePlan`**

```cpp
plan.scanned.assign(antCount, MapTemplate(world.terrainMap.size(),
    std::vector<int>(world.terrainMap.front().size(), 0)));
plan.trails.assign(antCount, {});
```

- [ ] **Step 3: Mark only the current ant's local scan square after each stop**

```cpp
for (int row = ant.position.first - ant.foodRadius; row <= ant.position.first + ant.foodRadius; ++row)
    for (int col = ant.position.second - ant.foodRadius; col <= ant.position.second + ant.foodRadius; ++col)
        if (row >= 0 && row < rows && col >= 0 && col < cols) scanned[row][col] = 1;
```

- [ ] **Step 4: Replace `nextWaypoint` selection with the first unscanned reachable queued cell**

```cpp
for (Coord waypoint : plan.queues[index])
    if (!plan.scanned[index][waypoint.first][waypoint.second] && canReachAndReturn(*this, ant, waypoint))
        return waypoint;
return {-1, -1};
```

- [ ] **Step 5: Run the existing build without adding or changing tests**

Run: `cmake --build build --target dev_challenge`

Expected: the executable builds successfully.

### Task 2: Sector/exploration/trail visualizer

**Files:**
- Modify: `src/applicant_solution.cpp`

**Interfaces:**
- Consumes: `Plan::sectorForAnt`, private `Plan::scanned`, and `Plan::trails`.
- Produces: a 2-second ANSI visualizer displaying sector color, explored tint, and trails.

- [ ] **Step 1: Derive a renderer-only explored flag from all private grids**

```cpp
bool explored = std::any_of(plan.scanned.begin(), plan.scanned.end(), [&](const MapTemplate &grid) {
    return grid[row][col] != 0;
});
```

- [ ] **Step 2: Color cells by assigned sector, darkening explored cells**

```cpp
const char *color = sector % 2 == 0 ? green : brown;
if (explored) color = sector % 2 == 0 ? darkGreen : darkBrown;
```

- [ ] **Step 3: Append positions to the owning ant's trail and overlay trail symbols before current ant positions**

```cpp
if (plan.trails[index].empty() || plan.trails[index].back() != ant.position)
    plan.trails[index].push_back(ant.position);
```

- [ ] **Step 4: Set the renderer delay default to 2000 ms**

```cpp
#define VISUALIZER_DELAY_MS 2000
```

- [ ] **Step 5: Run the existing build without adding or changing tests**

Run: `cmake --build build --target dev_challenge`

Expected: the executable builds successfully.
