# V1 solution rough draft

- Keep `AntWorld::forage()`, strategy helpers, and per-ant memory in `src/applicant_solution.cpp`; leave the game framework untouched.
- Assign each ant an initial outward sector/waypoint from `homeCoordinates`.
- If carrying food, call `returnHome()`.
- Otherwise call `foodScan()`, remember food found by that ant, and pursue the remembered food with the cheapest path from its current position, even when it cannot reach the food or return home.
- If no food is remembered, move toward the next sector waypoint without reserving energy for the return trip.
- Use the solution's own home-distance calculation and the provided path helpers; do not inspect global `foodMap` except through `foodScan()`.
- After 10 consecutive turns without energy use, use ordinary adjacent and homeward moves to spend remaining energy. An ant can still get stuck with positive energy if every adjacent move is unaffordable.
- Skip pheromones in v1; add one-per-ant "productive area" markers only if benchmarks show sector search needs help.
- Test fixed seed `12345`, then several random seeds and compare final scores.

## Equal-area sectors from home

Let `n = antCount`. The home coordinate is random, so `n` equally spaced rays do **not** give `n` equal-area parts of the square unless home happens to be centred. For equal sectors, create `n` wedges whose clipped area inside the map is equal:

1. Treat `homeCoordinates` as point `P`, choose any starting angle, and sweep a ray around `P` through 360 degrees.
2. For each angle `theta`, find where the ray `(cos(theta), sin(theta))` first hits a map edge. Its maximum distance is `r(theta)`.
3. The area added by a tiny angular slice is `0.5 * r(theta)^2 * dtheta`.
4. Accumulate that area while sweeping. Each time it reaches `mapArea / n`, save the current ray as a sector boundary and reset the accumulator.
5. Give ant `i` the wedge between boundary `i` and boundary `i + 1`; its exploration waypoints lie along that wedge's centre angle, moving outward from `P`.

This is a numeric approximation: sample many small angle steps and linearly interpolate the final boundary. Equal-angle sectors are the simpler v1 fallback if equal coverage does not improve scores.
- ants wont all start with the same energy, so make sure to have the ants with the most energy take the most diagonal paths first (closest to 45 degrees).

-start off with the number of ants, and the size of the board, and the location of the home base. divide the board into sectors based on number of ants such that each sector has equal area.

## Discrete sector pseudocode

The map is made of integer cells. Exact equality is possible only when the number of cells being partitioned is divisible by `n`; otherwise, make sector sizes differ by at most one cell. Exclude home from the partition because every ant begins there.

```text
n = ants.size()
cells = every map cell except homeCoordinates

# Put cells in circular order around home.  The tie-breakers make the result repeatable.
sort cells by (
    normalized_angle(atan2(cell.row - home.row, cell.col - home.col)), # (polar coordinate sorting)
    cell.row,
    cell.col
)

base = cells.size() / n
remainder = cells.size() % n
nextCell = 0

for sector in 0 .. n - 1:
    sectorSize = base
    if sector < remainder:
        sectorSize += 1       # first sectors receive the one-cell remainder

    sectors[sector] = cells[nextCell : nextCell + sectorSize]
    nextCell += sectorSize

    if sectorSize == 0:
        continue

    # This is a scan queue, not a single target.
    sort sectors[sector] using comparator(a, b):
        keyA = (
            path_cost(homeCoordinates, a),
            normalized_angle(atan2(a.row - home.row, a.col - home.col)),
            a.row, a.col
        )
        keyB = (
            path_cost(homeCoordinates, b),
            normalized_angle(atan2(b.row - home.row, b.col - home.col)),
            b.row, b.col
        )
        return keyA < keyB
```

The boundary between two sectors is the angle halfway between their adjacent cells. This is a discrete adaptation of radial ordering: it balances integer cell counts (not continuous geometric area) with a workload difference of at most one cell.

Each ant consumes its sector queue from nearest to farthest. It scans at each waypoint, pursues the nearest remembered food, returns home when carrying food, and resumes at the next queued waypoint. With smart scanning, it skips cells already covered by its own 7-by-7 scan; without it, it visits every ordered cell.

`USE_SMART_SCANNING` selects the sweep method: `0` visits every ordered sector cell; `1` skips a queued cell when that ant's own 7-by-7 scan already covered it.

## Assign longer sectors to higher-energy ants

Do not rank sectors by wedge angle. A narrow wedge often reaches farther toward a corner, but the actual terrain cost can differ. Rank using the greatest round-trip Dijkstra cost of any cell in the sector, before smart scanning skips waypoints:

```text
for each sector in sectors:
    sectorCost[sector] = 0

    for each cell in sectors[sector]:
        outbound = homeDistances[cell]  # calculated in this solution file
        roundTrip = 2 * outbound    # edge costs are symmetric
        sectorCost[sector] = max(sectorCost[sector], roundTrip)

sectorOrder = sector indices sorted by (sectorCost descending, sector index descending)
antOrder = ant indices sorted by (ants[index].energy descending, index descending)

for rank in 0 .. min(sectorOrder.size(), antOrder.size()) - 1:
    assignedSector[antOrder[rank]] = sectorOrder[rank]
```

The first ant in `antOrder` has the most starting energy and receives the sector with the highest required round-trip cost. Equal costs and equal energies favour higher indices.

### Alternative experiment: rank by angular width

Use this **instead of** path-cost ranking to test the geometric heuristic. It only has an effect for equal-area sectors; equal-angle sectors all have the same width.

```text
for each sector in sectors:
    firstAngle = angle of the first cell in the sector's radial ordering
    lastAngle = angle of the last cell in the sector's radial ordering
    sectorWidth[sector] = wrapped_difference(lastAngle, firstAngle)

sectorOrder = sector indices sorted by (sectorWidth ascending, sector index ascending)
antOrder = ant indices sorted by (ants[index].energy descending, index descending)

for rank in 0 .. min(sectorOrder.size(), antOrder.size()) - 1:
    assignedSector[antOrder[rank]] = sectorOrder[rank]
```

This gives the narrowest angular sector to the highest-energy ant. Compare its score over the same seeds against the Dijkstra-ranked version; keep it only if it wins.

## Sources

- J. M. Diaz-Banez, P. Perez-Lantero, and R. Fabila-Monroy, ["On the Number of Radial Orderings of Planar Point Sets"](https://arxiv.org/abs/1204.0547): defines radial ordering as the circular order of points by their angle around an observation point and defines a `k`-fan as `k` rays from one center.
- R. Nandakumar, ["Convex Regions and their 'Fairest' Equipartitioning Fans"](https://arxiv.org/abs/1208.6508): studies equal-area partitions of convex regions made by rays from a common origin.
- MIT, ["Areas in Polar Coordinates"](https://math.mit.edu/classes/18.089/Summer2009/Lecture18.pdf): gives the continuous-sector area formula `A = 1/2 integral r(theta)^2 dtheta` used by the geometric version.
