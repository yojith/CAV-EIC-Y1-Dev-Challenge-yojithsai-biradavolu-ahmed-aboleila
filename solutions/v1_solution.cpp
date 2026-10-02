//
// Created by dusan on 9/15/26.
// V1: each ant searches an assigned sector and collects food it has seen there.
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

// Set to 0 to visit every cell in the ordered sector queue. Set to 1 to skip cells covered by this ant's 7x7 scan.
#ifndef USE_SMART_SCANNING
#define USE_SMART_SCANNING 1
#endif


namespace {
    // Terrain and home are known in advance. Food is learned only through each ant's scan.
    struct Plan {
        const AntWorld *world{};
        Coord home{-1, -1};
        std::size_t rows{};
        std::size_t cols{};
        // A separate ordered search queue and queue position for each ant ID.
        std::vector<std::vector<Coord>> queues;
        std::vector<std::size_t> nextWaypoint;
        // Each ant has its own scanned-cell map and remembered-food list; no sharing.
        std::vector<MapTemplate> scanned;
        std::vector<std::vector<Coord>> rememberedFood;
        std::vector<int> sectorForAnt;
        MapTemplate sectorForCell;
    };

    Plan plan;


    // Return an angle from 0 to 2*pi so cells can be ordered around home.
    double angleFrom(Coord origin, Coord point) {
        double angle = std::atan2(point.first - origin.first, point.second - origin.second);
        return angle < 0 ? angle + 2.0 * std::numbers::pi : angle;
    }

    // The game's path helpers account for both distance and changes in height.
    int pathCost(const MapTemplate &terrain, Coord from, Coord to) {
        return calculatePathCost(terrain, shortestPath(terrain, from, to));
    }

    // Rebuild the plan if the simulator starts a different world.
    bool isCurrentPlan(const AntWorld &world) {
        return plan.world == &world && plan.home == world.homeCoordinates &&
               plan.rows == world.terrainMap.size() && plan.cols == world.terrainMap.front().size();
    }

