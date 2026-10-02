//
// Created by dusan on 9/15/26.
//
// V4.1: sector search with path-aware intermediate scans and opportunistic food collection.
//

#include "../include/antworld.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>


// Set to 1 (or compile with -DUSE_ANGLE_RANKING=1) to rank narrow sectors first.
#ifndef USE_ANGLE_RANKING
#define USE_ANGLE_RANKING 0
#endif


// V4.1 is designed around smart scanning.
// When enabled, already-scanned cells are skipped as exploration targets.
#ifndef USE_SMART_SCANNING
#define USE_SMART_SCANNING 1
#endif


namespace {

    // Terrain and home are known in advance.
    // Food is learned only through each ant's own foodScan().
    struct Plan {
        const AntWorld *world{};
        Coord home{-1, -1};

        std::size_t rows{};
        std::size_t cols{};

        // Ordered list of cells belonging to each ant's assigned sector.
        // The order is based on home cost, then radial angle, then coordinates.
        std::vector<std::vector<Coord>> queues;

        // Each ant has its own scanned-cell map.
        std::vector<MapTemplate> scanned;

        // Each ant has its own remembered-food list.
        // No food memory is shared between ants.
        std::vector<std::vector<Coord>> rememberedFood;

        // Assigned sector for each ant ID.
        std::vector<int> sectorForAnt;

        // Sector ownership for every board cell.
        MapTemplate sectorForCell;
    };


    Plan plan;


    // Return an angle from 0 to 2*pi so cells can be ordered around home.
    double angleFrom(Coord origin, Coord point) {
        double angle = std::atan2(
            point.first - origin.first,
            point.second - origin.second
        );

        return angle < 0
            ? angle + 2.0 * std::numbers::pi
            : angle;
    }


    // Return the terrain-aware energy cost of the shortest path.
    int pathCost(const MapTemplate &terrain, Coord from, Coord to) {
        return calculatePathCost(
            terrain,
            shortestPath(terrain, from, to)
        );
    }


    // Rebuild the plan if a different AntWorld is running.
    bool isCurrentPlan(const AntWorld &world) {
        return plan.world == &world &&
               plan.home == world.homeCoordinates &&
               plan.rows == world.terrainMap.size() &&
               plan.cols == world.terrainMap.front().size();
    }


