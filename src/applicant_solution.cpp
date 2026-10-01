//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <queue>
#include <tuple>
#include <vector>

// Set to 1 (or compile with -DUSE_ANGLE_RANKING=1) to rank narrow sectors first.
#ifndef USE_ANGLE_RANKING
#define USE_ANGLE_RANKING 0
#endif

// Set to 0 to visit every cell in the ordered sector queue. Set to 1 to skip cells covered by this ant's 7x7 scan.
#ifndef USE_SMART_SCANNING
#define USE_SMART_SCANNING 1
#endif

// Set to 1 (or compile with -DUSE_V2_STRATEGY=1) for the single-ant handoff strategy.
#ifndef USE_V2_STRATEGY
#define USE_V2_STRATEGY 0
#endif

// V3 shares V2's sequential sweep and changes the food handoff.
#ifndef USE_V3_STRATEGY
#define USE_V3_STRATEGY 0
#endif

// V3 only: 0 passes food into the next sector; 1 follows home and leaves a pheromone.
#ifndef V3_HOME_PHEROMONE
#define V3_HOME_PHEROMONE 0
#endif

#if USE_V2_STRATEGY && USE_V3_STRATEGY
#error Select either V2 or V3, not both.
#endif

#ifndef WAIT_FOR_ENTER
#define WAIT_FOR_ENTER 1
#endif

// Terminal visualization is on by default.
#ifndef ENABLE_VISUALIZER
#define ENABLE_VISUALIZER 1
#endif

namespace {
    struct Plan {
        const AntWorld *world{};
        Coord home{-1, -1};
        std::size_t rows{};
        std::size_t cols{};
        std::vector<std::vector<Coord>> queues;
        std::vector<std::size_t> nextWaypoint;
        std::vector<MapTemplate> scanned;
        std::vector<std::vector<Coord>> rememberedFood;
        std::vector<std::vector<Coord>> trails;
        std::vector<int> sectorForAnt;
        MapTemplate sectorForCell;
        bool renderedInitial{};
    };

    Plan plan;

#if USE_V2_STRATEGY || USE_V3_STRATEGY
    struct V2State {
        std::vector<int> order;
        std::size_t active{};
        std::vector<int> phase; // 0: collect, 1: explore next sector, 2: carry toward home, 3: finished
        std::vector<int> exploreFloor;
        std::vector<Coord> explorationTarget;
        std::vector<std::vector<Coord>> visitedPheromone;
    } v2;
#endif

    double angleFrom(Coord origin, Coord point) {
        double angle = std::atan2(point.first - origin.first, point.second - origin.second);
        return angle < 0 ? angle + 2.0 * std::numbers::pi : angle;
    }

    int pathCost(const MapTemplate &terrain, Coord from, Coord to) {
        return calculatePathCost(terrain, shortestPath(terrain, from, to));
    }

    bool isCurrentPlan(const AntWorld &world) {
        return plan.world == &world && plan.home == world.homeCoordinates &&
               plan.rows == world.terrainMap.size() && plan.cols == world.terrainMap.front().size();
    }

