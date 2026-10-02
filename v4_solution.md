# V4: sector search, opportunistic food collection

V4 is V1 with one restriction removed: **an ant may collect food outside its assigned sector if its own scan has found it and it can afford the trip home**. The ant still searches *only its assigned sector* when it has no affordable known food. It does not patrol other sectors, communicate food coordinates, or use pheromones.

Think of sectors as search assignments, not fences. If an ant scanning near a sector boundary sees food just across it, V1 ignores that food; V4 may take it. For example, an east-sector ant with 8 energy can collect west-sector food when getting there costs 2 energy and returning home costs 3. This spends less time walking to an assigned search waypoint when a deliverable food item is already known.

## What each ant knows

- All ants are given the terrain and home coordinate for path-cost calculations. The terrain map is not discovered by scanning.
- Each ant has its **own** list of scanned cells and remembered food coordinates. A 7-by-7 `foodScan()` adds currently visible food to that ant's memory. A remembered food coordinate is removed if a later scan of that location finds it empty.
- Food an ant has never scanned is unknown to it, even though the simulator holds the full food map. No ant reads another ant's memories. The `Plan` object stores these memories in separate slots by ant ID.

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

    affordableFood = []
    for food in this ant's remembered food:
        deliveryCost = shortest_path_cost(ant.position, food)
                     + shortest_path_cost(food, home)
        if deliveryCost <= ant.energy:
            affordableFood.append((deliveryCost, food))

    if affordableFood is not empty:
        move to the food with the smallest deliveryCost
        forget that coordinate after the move
        continue                         # pickup occurs in move(); deliver next turn

    waypoint = first cell in this ant's ordered sector queue
               not already covered by its scan (when smart scanning is on)
    if waypoint exists and
       shortest_path_cost(ant.position, waypoint)
         + shortest_path_cost(waypoint, home) <= ant.energy:
        move to waypoint                # scan again on the next forage turn
    else if ant is not home:
        move toward home
```

One ant may collect food from another's sector, but the other ant is not notified; its own later scan must discover that the food is gone. Shortest paths may also cross sector boundaries. When an ant reaches home carrying food, the simulator credits the score. The affordability check preserves enough energy for the planned return trip, but it does not guarantee that all food can be delivered or that the ant spends all its energy.

`-DSOLUTION=v4` compiles `solutions/v4_solution.cpp`; the no-option build compiles `src/applicant_solution.cpp` (V1). The original hypothesis was that V1's hard sector filter discards useful nearby food. V4 keeps the same search queue and adds no pheromone or handoff behavior, so benchmark differences mainly measure that relaxed food choice.