    // Split all non-home cells into equal-cell-count angular sectors,
    // then assign one sector to each ant.
    void makePlan(const AntWorld &world) {
        const std::size_t antCount = world.ants.size();

        // Ant IDs may have gaps after ants die.
        // Size private-memory arrays using the largest current ant ID.
        std::size_t slots = 0;

        for (const Ant &ant : world.ants) {
            slots = std::max(
                slots,
                static_cast<std::size_t>(ant.id + 1)
            );
        }


        std::vector<Coord> cells;

        // Terrain is known, so this can be calculated once for sector ordering.
        MapTemplate homeCosts =
            shortestDistances(world.terrainMap, world.homeCoordinates);


        for (int row = 0;
             row < static_cast<int>(world.terrainMap.size());
             ++row) {

            for (int col = 0;
                 col < static_cast<int>(world.terrainMap.front().size());
                 ++col) {

                if (Coord{row, col} != world.homeCoordinates) {
                    cells.emplace_back(row, col);
                }
            }
        }


        // Sweep around home by angle.
        std::sort(
            cells.begin(),
            cells.end(),
            [&](Coord a, Coord b) {

                const double angleA =
                    angleFrom(world.homeCoordinates, a);

                const double angleB =
                    angleFrom(world.homeCoordinates, b);

                if (angleA != angleB)
                    return angleA < angleB;

                if (a.first != b.first)
                    return a.first < b.first;

                return a.second < b.second;
            }
        );


        std::vector<std::vector<Coord>> sectors(antCount);

        MapTemplate sectorForCell(
            world.terrainMap.size(),
            std::vector<int>(
                world.terrainMap.front().size(),
                -1
            )
        );

        std::vector<double> sectorWidth(antCount, 0.0);
        std::vector<int> sectorRoundTrip(antCount, 0);


        // Give every sector the same number of cells,
        // with at most one extra cell in some sectors.
        const std::size_t base =
            cells.size() / antCount;

        const std::size_t remainder =
            cells.size() % antCount;

        std::size_t first = 0;


        for (std::size_t sector = 0;
             sector < antCount;
             ++sector) {

            const std::size_t count =
                base + (sector < remainder ? 1 : 0);


            for (std::size_t index = first;
                 index < first + count;
                 ++index) {

                sectors[sector].push_back(cells[index]);
            }

            first += count;


            // Record which sector owns each cell.
            for (Coord cell : sectors[sector]) {
                sectorForCell[cell.first][cell.second] =
                    static_cast<int>(sector);
            }


            if (!sectors[sector].empty()) {

                sectorWidth[sector] =
                    angleFrom(
                        world.homeCoordinates,
                        sectors[sector].back()
                    )
                    -
                    angleFrom(
                        world.homeCoordinates,
                        sectors[sector].front()
                    );


                for (Coord waypoint : sectors[sector]) {
                    sectorRoundTrip[sector] =
                        std::max(
                            sectorRoundTrip[sector],
                            2 * homeCosts
                                [waypoint.first]
                                [waypoint.second]
                        );
                }
            }


            // Preserve V4's deterministic in-sector ordering:
            // home cost -> angle -> row -> column.
            std::sort(
                sectors[sector].begin(),
                sectors[sector].end(),
                [&](Coord a, Coord b) {

                    const int costA =
                        homeCosts[a.first][a.second];

                    const int costB =
                        homeCosts[b.first][b.second];

                    if (costA != costB)
                        return costA < costB;


                    const double angleA =
                        angleFrom(world.homeCoordinates, a);

                    const double angleB =
                        angleFrom(world.homeCoordinates, b);

                    if (angleA != angleB)
                        return angleA < angleB;

                    if (a.first != b.first)
                        return a.first < b.first;

                    return a.second < b.second;
                }
            );
        }


        // Rank sectors using the same V4 rule.
        std::vector<int> sectorRank;

        for (int sector = 0;
             sector < static_cast<int>(antCount);
             ++sector) {

            sectorRank.push_back(sector);
        }


        std::sort(
            sectorRank.begin(),
            sectorRank.end(),
            [&](int a, int b) {

#if USE_ANGLE_RANKING

                if (sectorWidth[a] != sectorWidth[b])
                    return sectorWidth[a] < sectorWidth[b];

                return a < b;

#else

                if (sectorRoundTrip[a] != sectorRoundTrip[b])
                    return sectorRoundTrip[a] > sectorRoundTrip[b];

                return a > b;

#endif
            }
        );


        // Higher-energy ants get earlier picks.
        std::vector<int> antRank;

        for (int index = 0;
             index < static_cast<int>(antCount);
             ++index) {

            antRank.push_back(index);
        }


        std::sort(
            antRank.begin(),
            antRank.end(),
            [&](int a, int b) {

                if (world.ants[a].energy != world.ants[b].energy) {
                    return world.ants[a].energy >
                           world.ants[b].energy;
                }

                return a > b;
            }
        );


        // Allocate private memory for every ant.
        plan = {};

        plan.world = &world;
        plan.home = world.homeCoordinates;
        plan.rows = world.terrainMap.size();
        plan.cols = world.terrainMap.front().size();

        plan.queues.resize(slots);

        plan.scanned.assign(
            slots,
            MapTemplate(
                plan.rows,
                std::vector<int>(plan.cols, 0)
            )
        );

        plan.rememberedFood.resize(slots);

        plan.sectorForAnt.assign(slots, -1);

        plan.sectorForCell =
            std::move(sectorForCell);


        // Match ranked ants to ranked sectors.
        for (std::size_t rank = 0;
             rank < antCount;
             ++rank) {

            const std::size_t antId =
                static_cast<std::size_t>(
                    world.ants[antRank[rank]].id
                );

            const int sector =
                sectorRank[rank];

            plan.queues[antId] =
                std::move(sectors[sector]);

            plan.sectorForAnt[antId] =
                sector;
        }
    }


    // True if the ant can reach destination and still return home.
    bool canReachAndReturn(
        const AntWorld &world,
        const Ant &ant,
        Coord destination
    ) {
        return
            pathCost(
                world.terrainMap,
                ant.position,
                destination
            )
            +
            pathCost(
                world.terrainMap,
                destination,
                world.homeCoordinates
            )
            <=
            ant.energy;
    }