    void makePlan(const AntWorld &world) {
        const std::size_t antCount = world.ants.size();
        const std::size_t slots = static_cast<std::size_t>(std::max_element(
            world.ants.begin(), world.ants.end(), [](const Ant &a, const Ant &b) { return a.id < b.id; })->id + 1);
        std::vector<Coord> cells;
        for (int row = 0; row < static_cast<int>(world.terrainMap.size()); ++row) {
            for (int col = 0; col < static_cast<int>(world.terrainMap.front().size()); ++col) {
                if (Coord{row, col} != world.homeCoordinates) cells.emplace_back(row, col);
            }
        }

        std::sort(cells.begin(), cells.end(), [&](Coord a, Coord b) {
            return std::tuple{angleFrom(world.homeCoordinates, a), a.first, a.second} <
                   std::tuple{angleFrom(world.homeCoordinates, b), b.first, b.second};
        });

        std::vector<std::vector<Coord>> sectors(antCount);
        MapTemplate sectorForCell(world.terrainMap.size(), std::vector<int>(world.terrainMap.front().size(), -1));
        std::vector<double> sectorWidth(antCount, 0.0);
        std::vector<int> sectorRoundTrip(antCount, 0);
        const std::size_t base = cells.size() / antCount;
        const std::size_t remainder = cells.size() % antCount;
        std::size_t first = 0;
        for (std::size_t sector = 0; sector < antCount; ++sector) {
            const std::size_t count = base + (sector < remainder ? 1 : 0);
            sectors[sector] = std::vector<Coord>(cells.begin() + static_cast<std::ptrdiff_t>(first),
                                                  cells.begin() + static_cast<std::ptrdiff_t>(first + count));
            first += count;
            for (Coord cell: sectors[sector]) sectorForCell[cell.first][cell.second] = static_cast<int>(sector);
            if (!sectors[sector].empty()) {
                sectorWidth[sector] = angleFrom(world.homeCoordinates, sectors[sector].back()) -
                                      angleFrom(world.homeCoordinates, sectors[sector].front());
                for (Coord waypoint: sectors[sector]) {
                    sectorRoundTrip[sector] = std::max(
                        sectorRoundTrip[sector], 2 * pathCost(world.terrainMap, world.homeCoordinates, waypoint));
                }
            }
            std::sort(sectors[sector].begin(), sectors[sector].end(), [&](Coord a, Coord b) {
                return std::tuple{pathCost(world.terrainMap, world.homeCoordinates, a),
                                  angleFrom(world.homeCoordinates, a), a.first, a.second} <
                       std::tuple{pathCost(world.terrainMap, world.homeCoordinates, b),
                                  angleFrom(world.homeCoordinates, b), b.first, b.second};
            });
        }

        std::vector<int> sectorRank(antCount);
        std::iota(sectorRank.begin(), sectorRank.end(), 0);
        std::sort(sectorRank.begin(), sectorRank.end(), [&](int a, int b) {
#if USE_ANGLE_RANKING
            return std::tuple{sectorWidth[a], a} < std::tuple{sectorWidth[b], b};
#else
            return std::tuple{sectorRoundTrip[a], a} > std::tuple{sectorRoundTrip[b], b};
#endif
        });

        std::vector<int> antRank(antCount);
        std::iota(antRank.begin(), antRank.end(), 0);
        std::sort(antRank.begin(), antRank.end(), [&](int a, int b) {
            return std::tuple{world.ants[a].energy, a} > std::tuple{world.ants[b].energy, b};
        });

        plan = {&world, world.homeCoordinates, world.terrainMap.size(), world.terrainMap.front().size(),
                std::vector<std::vector<Coord>>(slots),
                std::vector<std::size_t>(slots, 0),
                std::vector<MapTemplate>(slots, MapTemplate(world.terrainMap.size(),
                                                             std::vector<int>(world.terrainMap.front().size(), 0))),
                std::vector<std::vector<Coord>>(slots), std::vector<std::vector<Coord>>(slots),
                std::vector<int>(slots, -1), std::move(sectorForCell), false};
#if USE_V2_STRATEGY || USE_V3_STRATEGY
        v2 = {};
        v2.phase.assign(slots, 0);
        v2.exploreFloor.assign(slots, 0);
        v2.explorationTarget.assign(slots, {-1, -1});
        v2.visitedPheromone.assign(slots, {});
#endif
        for (std::size_t rank = 0; rank < antCount; ++rank) {
            const std::size_t antId = static_cast<std::size_t>(world.ants[antRank[rank]].id);
#if USE_V2_STRATEGY || USE_V3_STRATEGY
            const int sector = (sectorRank.front() + static_cast<int>(rank)) % static_cast<int>(antCount);
            v2.order.push_back(static_cast<int>(antId));
#else
            const int sector = sectorRank[rank];
#endif
            plan.queues[antId] = std::move(sectors[sector]);
            plan.sectorForAnt[antId] = sector;
        }
    }

    bool canReachAndReturn(const AntWorld &world, const Ant &ant, Coord destination) {
        return pathCost(world.terrainMap, ant.position, destination) +
               pathCost(world.terrainMap, destination, world.homeCoordinates) <= ant.energy;
    }

