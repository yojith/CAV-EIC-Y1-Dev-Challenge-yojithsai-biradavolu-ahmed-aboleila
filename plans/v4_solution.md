# V4: sector search, opportunistic food collection

V4 is V1 with one restriction removed: **an ant may pursue food outside its assigned sector if its own scan has found it**. The ant still searches *only its assigned sector* when it has no remembered food. It does not patrol other sectors, communicate food coordinates, or use pheromones. Like V1, it no longer reserves energy for a return trip.

Think of sectors as search assignments, not fences. If an ant scanning near a sector boundary sees food just across it, V1 ignores that food; V4 may take it. An ant may even pursue food whose outbound path exceeds its remaining energy; `move()` will stop where energy permits. This can find food sooner but can also strand an ant or its carried food.

## What each ant knows

- All ants are given the terrain and home coordinate for path-cost calculations. The terrain map is not discovered by scanning.
- Each ant has its **own** list of scanned cells and remembered food coordinates. A 7-by-7 `foodScan()` adds currently visible food to that ant's memory. A remembered food coordinate is removed if a later scan of that location finds it empty.
- Food an ant has never scanned is unknown to it, even though the simulator holds the full food map. No ant reads another ant's memories. The `Plan` object stores these memories in separate slots by each ant's original index and keeps that mapping aligned when dead ants are removed.

## Initialization

V4 uses V1's setup. Sort all non-home cells by angle around home and split them into equal-*cell-count* sectors. Within each sector, queue cells by increasing terrain-aware cost from home (then angle and coordinates). Give the highest-energy ants the sectors with the largest maximum round-trip waypoint cost. `USE_ANGLE_RANKING=1` substitutes narrowest-angle-first sector ranking; `USE_SMART_SCANNING=1` (the default) skips queued cells already covered by that ant's own scan.

## One forage turn

```text
for each ant:                         # every ant acts once per forage() call
    mark this ant's current 7x7 area as scanned
    visibleFood = foodScan()          # only the scan reveals food
    remove remembered food now visible but no longer present
    remember every currently visible food location, including other sectors

    if ant is carrying food:
        move toward home
        continue

    knownFood = []
    for food in this ant's remembered food:
        outboundCost = shortest_path_cost(ant.position, food)
        knownFood.append((outboundCost, food))

    if knownFood is not empty:
        move toward the food with the smallest outboundCost
        forget that coordinate after the move
        continue                         # pickup occurs only if move() reaches the food cell

    waypoint = first cell in this ant's ordered sector queue
               not already covered by its scan (when smart scanning is on)
    if waypoint exists:
        move toward waypoint           # scan again on the next forage turn
    else if ant is not home:
        move toward home

if no ant spent energy for 10 consecutive turns:
    enter death spiral: use real adjacent and homeward moves to spend energy
```

One ant may collect food from another's sector, but the other ant is not notified; its own later scan must discover that the food is gone. Shortest paths may also cross sector boundaries. When an ant reaches home carrying food, the simulator credits the score. The death spiral uses `move()` and `returnHome()`, never directly edits energy, so an ant can remain alive if every adjacent move costs more than its remaining energy.

`-DSOLUTION=v4` compiles `solutions/v4_solution.cpp`; the no-option build compiles `src/applicant_solution.cpp` (V1). The original hypothesis was that V1's hard sector filter discards useful nearby food. V4 keeps the same search queue and adds no pheromone or handoff behavior, so benchmark differences mainly measure that relaxed food choice.