    // Mark the ant's current 7x7 food-view area as scanned.
    void markLocalScan(
        const AntWorld &world,
        const Ant &ant
    ) {
        MapTemplate &scanned =
            plan.scanned[ant.id];


        for (int row =
                 ant.position.first - ant.foodRadius;
             row <=
                 ant.position.first + ant.foodRadius;
             ++row) {

            for (int col =
                     ant.position.second - ant.foodRadius;
                 col <=
                     ant.position.second + ant.foodRadius;
                 ++col) {

                if (
                    row >= 0 &&
                    row <
                        static_cast<int>(
                            world.terrainMap.size()
                        )
                    &&
                    col >= 0 &&
                    col <
                        static_cast<int>(
                            world.terrainMap.front().size()
                        )
                ) {
                    scanned[row][col] = 1;
                }
            }
        }
    }


    // Update only this ant's private food memory using its current scan.
    void rememberVisibleFood(
        AntWorld &world,
        Ant &ant
    ) {
        std::vector<Coord> visible =
            ant.foodScan(world.foodMap);

        std::vector<Coord> &memory =
            plan.rememberedFood[ant.id];


        // If a previously remembered food location is currently
        // visible but the food is gone, remove it from memory.
        memory.erase(
            std::remove_if(
                memory.begin(),
                memory.end(),
                [&](Coord food) {

                    const bool inCurrentScan =
                        std::abs(
                            food.first -
                            ant.position.first
                        )
                        <=
                        ant.foodRadius
                        &&
                        std::abs(
                            food.second -
                            ant.position.second
                        )
                        <=
                        ant.foodRadius;


                    return
                        inCurrentScan
                        &&
                        std::find(
                            visible.begin(),
                            visible.end(),
                            food
                        )
                        ==
                        visible.end();
                }
            ),
            memory.end()
        );


        // V4.1 keeps V4's opportunistic collection rule:
        // remember visible food even if it belongs to another sector.
        for (Coord food : visible) {

            if (
                std::find(
                    memory.begin(),
                    memory.end(),
                    food
                )
                ==
                memory.end()
            ) {
                memory.push_back(food);
            }
        }
    }


    // Pick the remembered food with the smallest total
    // current -> food -> home energy cost.
    Coord bestRememberedFood(
        const AntWorld &world,
        const Ant &ant
    ) {
        Coord best{-1, -1};

        int bestCost = 0;


        for (Coord food :
             plan.rememberedFood[ant.id]) {

            const int cost =
                pathCost(
                    world.terrainMap,
                    ant.position,
                    food
                )
                +
                pathCost(
                    world.terrainMap,
                    food,
                    world.homeCoordinates
                );


            if (
                cost <= ant.energy
                &&
                (
                    best.first == -1
                    ||
                    cost < bestCost
                )
            ) {
                best = food;
                bestCost = cost;
            }
        }


        return best;
    }


    // Remove a food coordinate from this ant's private memory.
    void forgetFood(
        const Ant &ant,
        Coord food
    ) {
        std::vector<Coord> &memory =
            plan.rememberedFood[ant.id];

        memory.erase(
            std::remove(
                memory.begin(),
                memory.end(),
                food
            ),
            memory.end()
        );
    }


    // Choose the cheapest currently-unscanned cell in this ant's
    // assigned sector that can still be reached and returned from.
    //
    // The sector's original V4 ordering acts as the tie-break:
    // home cost -> angle -> coordinates.
    Coord bestUnscannedSectorCell(
        const AntWorld &world,
        const Ant &ant
    ) {
        const std::vector<Coord> &queue =
            plan.queues[ant.id];

        Coord best{-1, -1};

        int bestMoveCost = 0;


        for (Coord candidate : queue) {

#if USE_SMART_SCANNING

            if (
                plan.scanned[ant.id]
                            [candidate.first]
                            [candidate.second]
                != 0
            ) {
                continue;
            }

#endif


            if (
                !canReachAndReturn(
                    world,
                    ant,
                    candidate
                )
            ) {
                continue;
            }


            const int moveCost =
                pathCost(
                    world.terrainMap,
                    ant.position,
                    candidate
                );


            // Only replace the current best if the move is strictly cheaper.
            // If costs tie, the earlier cell in the V4 queue keeps priority.
            if (
                best.first == -1
                ||
                moveCost < bestMoveCost
            ) {
                best = candidate;
                bestMoveCost = moveCost;
            }
        }


        return best;
    }