    void rememberVisibleFood(AntWorld &world, Ant &ant) {
        std::vector<Coord> visible = ant.foodScan(world.foodMap);
        std::vector<Coord> &memory = plan.rememberedFood[ant.id];
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

    void recordTrail(const AntWorld &world, const Ant &ant, Coord from) {
        for (Coord cell: shortestPath(world.terrainMap, from, ant.position)) {
            std::vector<Coord> &trail = plan.trails[ant.id];
            if (trail.empty() || trail.back() != cell) trail.push_back(cell);
        }
    }

#if USE_V2_STRATEGY || USE_V3_STRATEGY
    std::vector<Coord> sectorPath(const AntWorld &world, Coord from, Coord to, int sector, int adjacent = -1) {
        const int rows = static_cast<int>(world.terrainMap.size());
        const int cols = static_cast<int>(world.terrainMap.front().size());
        const int infinity = std::numeric_limits<int>::max();
        MapTemplate distance(rows, std::vector<int>(cols, infinity));
        std::vector<std::vector<Coord>> parent(rows, std::vector<Coord>(cols, {-1, -1}));
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pending;
        distance[from.first][from.second] = 0;
        pending.push({0, from});

        while (!pending.empty()) {
            const Node current = pending.top();
            pending.pop();
            if (current.cost != distance[current.pos.first][current.pos.second]) continue;
            if (current.pos == to) break;
            for (Coord delta: {Coord{-1, 0}, Coord{1, 0}, Coord{0, -1}, Coord{0, 1}}) {
                const Coord neighbor{current.pos.first + delta.first, current.pos.second + delta.second};
                if (neighbor.first < 0 || neighbor.first >= rows || neighbor.second < 0 || neighbor.second >= cols) continue;
                const int owner = plan.sectorForCell[neighbor.first][neighbor.second];
                if (neighbor != world.homeCoordinates && owner != sector && owner != adjacent) continue;
                const int cost = current.cost + 1 + std::abs(world.terrainMap[current.pos.first][current.pos.second] -
                                                              world.terrainMap[neighbor.first][neighbor.second]);
                if (cost >= distance[neighbor.first][neighbor.second]) continue;
                distance[neighbor.first][neighbor.second] = cost;
                parent[neighbor.first][neighbor.second] = current.pos;
                pending.push({cost, neighbor});
            }
        }
        if (distance[to.first][to.second] == infinity) return {};
        std::vector<Coord> path;
        for (Coord cell = to; cell != from; cell = parent[cell.first][cell.second]) path.push_back(cell);
        path.push_back(from);
        std::reverse(path.begin(), path.end());
        return path;
    }

    void moveV2Segment(AntWorld &world, Ant &ant, const std::vector<Coord> &path, int energyFloor = 0) {
        const Coord from = ant.position;
        const bool wasCarrying = ant.carryingFood;
        for (std::size_t step = 1; step < path.size() && step <= 3; ++step) {
            const Coord next = path[step];
            const int cost = 1 + std::abs(world.terrainMap[ant.position.first][ant.position.second] -
                                           world.terrainMap[next.first][next.second]);
            if (ant.energy - cost < energyFloor) break;
            ant.move(world.terrainMap, next, world.foodMap);
            if (!wasCarrying && ant.carryingFood) break;
        }
        recordTrail(world, ant, from);
    }

    Coord v2FoodTarget(const AntWorld &world, const Ant &ant) {
        Coord best{-1, -1};
        int bestCost = std::numeric_limits<int>::max();
        for (Coord food: plan.rememberedFood[ant.id]) {
            const std::vector<Coord> route = sectorPath(world, ant.position, food, plan.sectorForAnt[ant.id]);
            if (route.empty()) continue;
            const int cost = calculatePathCost(world.terrainMap, route);
            if (cost <= ant.energy && cost < bestCost) {
                best = food;
                bestCost = cost;
            }
        }
        return best;
    }

    Coord v2NextWaypoint(const AntWorld &world, const Ant &ant) {
        std::size_t &cursor = plan.nextWaypoint[ant.id];
        const std::vector<Coord> &queue = plan.queues[ant.id];
        while (cursor < queue.size()) {
            const Coord cell = queue[cursor];
            if (plan.scanned[ant.id][cell.first][cell.second]) {
                ++cursor;
                continue;
            }
            const std::vector<Coord> route = sectorPath(world, ant.position, cell, plan.sectorForAnt[ant.id]);
            if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) return cell;
            ++cursor;
        }
        return {-1, -1};
    }

