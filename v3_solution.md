# V3 solution rough draft

V3 activates one ant at a time, as in V2. The active ant scans its sector, collects food it can reach, and returns home when it can. The two modes differ when it is **carrying food but no longer has enough energy to reach home**:

## Mode 1: next-sector handoff (default)

Set `USE_V3_STRATEGY=1` and `V3_HOME_PHEROMONE=0`. The ant moves into the next sector, then spends its remaining energy moving toward a cell nearer home. If it dies while carrying food, the game drops that food at its final position. This mode does not drop pheromones or spend a separate phase exploring the next sector. The next ant may find the dropped food through its own scan; the handoff is not guaranteed.

The test runner calls this mode `v3-next`.

## Mode 2: home-directed pheromone handoff

Set `USE_V3_STRATEGY=1` and `V3_HOME_PHEROMONE=1`. Instead of entering the next sector, the ant follows its route toward home and places a pheromone at its current position as it moves. If it runs out of energy, it drops its carried food there. The following ant checks for a pheromone it can **see** in the previous sector, travels to it, scans for nearby food, and tries to collect it before resuming its own sector. Pheromones do not give ants global knowledge, and the next ant may never see the marker or reach the dropped food.

The test runner calls this mode `v3-home`.

Both modes are V3, selected at compile time by `V3_HOME_PHEROMONE`; `USE_V3_STRATEGY=0` disables V3 entirely. Keep both runner entries when comparing the handoff designs, or use `v3-next` as the single default V3 representative when comparing four strategies.
