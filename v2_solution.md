# V2 solution rough draft

-Want to split the map into sectors of equal area based on the number of ants, since placement of house is arbitrary, dividing sectors would depend on a sweeping arm that would calculate the area incrementally until each  sector size is achieved then it would set the boundary (use code from the v1)
-Only one ant will be collecting food at a time
-Ants will  begin by moving away from the house as much as possible until they encounter food. prioritize collecting food in the beginning, starting with what is closest to the home, then expanding outwards
-Once they reach a food item within their radius of vision, they have to calculate the energy required to grab that food and the energy required to go back home, and compare it with the amount of energy they have left.
-If the ants realize that they don't have enough energy to make it back home, they will still pick up that food but what they will do is move into the next ant's sector (next sector counter clockwise)
-Once they reach there, their goal is to use half the remaining energy to explore and drop a pheromone in the region with the most abundant food in the next ant's sector, something it does by checking the cells in the next ants sector for the cell closest to the ant. then use the remaining half of their energy to deliver the food they are carrying (originally from their own sector) as close to the house (still in the  following/second ant's sector sector). so that its easier for the next ant to pick up this food from where the original ant died
-The next ant then follows the same playbook such that it prioritizes food  at the beginning then, discover, then delivery
-An ant cannot cross into the previous sector even if it senses that a pheromone is there
-Ants know the boarders of their sectors and they cannot cross into other sectors unless they reach the second part of their life span
-Ants leave in a decreasing energy order. The first ant that searches the first sector is the one with the most energy, and the ant that checks the last sector is the one with the least amount of energy. (The goal is to find the lowest energy path to be as close to home while dying in the next sector)

- ** possible optimization area is figuring out which sector to leave from as well. start at narrowest, or the widest?

-Once an ant picks up food, it remebers where it picked it up, and uses the dijkstra algorithm to find the shortest path back rather than sweeping the same path again
-When sweeping, the ant moves in 3 unit cell segments, unless it scans food in its radius, and recalculates the most optimal route before evry step, while remaining on its general course (make sure ants do not leave their sector while search and collecting food to take home)