    Coord chooseHandoffSite(AntWorld &world, Ant &ant, int nextSector) {
        const std::vector<Coord> visible = ant.foodScan(world.foodMap);
        Coord best{-1, -1};
        int bestFood = -1;
        int bestCost = std::numeric_limits<int>::max();
        const int budget = ant.energy - v2.exploreFloor[ant.id];
        for (int row = std::max(0, ant.position.first - ant.foodRadius);
             row <= std::min(static_cast<int>(world.terrainMap.size()) - 1, ant.position.first + ant.foodRadius); ++row) {
            for (int col = std::max(0, ant.position.second - ant.foodRadius);
                 col <= std::min(static_cast<int>(world.terrainMap.front().size()) - 1, ant.position.second + ant.foodRadius); ++col) {
                if (plan.sectorForCell[row][col] != nextSector) continue;
                const Coord cell{row, col};
                const std::vector<Coord> route = sectorPath(world, ant.position, cell,
                                                             plan.sectorForAnt[ant.id], nextSector);
                if (route.empty()) continue;
                const int cost = calculatePathCost(world.terrainMap, route);
                if (cost > budget) continue;
                const int nearbyFood = static_cast<int>(std::count_if(visible.begin(), visible.end(), [&](Coord food) {
                    return plan.sectorForCell[food.first][food.second] == nextSector &&
                           std::abs(food.first - row) <= ant.foodRadius &&
                           std::abs(food.second - col) <= ant.foodRadius;
                }));
                if (nearbyFood > bestFood || (nearbyFood == bestFood && cost < bestCost)) {
                    best = cell;
                    bestFood = nearbyFood;
                    bestCost = cost;
                }
            }
        }
        if (best.first >= 0) return best;
        // The next border can be outside the current scan; its geometry is part of the planned route.
        for (int row = 0; row < static_cast<int>(world.terrainMap.size()); ++row) {
            for (int col = 0; col < static_cast<int>(world.terrainMap.front().size()); ++col) {
                if (plan.sectorForCell[row][col] != nextSector) continue;
                const Coord cell{row, col};
                const auto route = sectorPath(world, ant.position, cell, plan.sectorForAnt[ant.id], nextSector);
                if (route.empty()) continue;
                const int cost = calculatePathCost(world.terrainMap, route);
                if (cost <= ant.energy && cost < bestCost) {
                    best = cell;
                    bestCost = cost;
                    v2.exploreFloor[ant.id] = 0;
                }
            }
        }
        return best;
    }

    Coord closestHandoffCell(const AntWorld &world, const Ant &ant, int nextSector) {
        Coord best{-1, -1};
        int bestHomeCost = std::numeric_limits<int>::max();
        int bestTravelCost = std::numeric_limits<int>::max();
        for (int row = 0; row < static_cast<int>(world.terrainMap.size()); ++row) {
            for (int col = 0; col < static_cast<int>(world.terrainMap.front().size()); ++col) {
                if (plan.sectorForCell[row][col] != nextSector) continue;
                const Coord cell{row, col};
                const std::vector<Coord> route = sectorPath(world, ant.position, cell, nextSector);
                if (route.empty()) continue;
                const int travelCost = calculatePathCost(world.terrainMap, route);
                if (travelCost > ant.energy) continue;
                const int homeCost = pathCost(world.terrainMap, cell, world.homeCoordinates);
                if (std::tuple{homeCost, travelCost} < std::tuple{bestHomeCost, bestTravelCost}) {
                    best = cell;
                    bestHomeCost = homeCost;
                    bestTravelCost = travelCost;
                }
            }
        }
        return best;
    }

