# V4 solution proposal

V4 should test **soft sector ownership**, not another handoff scheme. V1 already chooses visible, remembered food by the energy cost of reaching it and returning home. V4 keeps that safety check, but lets an ant collect affordable food outside its assigned sector when it sees it. Sectors remain a starting bias so ants do not all search in the same direction.

- Run all ants each turn, as in V1. Each ant keeps its own scanned-cell map and remembered food; it never reads food outside `foodScan()` or another ant's memory.
- If carrying food, return home. Otherwise, prefer an affordable remembered food item with the lowest `current -> food -> home` terrain cost, regardless of sector.
- If no remembered food is affordable, choose a reachable search waypoint by **new cells revealed per unit of travel energy**. Give cells in the ant's assigned sector a modest priority, but allow another sector when its own has no useful waypoint. A waypoint is useful only if its 7-by-7 scan covers at least one cell that this ant has not scanned.
- Keep enough energy to return home from a search waypoint. Re-scan and re-plan after moving; do not assume food exists in unseen cells.
- Do not add pheromones to the first V4 implementation. At 40% food density, they may cost more travel than they save. If benchmarks show sparse maps are weak, try one clearly defined marker meaning (for example, a visible food cluster that the marking ant cannot afford), and compare with pheromones disabled.

The hypothesis is that V1's hard sector food filter and fixed-distance scan queue waste opportunities, especially when an ant has energy left but its own sector is already covered. V4 should be judged against V1/V2/V3 on identical seeds, board sizes, ant counts, and food densities. Compare both raw score and fraction of initial food delivered; do not tune solely to seed `12345`.

This is a proposal only. No V4 strategy code is implemented yet.