    // Split all non-home cells into equal-size sectors, then assign one to each ant.
    void makePlan(const AntWorld &world) {
        const std::size_t antCount = world.ants.size();
        // Ant IDs may have gaps after another ant dies, so size memory by the largest ID.
        std::size_t slots = 0;
        for (const Ant &ant: world.ants) {
            slots = std::max(slots, static_cast<std::size_t>(ant.id + 1));
        }
        std::vector<Coord> cells;
        // These costs use public terrain, not hidden food locations.
        MapTemplate homeCosts = shortestDistances(world.terrainMap, world.homeCoordinates);
        for (int row = 0; row < static_cast<int>(world.terrainMap.size()); ++row) {
            for (int col = 0; col < static_cast<int>(world.terrainMap.front().size()); ++col) {
                if (Coord{row, col} != world.homeCoordinates) {
                    cells.emplace_back(row, col);
                }
            }
        }

        // Sort by angle first. Row and column make ties deterministic.
        std::sort(cells.begin(), cells.end(), [&](Coord a, Coord b) {
            const double angleA = angleFrom(world.homeCoordinates, a);
            const double angleB = angleFrom(world.homeCoordinates, b);
            if (angleA != angleB) return angleA < angleB;
            if (a.first != b.first) return a.first < b.first;
            return a.second < b.second;
        });

        std::vector<std::vector<Coord>> sectors(antCount);
        MapTemplate sectorForCell(world.terrainMap.size(), std::vector<int>(world.terrainMap.front().size(), -1));
        std::vector<double> sectorWidth(antCount, 0.0);
        std::vector<int> sectorRoundTrip(antCount, 0);
        // Give every sector the same number of cells, plus at most one extra.
        const std::size_t base = cells.size() / antCount;
        const std::size_t remainder = cells.size() % antCount;
        std::size_t first = 0;
        for (std::size_t sector = 0; sector < antCount; ++sector) {
            const std::size_t count = base + (sector < remainder ? 1 : 0);
            for (std::size_t index = first; index < first + count; ++index) {
                sectors[sector].push_back(cells[index]);
            }
            first += count;
            for (Coord cell: sectors[sector]) sectorForCell[cell.first][cell.second] = static_cast<int>(sector);
            if (!sectors[sector].empty()) {
                sectorWidth[sector] = angleFrom(world.homeCoordinates, sectors[sector].back()) -
                                      angleFrom(world.homeCoordinates, sectors[sector].front());
                for (Coord waypoint: sectors[sector]) {
                    sectorRoundTrip[sector] = std::max(
                        sectorRoundTrip[sector], 2 * homeCosts[waypoint.first][waypoint.second]);
                }
            }
            // Within a sector, search cheap-to-reach cells before distant cells.
            std::sort(sectors[sector].begin(), sectors[sector].end(), [&](Coord a, Coord b) {
                const int costA = homeCosts[a.first][a.second];
                const int costB = homeCosts[b.first][b.second];
                if (costA != costB) return costA < costB;
                const double angleA = angleFrom(world.homeCoordinates, a);
                const double angleB = angleFrom(world.homeCoordinates, b);
                if (angleA != angleB) return angleA < angleB;
                if (a.first != b.first) return a.first < b.first;
                return a.second < b.second;
            });
        }

        // By default, difficult sectors (largest round trip) are assigned first.
        // The optional angle mode instead assigns the narrowest sectors first.
        std::vector<int> sectorRank;
        for (int sector = 0; sector < static_cast<int>(antCount); ++sector) {
            sectorRank.push_back(sector);
        }
        std::sort(sectorRank.begin(), sectorRank.end(), [&](int a, int b) {
#if USE_ANGLE_RANKING
            if (sectorWidth[a] != sectorWidth[b]) return sectorWidth[a] < sectorWidth[b];
            return a < b;
#else
            if (sectorRoundTrip[a] != sectorRoundTrip[b]) return sectorRoundTrip[a] > sectorRoundTrip[b];
            return a > b;
#endif
        });

        // Higher-energy ants get earlier picks. A higher list index breaks ties.
        std::vector<int> antRank;
        for (int index = 0; index < static_cast<int>(antCount); ++index) {
            antRank.push_back(index);
        }
        std::sort(antRank.begin(), antRank.end(), [&](int a, int b) {
            if (world.ants[a].energy != world.ants[b].energy) {
                return world.ants[a].energy > world.ants[b].energy;
            }
            return a > b;
        });

        // Allocate private memory slots by ant ID, then fill their search queues.
        plan = {};
        plan.world = &world;
        plan.home = world.homeCoordinates;
        plan.rows = world.terrainMap.size();
        plan.cols = world.terrainMap.front().size();
        plan.queues.resize(slots);
        plan.nextWaypoint.assign(slots, 0);
        plan.scanned.assign(slots, MapTemplate(plan.rows, std::vector<int>(plan.cols, 0)));
        plan.rememberedFood.resize(slots);
        plan.sectorForAnt.assign(slots, -1);
        plan.sectorForCell = std::move(sectorForCell);
        for (std::size_t rank = 0; rank < antCount; ++rank) {
            const std::size_t antId = static_cast<std::size_t>(world.ants[antRank[rank]].id);
            const int sector = sectorRank[rank];
            plan.queues[antId] = std::move(sectors[sector]);
            plan.sectorForAnt[antId] = sector;
        }
    }

    // A trip is affordable only if the ant can also return to home afterward.
    bool canReachAndReturn(const AntWorld &world, const Ant &ant, Coord destination) {
        return pathCost(world.terrainMap, ant.position, destination) +
               pathCost(world.terrainMap, destination, world.homeCoordinates) <= ant.energy;
    }