    void spendInSector(AntWorld &world, Ant &ant, int sector) {
        for (int step = 0; step < 3; ++step) {
            Coord neighbor{-1, -1};
            int closest = std::numeric_limits<int>::max();
            for (Coord delta: {Coord{-1, 0}, Coord{1, 0}, Coord{0, -1}, Coord{0, 1}}) {
                const Coord cell{ant.position.first + delta.first, ant.position.second + delta.second};
                if (cell.first < 0 || cell.first >= static_cast<int>(world.terrainMap.size()) ||
                    cell.second < 0 || cell.second >= static_cast<int>(world.terrainMap.front().size()) ||
                    plan.sectorForCell[cell.first][cell.second] != sector) continue;
                const int cost = 1 + std::abs(world.terrainMap[ant.position.first][ant.position.second] -
                                              world.terrainMap[cell.first][cell.second]);
                if (cost > ant.energy) continue;
                const int homeCost = pathCost(world.terrainMap, cell, world.homeCoordinates);
                if (homeCost < closest) {
                    neighbor = cell;
                    closest = homeCost;
                }
            }
            if (neighbor.first < 0) {
                // ponytail: the engine drops carried food only at zero energy; no affordable move can leave it stranded.
                v2.phase[ant.id] = 3;
                return;
            }
            const Coord from = ant.position;
            ant.move(world.terrainMap, neighbor, world.foodMap);
            recordTrail(world, ant, from);
            if (ant.energy == 0) return;
        }
    }

    void forageV2(AntWorld &world) {
        while (v2.active < v2.order.size()) {
            const int id = v2.order[v2.active];
            const auto it = std::find_if(world.ants.begin(), world.ants.end(),
                                         [&](const Ant &candidate) { return candidate.id == id; });
            if (it != world.ants.end() && v2.phase[id] != 3 && it->energy > 0) break;
            ++v2.active;
        }
        if (v2.active == v2.order.size()) return;
        Ant &ant = *std::find_if(world.ants.begin(), world.ants.end(), [&](const Ant &candidate) {
            return candidate.id == v2.order[v2.active];
        });
        markLocalScan(world, ant);
        rememberVisibleFood(world, ant);
        const int ownSector = plan.sectorForAnt[ant.id];
        const int nextSector = (ownSector + 1) % static_cast<int>(v2.order.size());

        if (v2.phase[ant.id] == 0 && ant.carryingFood &&
            pathCost(world.terrainMap, ant.position, world.homeCoordinates) > ant.energy) {
            v2.phase[ant.id] = 1;
            v2.exploreFloor[ant.id] = ant.energy / 2;
            v2.explorationTarget[ant.id] = chooseHandoffSite(world, ant, nextSector);
        }

        if (v2.phase[ant.id] == 1) {
            const Coord target = v2.explorationTarget[ant.id];
            if (target.first >= 0 && ant.position != target) {
                const auto route = sectorPath(world, ant.position, target, ownSector, nextSector);
                moveV2Segment(world, ant, route, v2.exploreFloor[ant.id]);
            }
            if (plan.sectorForCell[ant.position.first][ant.position.second] == nextSector &&
                (ant.position == target || ant.energy <= v2.exploreFloor[ant.id])) {
                ant.dropPheromone(world.pheromoneMap);
                v2.phase[ant.id] = 2;
            } else if (target.first < 0 || ant.energy <= v2.exploreFloor[ant.id]) {
                v2.phase[ant.id] = 3;
            }
            return;
        }

        if (v2.phase[ant.id] == 2) {
            const Coord target = closestHandoffCell(world, ant, nextSector);
            if (target.first >= 0 && ant.position != target) {
                moveV2Segment(world, ant, sectorPath(world, ant.position, target, nextSector));
                return;
            }
            spendInSector(world, ant, nextSector);
            return;
        }

        if (ant.carryingFood) {
            const Coord from = ant.position;
            ant.returnHome(world.terrainMap, world.foodMap);
            recordTrail(world, ant, from);
            return;
        }

        const Coord food = v2FoodTarget(world, ant);
        if (food.first >= 0) {
            moveV2Segment(world, ant, sectorPath(world, ant.position, food, ownSector));
            if (ant.position == food) forgetFood(ant, food);
            return;
        }

        for (Coord marker: ant.pheromoneScan(world.pheromoneMap)) {
            if (plan.sectorForCell[marker.first][marker.second] != ownSector ||
                std::find(v2.visitedPheromone[ant.id].begin(), v2.visitedPheromone[ant.id].end(), marker) !=
                    v2.visitedPheromone[ant.id].end()) continue;
            const auto route = sectorPath(world, ant.position, marker, ownSector);
            if (route.empty() || calculatePathCost(world.terrainMap, route) > ant.energy) continue;
            moveV2Segment(world, ant, route);
            if (ant.position == marker) v2.visitedPheromone[ant.id].push_back(marker);
            return;
        }

        const Coord waypoint = v2NextWaypoint(world, ant);
        if (waypoint.first >= 0) {
            moveV2Segment(world, ant, sectorPath(world, ant.position, waypoint, ownSector));
        } else {
            v2.phase[ant.id] = 3;
        }
    }

#if USE_V3_STRATEGY
    Coord nearestNextSectorEntry(const AntWorld &world, const Ant &ant, int nextSector) {
        Coord best{-1, -1};
        int bestCost = std::numeric_limits<int>::max();
        for (int row = 0; row < static_cast<int>(world.terrainMap.size()); ++row) {
            for (int col = 0; col < static_cast<int>(world.terrainMap.front().size()); ++col) {
                if (plan.sectorForCell[row][col] != nextSector) continue;
                const Coord cell{row, col};
                const auto route = sectorPath(world, ant.position, cell, plan.sectorForAnt[ant.id], nextSector);
                if (route.empty()) continue;
                const int cost = calculatePathCost(world.terrainMap, route);
                if (cost <= ant.energy && cost < bestCost) {
                    best = cell;
                    bestCost = cost;
                }
            }
        }
        return best;
    }

