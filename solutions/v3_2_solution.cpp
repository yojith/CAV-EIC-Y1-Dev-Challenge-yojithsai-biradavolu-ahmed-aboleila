//
// Created by dusan on 9/15/26.
// V3.2: an exhausted carrier marks its homeward route with pheromones.
//

#include "../include/antworld.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

// Set to 1 (or compile with -DUSE_ANGLE_RANKING=1) to rank narrow sectors first.
#ifndef USE_ANGLE_RANKING
#define USE_ANGLE_RANKING 0
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

    // V3.2 runs one ant at a time and tracks homeward pheromone handoffs.
    struct ForageState {
        std::vector<int> order;
        std::size_t active{};
        std::vector<int> phase; // 0: collect, 3: finished, 4: mark homeward route, 5: follow marker
        std::vector<Coord> explorationTarget;
        std::vector<std::vector<Coord>> visitedPheromone;
    } state;

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

        // Rank sectors to choose where the first ant starts. Later ants follow clockwise.
        // The optional angle mode starts at the narrowest sector instead.
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

        // Higher-energy ants act first. A higher list index breaks ties.
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
        state = {};
        state.phase.assign(slots, 0);
        state.explorationTarget.assign(slots, {-1, -1});
        state.visitedPheromone.assign(slots, {});
        for (std::size_t rank = 0; rank < antCount; ++rank) {
            const std::size_t antId = static_cast<std::size_t>(world.ants[antRank[rank]].id);
            const int sector = (sectorRank.front() + static_cast<int>(rank)) % static_cast<int>(antCount);
            state.order.push_back(static_cast<int>(antId));
            plan.queues[antId] = std::move(sectors[sector]);
            plan.sectorForAnt[antId] = sector;
        }
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

        for (Coord food: visible) {
            if (plan.sectorForCell[food.first][food.second] == plan.sectorForAnt[ant.id] &&
                std::find(memory.begin(), memory.end(), food) == memory.end()) {
                memory.push_back(food);
            }
        }
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


    // Move at most three path cells this turn, stopping early after a food pickup.
    void moveSegment(AntWorld &world, Ant &ant, const std::vector<Coord> &path) {
        const bool wasCarrying = ant.carryingFood;
        for (std::size_t step = 1; step < path.size() && step <= 3; ++step) {
            const Coord next = path[step];
            const int cost = 1 + std::abs(world.terrainMap[ant.position.first][ant.position.second] -
                                           world.terrainMap[next.first][next.second]);
            if (ant.energy < cost) break;
            ant.move(world.terrainMap, next, world.foodMap);
            if (!wasCarrying && ant.carryingFood) break;
        }
    }

    // A reachable food item is worth trying even if the return trip may need a handoff.
    Coord knownFoodTarget(const AntWorld &world, const Ant &ant) {
        Coord best{-1, -1};
        int bestCost = std::numeric_limits<int>::max();
        for (Coord food: plan.rememberedFood[ant.id]) {
            const std::vector<Coord> route = shortestPath(world.terrainMap, ant.position, food);
            if (route.empty()) continue;
            const int cost = calculatePathCost(world.terrainMap, route);
            if (cost <= ant.energy && cost < bestCost) {
                best = food;
                bestCost = cost;
            }
        }
        return best;
    }

    // Continue through this ant's queue, skipping previously scanned cells.
    Coord nextWaypoint(const AntWorld &world, const Ant &ant) {
        std::size_t &cursor = plan.nextWaypoint[ant.id];
        const std::vector<Coord> &queue = plan.queues[ant.id];
        while (cursor < queue.size()) {
            const Coord cell = queue[cursor];
            if (plan.scanned[ant.id][cell.first][cell.second]) {
                ++cursor;
                continue;
            }
            const std::vector<Coord> route = shortestPath(world.terrainMap, ant.position, cell);
            if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) return cell;
            ++cursor;
        }
        return {-1, -1};
    }


    // Only the active ant acts. When it finishes, the following ant takes over.
    void forageV3(AntWorld &world) {
        while (state.active < state.order.size()) {
            const int id = state.order[state.active];
            const auto it = std::find_if(world.ants.begin(), world.ants.end(),
                                         [&](const Ant &candidate) { return candidate.id == id; });
            if (it != world.ants.end() && state.phase[id] != 3 && it->energy > 0) break;
            ++state.active;
        }
        if (state.active == state.order.size()) return;
        Ant &ant = *std::find_if(world.ants.begin(), world.ants.end(), [&](const Ant &candidate) {
            return candidate.id == state.order[state.active];
        });
        markLocalScan(world, ant);
        rememberVisibleFood(world, ant);
        const int ownSector = plan.sectorForAnt[ant.id];
        const int nextSector = (ownSector + 1) % static_cast<int>(state.order.size());

        if (ant.carryingFood && state.phase[ant.id] == 5) state.phase[ant.id] = 0;
        // A carrier that cannot get home switches to a pheromone-marking route.
        if (state.phase[ant.id] == 0 && ant.carryingFood &&
            pathCost(world.terrainMap, ant.position, world.homeCoordinates) > ant.energy) {
            state.phase[ant.id] = 4;
        }

        // Phase 4: move homeward as far as possible and mark the path.
        if (state.phase[ant.id] == 4) {
            const Coord from = ant.position;
            ant.returnHome(world.terrainMap, world.foodMap);
            if (ant.position == world.homeCoordinates) {
                state.phase[ant.id] = 0;
                return;
            }
            if (ant.position == from && ant.energy > 0) {
                Coord neighbor{-1, -1};
                int closest = std::numeric_limits<int>::max();
                for (Coord delta: {Coord{-1, 0}, Coord{1, 0}, Coord{0, -1}, Coord{0, 1}}) {
                    const Coord cell{ant.position.first + delta.first, ant.position.second + delta.second};
                    if (cell.first < 0 || cell.first >= static_cast<int>(world.terrainMap.size()) ||
                        cell.second < 0 || cell.second >= static_cast<int>(world.terrainMap.front().size())) continue;
                    const int cost = 1 + std::abs(world.terrainMap[ant.position.first][ant.position.second] -
                                                  world.terrainMap[cell.first][cell.second]);
                    if (cost > ant.energy) continue;
                    const int homeCost = pathCost(world.terrainMap, cell, world.homeCoordinates);
                    if (homeCost < closest) {
                        neighbor = cell;
                        closest = homeCost;
                    }
                }
                if (neighbor.first >= 0) {
                    ant.move(world.terrainMap, neighbor, world.foodMap);
                } else {
                    state.phase[ant.id] = 3;
                }
            }
            ant.dropPheromone(world.pheromoneMap);
            return;
        }

        // Phase 5: follow a visible marker and check whether food was dropped nearby.
        if (state.phase[ant.id] == 5) {
            const Coord marker = state.explorationTarget[ant.id];
            if (ant.position != marker) {
                const auto route = shortestPath(world.terrainMap, ant.position, marker);
                if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) {
                    moveSegment(world, ant, route);
                    return;
                }
            } else {
                Coord food{-1, -1};
                int bestCost = std::numeric_limits<int>::max();
                for (Coord candidate: ant.foodScan(world.foodMap)) {
                    const auto route = shortestPath(world.terrainMap, ant.position, candidate);
                    if (route.empty()) continue;
                    const int cost = calculatePathCost(world.terrainMap, route);
                    if (cost <= ant.energy && cost < bestCost) {
                        food = candidate;
                        bestCost = cost;
                    }
                }
                if (food.first >= 0) {
                    moveSegment(world, ant, shortestPath(world.terrainMap, ant.position, food));
                    return;
                }
            }
            state.visitedPheromone[ant.id].push_back(marker);
            state.phase[ant.id] = 0;
        }

        if (ant.carryingFood) {
            ant.returnHome(world.terrainMap, world.foodMap);
            return;
        }

        const int previousSector = (ownSector + static_cast<int>(state.order.size()) - 1) %
                                   static_cast<int>(state.order.size());
        // Only pheromones in scan range can guide the next ant.
        for (Coord marker: ant.pheromoneScan(world.pheromoneMap)) {
            if (plan.sectorForCell[marker.first][marker.second] != previousSector ||
                std::find(state.visitedPheromone[ant.id].begin(), state.visitedPheromone[ant.id].end(), marker) !=
                    state.visitedPheromone[ant.id].end()) continue;
            state.explorationTarget[ant.id] = marker;
            state.phase[ant.id] = 5;
            const auto route = shortestPath(world.terrainMap, ant.position, marker);
            if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) {
                moveSegment(world, ant, route);
                return;
            }
            state.visitedPheromone[ant.id].push_back(marker);
            state.phase[ant.id] = 0;
        }

        const Coord food = knownFoodTarget(world, ant);
        if (food.first >= 0) {
            moveSegment(world, ant, shortestPath(world.terrainMap, ant.position, food));
            if (ant.position == food) forgetFood(ant, food);
            return;
        }
        const Coord waypoint = nextWaypoint(world, ant);
        if (waypoint.first >= 0) {
            moveSegment(world, ant, shortestPath(world.terrainMap, ant.position, waypoint));
        } else {
            state.phase[ant.id] = 3;
        }
    }

} // namespace


/** @brief Runs the selected sector-foraging strategy. */
void AntWorld::forage() {
    if (ants.empty()) return;

    // ponytail: one active world plan; key plans by world lifetime if concurrent simulations are needed.
    if (!isCurrentPlan(*this)) makePlan(*this);


    forageV3(*this);
}