    // Refresh only this ant's food memory using its current 7-by-7 view.
    void rememberVisibleFood(AntWorld &world, Ant &ant) {
        std::vector<Coord> visible = ant.foodScan(world.foodMap);
        std::vector<Coord> &memory = plan.rememberedFood[ant.id];
        // If a remembered tile is visible but now empty, forget it.
        memory.erase(std::remove_if(memory.begin(), memory.end(), [&](Coord food) {
            const bool inCurrentScan = std::abs(food.first - ant.position.first) <= ant.foodRadius &&
                                       std::abs(food.second - ant.position.second) <= ant.foodRadius;
            return inCurrentScan && std::find(visible.begin(), visible.end(), food) == visible.end();
        }), memory.end());

        // V1 only remembers food inside this ant's assigned sector.
        for (Coord food: visible) {
            if (plan.sectorForCell[food.first][food.second] == plan.sectorForAnt[ant.id] &&
                std::find(memory.begin(), memory.end(), food) == memory.end()) {
                memory.push_back(food);
            }
        }
    }

    // Pick the cheapest remembered food that can still be delivered home.
    Coord bestRememberedFood(const AntWorld &world, const Ant &ant) {
        Coord best{-1, -1};
        int bestCost = 0;
        for (Coord food: plan.rememberedFood[ant.id]) {
            const int cost = pathCost(world.terrainMap, ant.position, food) +
                             pathCost(world.terrainMap, food, world.homeCoordinates);
            if (cost <= ant.energy && (best.first == -1 || cost < bestCost)) {
                best = food;
                bestCost = cost;
            }
        }
        return best;
    }

    void forgetFood(const Ant &ant, Coord food) {
        std::vector<Coord> &memory = plan.rememberedFood[ant.id];
        memory.erase(std::remove(memory.begin(), memory.end(), food), memory.end());
    }

    // A scan covers a square around the ant, clipped at the board edge.
    void markLocalScan(const AntWorld &world, const Ant &ant) {
        MapTemplate &scanned = plan.scanned[ant.id];
        for (int row = ant.position.first - ant.foodRadius; row <= ant.position.first + ant.foodRadius; ++row) {
            for (int col = ant.position.second - ant.foodRadius; col <= ant.position.second + ant.foodRadius; ++col) {
                if (row >= 0 && row < static_cast<int>(world.terrainMap.size()) &&
                    col >= 0 && col < static_cast<int>(world.terrainMap.front().size())) {
                    scanned[row][col] = 1;
                }
            }
        }
    }

    // Keep the original queue order; smart scanning only skips already-seen cells.
    Coord nextQueuedWaypoint(const AntWorld &world, const Ant &ant) {
        std::size_t &next = plan.nextWaypoint[ant.id];
        const std::vector<Coord> &queue = plan.queues[ant.id];
#if USE_SMART_SCANNING
        while (next < queue.size() && plan.scanned[ant.id][queue[next].first][queue[next].second]) {
            ++next;
        }
#endif
        if (next == queue.size()) return {-1, -1};

        const Coord waypoint = queue[next];
        return canReachAndReturn(world, ant, waypoint) ? waypoint : Coord{-1, -1};
    }


} // namespace


/** @brief Runs the selected sector-foraging strategy. */
void AntWorld::forage() {
    if (ants.empty()) return;

    // ponytail: one active world plan; key plans by world lifetime if concurrent simulations are needed.
    if (!isCurrentPlan(*this)) makePlan(*this);


    for (Ant &ant: ants) {
        // First update this ant's private knowledge from its present position.
        markLocalScan(*this, ant);
        rememberVisibleFood(*this, ant);
        // A carrier's first job is delivery.
        if (ant.carryingFood) {
            ant.returnHome(terrainMap, foodMap);
            continue;
        }

        // Known affordable food takes priority over further exploration.
        const Coord food = bestRememberedFood(*this, ant);
        if (food.first != -1) {
            ant.move(terrainMap, food, foodMap);
            forgetFood(ant, food);
            continue;
        }

        // Otherwise continue through this ant's own sector queue.
        const Coord waypoint = nextQueuedWaypoint(*this, ant);
        if (waypoint.first != -1) {
            ant.move(terrainMap, waypoint, foodMap);
#if !USE_SMART_SCANNING
            ++plan.nextWaypoint[ant.id];
#endif
            continue;
        }

        // No affordable queue target remains, so stop wandering away from home.
        if (ant.position != homeCoordinates) {
            ant.returnHome(terrainMap, foodMap);
        }
    }
}