    void forageV3(AntWorld &world) {
        while (v2.active < v2.order.size()) {
            const int id = v2.order[v2.active];
            const auto it = std::find_if(world.ants.begin(), world.ants.end(),
                                         [&](const Ant &candidate) { return candidate.id == id; });
            if (it != world.ants.end() && v2.phase[id] != 3 && it->energy > 0) break;
            ++v2.active;
        }
        if (v2.active == v2.order.size()) return;
        Ant &ant = *std::find_if(world.ants.begin(), world.ants.end(), [&](const Ant &candidate) {
            return candidate.id == v2.order[v2.active];
        });
        markLocalScan(world, ant);
        rememberVisibleFood(world, ant);
        const int ownSector = plan.sectorForAnt[ant.id];
        const int nextSector = (ownSector + 1) % static_cast<int>(v2.order.size());

        if (ant.carryingFood && v2.phase[ant.id] == 5) v2.phase[ant.id] = 0;
        if (v2.phase[ant.id] == 0 && ant.carryingFood &&
            pathCost(world.terrainMap, ant.position, world.homeCoordinates) > ant.energy) {
#if V3_HOME_PHEROMONE
            v2.phase[ant.id] = 4;
#else
            v2.phase[ant.id] = 2;
            v2.explorationTarget[ant.id] = nearestNextSectorEntry(world, ant, nextSector);
#endif
        }

#if V3_HOME_PHEROMONE
        if (v2.phase[ant.id] == 4) {
            const Coord from = ant.position;
            ant.returnHome(world.terrainMap, world.foodMap);
            recordTrail(world, ant, from);
            if (ant.position == world.homeCoordinates) {
                v2.phase[ant.id] = 0;
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
                    const Coord previous = ant.position;
                    ant.move(world.terrainMap, neighbor, world.foodMap);
                    recordTrail(world, ant, previous);
                } else {
                    v2.phase[ant.id] = 3;
                }
            }
            ant.dropPheromone(world.pheromoneMap);
            return;
        }