    // While exploring toward an unscanned sector target, inspect the
    // calculated route and return the FIRST unscanned cell belonging
    // to this ant's own sector.
    //
    // Stopping there creates an intermediate scan on the next forage()
    // turn. Cells outside the assigned sector do not trigger an
    // intermediate scan, even if the shortest path passes through them.
    Coord firstUnscannedCellOnExplorationPath(
        const AntWorld &world,
        const Ant &ant,
        const std::vector<Coord> &path
    ) {
        const int ownSector =
            plan.sectorForAnt[ant.id];


        // path[0] is the ant's current position,
        // which has already been scanned this turn.
        for (std::size_t index = 1;
             index < path.size();
             ++index) {

            const Coord cell =
                path[index];


            const bool inOwnSector =
                plan.sectorForCell
                    [cell.first]
                    [cell.second]
                ==
                ownSector;


            const bool unscanned =
                plan.scanned[ant.id]
                            [cell.first]
                            [cell.second]
                ==
                0;


            if (inOwnSector && unscanned) {
                return cell;
            }
        }


        return {-1, -1};
    }

} // namespace



/**
 * @brief V4.1 sector-search strategy.
 *
 * Key behavior:
 *  - Each ant keeps private scan and food memory.
 *  - Known affordable food always takes priority.
 *  - Food trips and return-home trips are direct: no intermediate scans.
 *  - When exploring, the ant chooses the cheapest currently-unscanned
 *    cell in its assigned sector.
 *  - If the exploration path passes through another unscanned cell in
 *    that same sector, the ant stops there first so it can scan on the
 *    next forage() call.
 */
void AntWorld::forage() {

    if (ants.empty())
        return;


    if (!isCurrentPlan(*this)) {
        makePlan(*this);
    }


    for (Ant &ant : ants) {

        /*
         * IMPORTANT:
         * Check carryingFood BEFORE scanning.
         *
         * If the ant collected food outside its assigned sector on the
         * previous turn, it should return home immediately instead of
         * scanning that neighboring sector and learning more food there.
         */
        if (ant.carryingFood) {
            ant.returnHome(
                terrainMap,
                foodMap
            );

            continue;
        }


        /*
         * Normal observation stage.
         *
         * Every non-carrying ant scans the 7x7 area around its current
         * position and updates only its own private memory.
         */
        markLocalScan(
            *this,
            ant
        );

        rememberVisibleFood(
            *this,
            ant
        );


        /*
         * FOOD PRIORITY
         *
         * Choose the remembered affordable food with the smallest
         * total delivery cost:
         *
         * current position -> food -> home
         *
         * This movement is DIRECT. There are no intermediate scans
         * while travelling to food.
         */
        const Coord food =
            bestRememberedFood(
                *this,
                ant
            );


        if (food.first != -1) {

            ant.move(
                terrainMap,
                food,
                foodMap
            );

            forgetFood(
                ant,
                food
            );

            continue;
        }


        /*
         * EXPLORATION
         *
         * No affordable remembered food exists.
         *
         * Select the cheapest currently-unscanned cell in the ant's
         * assigned sector.
         */
        const Coord target =
            bestUnscannedSectorCell(
                *this,
                ant
            );


        if (target.first != -1) {

            const std::vector<Coord> path =
                shortestPath(
                    terrainMap,
                    ant.position,
                    target
                );


            /*
             * INTERMEDIATE SCAN LOGIC
             *
             * If the route passes through an unscanned cell in this
             * ant's OWN sector, stop at the first such cell.
             *
             * On the next forage() call:
             *   1. the ant scans from that cell,
             *   2. newly-covered cells are marked,
             *   3. newly-seen food is remembered,
             *   4. the route/target is recalculated.
             *
             * Therefore the old path is never blindly resumed.
             */
            const Coord intermediate =
                firstUnscannedCellOnExplorationPath(
                    *this,
                    ant,
                    path
                );


            if (intermediate.first != -1) {

                ant.move(
                    terrainMap,
                    intermediate,
                    foodMap
                );

            } else {

                // Every own-sector cell along the path is already scanned,
                // so travelling directly to the chosen target is safe.
                ant.move(
                    terrainMap,
                    target,
                    foodMap
                );
            }


            continue;
        }


        /*
         * No affordable unscanned sector target remains.
         *
         * Stop travelling farther away and return home.
         * This return is direct and does not use intermediate scans.
         */
        if (ant.position != homeCoordinates) {

            ant.returnHome(
                terrainMap,
                foodMap
            );
        }
    }
}
