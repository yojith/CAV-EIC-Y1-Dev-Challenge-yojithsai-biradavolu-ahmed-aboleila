# Private Sector Scanning

Each ant receives one precomputed sector and maintains its own scanned-cell grid. This is private state: an ant never reads another ant's scan results or food knowledge.

On each turn, an ant first scans within `foodRadius`. It records every in-bounds cell in that scan area as scanned. If it sees food whose route from its current location to the food and then home fits within its remaining energy, it goes to the cheapest such food. Otherwise it chooses the nearest-to-home unscanned cell in its sector that it can reach and return from, marks its scan area on arrival, and repeats. A carrying ant returns home before scanning for more food.

The visualizer updates every 2000 ms. Sector backgrounds alternate green and brown. A darker background indicates that at least one ant has scanned that cell; this combined shade is renderer-only and does not provide shared knowledge. It also overlays each ant's route trail and current position.

The existing 100-step cap remains unchanged. No tests are added or changed.