        if (v2.phase[ant.id] == 5) {
            const Coord marker = v2.explorationTarget[ant.id];
            if (ant.position != marker) {
                const auto route = shortestPath(world.terrainMap, ant.position, marker);
                if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) {
                    moveV2Segment(world, ant, route);
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
                    moveV2Segment(world, ant, shortestPath(world.terrainMap, ant.position, food));
                    return;
                }
            }
            v2.visitedPheromone[ant.id].push_back(marker);
            v2.phase[ant.id] = 0;
        }
#else
        if (v2.phase[ant.id] == 2) {
            if (plan.sectorForCell[ant.position.first][ant.position.second] != nextSector) {
                const Coord entry = v2.explorationTarget[ant.id];
                if (entry.first < 0) {
                    v2.phase[ant.id] = 3;
                    return;
                }
                moveV2Segment(world, ant, sectorPath(world, ant.position, entry, ownSector, nextSector));
                return;
            }
            const Coord target = closestHandoffCell(world, ant, nextSector);
            if (target.first >= 0 && ant.position != target) {
                moveV2Segment(world, ant, sectorPath(world, ant.position, target, nextSector));
            } else {
                spendInSector(world, ant, nextSector);
            }
            return;
        }
#endif

        if (ant.carryingFood) {
            const Coord from = ant.position;
            ant.returnHome(world.terrainMap, world.foodMap);
            recordTrail(world, ant, from);
            return;
        }

#if V3_HOME_PHEROMONE
        const int previousSector = (ownSector + static_cast<int>(v2.order.size()) - 1) %
                                   static_cast<int>(v2.order.size());
        for (Coord marker: ant.pheromoneScan(world.pheromoneMap)) {
            if (plan.sectorForCell[marker.first][marker.second] != previousSector ||
                std::find(v2.visitedPheromone[ant.id].begin(), v2.visitedPheromone[ant.id].end(), marker) !=
                    v2.visitedPheromone[ant.id].end()) continue;
            v2.explorationTarget[ant.id] = marker;
            v2.phase[ant.id] = 5;
            const auto route = shortestPath(world.terrainMap, ant.position, marker);
            if (!route.empty() && calculatePathCost(world.terrainMap, route) <= ant.energy) {
                moveV2Segment(world, ant, route);
                return;
            }
            v2.visitedPheromone[ant.id].push_back(marker);
            v2.phase[ant.id] = 0;
        }
#endif

        const Coord food = v2FoodTarget(world, ant);
        if (food.first >= 0) {
            moveV2Segment(world, ant, sectorPath(world, ant.position, food, ownSector));
            if (ant.position == food) forgetFood(ant, food);
            return;
        }
        const Coord waypoint = v2NextWaypoint(world, ant);
        if (waypoint.first >= 0) {
            moveV2Segment(world, ant, sectorPath(world, ant.position, waypoint, ownSector));
        } else {
            v2.phase[ant.id] = 3;
        }
    }
#endif
#endif

