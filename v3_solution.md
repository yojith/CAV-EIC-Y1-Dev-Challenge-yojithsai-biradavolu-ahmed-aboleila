# V3.1 and V3.2 handoff strategies

V3 activates one ant at a time, as in V2. The active ant scans its sector, collects food it can reach, and returns home when it can. The two modes differ when it is **carrying food but no longer has enough energy to reach home**:

## V3.1: next-sector handoff

Compile `solutions/v3_1_solution.cpp` with `-DSOLUTION=v3_1`. The ant moves into the next sector, then spends its remaining energy moving toward a cell nearer home. If it dies while carrying food, the game drops that food at its final position. This mode does not drop pheromones or spend a separate phase exploring the next sector. The next ant may find the dropped food through its own scan; the handoff is not guaranteed.

The test runner calls this mode `v3_1`.

## V3.2: home-directed pheromone handoff

Compile `solutions/v3_2_solution.cpp` with `-DSOLUTION=v3_2`. Instead of entering the next sector, the ant follows its route toward home and places a pheromone at its current position as it moves. If it runs out of energy, it drops its carried food there. The following ant checks for a pheromone it can **see** in the previous sector, travels to it, scans for nearby food, and tries to collect it before resuming its own sector. Pheromones do not give ants global knowledge, and the next ant may never see the marker or reach the dropped food.

The test runner calls this mode `v3_2`.

Both modes are separate source files. The ordinary no-option build still compiles `src/applicant_solution.cpp`, not either V3 mode. The historical `evaluation_results.csv` calls these modes `v3-next` and `v3-home`; the report displays them as V3.1 and V3.2 without rewriting those results.
