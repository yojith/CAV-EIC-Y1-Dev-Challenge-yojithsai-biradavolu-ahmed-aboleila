# V3 Solution Rough Draft

similar to V2, we go one by one each ant scanning its sector, and when it's about to die, it goes to the closest cell in the next sector, and then moves toward the home , leaving its food there as it dies. The only difference is that it doesn't spend energy exploring and it doesnt drop pheremones at all.
have a togglable define that changes the algorithm slightly, so that instead of going to the next sector, it goes home and drops a pheremone before it dies where it dies. The next ant first searches for that pheremone, goes straight to the pheremone and picks up the nearest food to the pheremone and goes home. then it scans its own sector same as v2.

let this v3 algorithm also be toggleable define to allow easy comparison against v2