#if ENABLE_VISUALIZER
    void renderWorldFrame(const AntWorld &world, bool waitForEnter) {
        static std::size_t frame = 0;
        constexpr const char *green = "\x1b[48;2;102;160;80m";
        constexpr const char *darkGreen = "\x1b[48;2;45;100;42m";
        constexpr const char *brown = "\x1b[48;2;170;120;75m";
        constexpr const char *darkBrown = "\x1b[48;2;100;65;40m";
        constexpr const char *antColors[] = {"\x1b[38;5;226m", "\x1b[38;5;51m", "\x1b[38;5;201m",
                                             "\x1b[38;5;231m", "\x1b[38;5;214m", "\x1b[38;5;159m"};

        std::cout << "\x1b[2J\x1b[HFrame " << ++frame << " | Score: " << world.score << '\n';
        for (std::size_t row = 0; row < world.terrainMap.size(); ++row) {
            for (std::size_t col = 0; col < world.terrainMap[row].size(); ++col) {
                const Coord cell{static_cast<int>(row), static_cast<int>(col)};
                const int sector = plan.sectorForCell.empty() ? 0 : plan.sectorForCell[row][col];
                const bool explored = std::any_of(plan.scanned.begin(), plan.scanned.end(), [&](const MapTemplate &grid) {
                    return grid[row][col] != 0;
                });
                const bool greenSector = sector < 0 || sector % 2 == 0;
                const char *background = greenSector ? (explored ? darkGreen : green) : (explored ? darkBrown : brown);
                char symbol = cell == world.homeCoordinates ? 'H' :
                              world.foodMap[row][col] == 1 ? 'F' :
                              world.pheromoneMap[row][col] == 1 ? 'P' : ' ';
                int owner = -1;
                for (std::size_t id = 0; id < plan.trails.size(); ++id) {
                    if (std::find(plan.trails[id].begin(), plan.trails[id].end(), cell) != plan.trails[id].end()) {
                        owner = static_cast<int>(id);
                    }
                }
                if (symbol == ' ' && owner >= 0) symbol = static_cast<char>('a' + owner % 26);
                for (const Ant &ant: world.ants) {
                    if (ant.position == cell) {
                        symbol = ant.carryingFood ? '@' : static_cast<char>('A' + ant.id % 26);
                        owner = ant.id;
                    }
                }
                std::cout << background << (owner >= 0 ? antColors[owner % 6] : "\x1b[97m") << symbol << "\x1b[0m";
            }
            std::cout << '\n';
        }
        for (std::size_t index = 0; index < world.ants.size(); ++index) {
            const Ant &ant = world.ants[index];
            std::cout << "A" << ant.id << "=(" << ant.position.first << ',' << ant.position.second << ")"
                      << " energy=" << ant.energy << (ant.carryingFood ? " carrying" : "") << '\n';
        }
#if USE_V2_STRATEGY || USE_V3_STRATEGY
        if (v2.active < v2.order.size()) {
            std::cout << "Active: A" << v2.order[v2.active]
                      << " phase=" << v2.phase[v2.order[v2.active]] << '\n';
        }
#endif
        std::cout << std::flush;
        if (waitForEnter) {
            std::cout << "Press Enter to advance..." << std::flush;
            std::cin.get();
        }
    }
#endif
} // namespace

void AntWorld::renderWorld(bool waitForEnter) {
#if ENABLE_VISUALIZER
    renderWorldFrame(*this, waitForEnter);
#endif
}

/** @brief Runs the selected sector-foraging strategy. */
void AntWorld::forage() {
    if (ants.empty()) return;

    // ponytail: one active world plan; key plans by world lifetime if concurrent simulations are needed.
    if (!isCurrentPlan(*this)) makePlan(*this);

#if ENABLE_VISUALIZER
    if (WAIT_FOR_ENTER || !plan.renderedInitial) {
        renderWorldFrame(*this, WAIT_FOR_ENTER);
        plan.renderedInitial = true;
    }
#endif

#if USE_V3_STRATEGY
    forageV3(*this);
#elif USE_V2_STRATEGY
    forageV2(*this);
#else
    for (Ant &ant: ants) {
        const Coord previous = ant.position;
        markLocalScan(*this, ant);
        rememberVisibleFood(*this, ant);
        if (ant.carryingFood) {
            ant.returnHome(terrainMap, foodMap);
            recordTrail(*this, ant, previous);
            continue;
        }

        const Coord food = bestRememberedFood(*this, ant);
        if (food.first != -1) {
            ant.move(terrainMap, food, foodMap);
            forgetFood(ant, food);
            recordTrail(*this, ant, previous);
            continue;
        }

        const Coord waypoint = nextQueuedWaypoint(*this, ant);
        if (waypoint.first != -1) {
            ant.move(terrainMap, waypoint, foodMap);
#if !USE_SMART_SCANNING
            ++plan.nextWaypoint[ant.id];
#endif
            recordTrail(*this, ant, previous);
            continue;
        }

        if (ant.position != homeCoordinates) {
            ant.returnHome(terrainMap, foodMap);
            recordTrail(*this, ant, previous);
        }
    }
#endif
}
