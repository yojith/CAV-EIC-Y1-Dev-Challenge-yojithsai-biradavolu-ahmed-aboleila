# Private Sector Scanning

Each ant receives one precomputed sector and maintains its own scanned-cell grid plus a list of food coordinates it has personally seen. This is private state: an ant never reads another ant's scan results or food knowledge.

On each turn, an ant first scans within `foodRadius`. It records every in-bounds cell in that scan area as scanned and updates its private in-sector food list. If a remembered food coordinate can be reached and returned from, it goes to the cheapest such food. Otherwise it chooses the next unscanned cell in its ordered sector queue that it can reach and return from. A carrying ant returns home before scanning for more food.

The visualizer waits for Enter before every forage action. Sector backgrounds alternate green and brown. A darker background indicates that at least one ant has scanned that cell; this combined shade is renderer-only and does not provide shared knowledge. It also overlays each ant's route trail and current position.

The existing 100-step cap remains unchanged. No tests are added or changed.
