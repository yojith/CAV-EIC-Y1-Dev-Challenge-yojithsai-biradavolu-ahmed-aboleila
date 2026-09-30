//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numbers>
#include <numeric>
#include <thread>
#include <tuple>
#include <vector>

// Set to 1 (or compile with -DUSE_ANGLE_RANKING=1) to rank narrow sectors first.
#ifndef USE_ANGLE_RANKING
#define USE_ANGLE_RANKING 0
#endif

// The game executable enables this. Tests override it to keep their output clean.
#ifndef ENABLE_VISUALIZER
#define ENABLE_VISUALIZER 1
#endif

#ifndef VISUALIZER_DELAY_MS
#define VISUALIZER_DELAY_MS 1000
#endif

namespace {
    struct Plan {
        const AntWorld *world{};
        Coord home{-1, -1};
        std::size_t rows{};
        std::size_t cols{};
        std::size_t antCount{};
        std::vector<std::vector<Coord>> queues;
        std::vector<std::size_t> nextWaypoint;
    };

    Plan plan;

    double angleFrom(Coord origin, Coord point) {
        double angle = std::atan2(point.first - origin.first, point.second - origin.second);
        return angle < 0 ? angle + 2.0 * std::numbers::pi : angle;
    }

    int pathCost(const MapTemplate &terrain, Coord from, Coord to) {
        return calculatePathCost(terrain, shortestPath(terrain, from, to));
    }

    bool isCurrentPlan(const AntWorld &world) {
        return plan.world == &world && plan.home == world.homeCoordinates &&
               plan.rows == world.terrainMap.size() && plan.cols == world.terrainMap.front().size() &&
               plan.antCount == world.ants.size();
    }

    void makePlan(const AntWorld &world) {
        const std::size_t antCount = world.ants.size();
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

        plan = {&world, world.homeCoordinates, world.terrainMap.size(), world.terrainMap.front().size(), antCount,
                std::vector<std::vector<Coord>>(antCount), std::vector<std::size_t>(antCount, 0)};
        for (std::size_t rank = 0; rank < antCount; ++rank) {
            plan.queues[antRank[rank]] = std::move(sectors[sectorRank[rank]]);
        }
    }

    bool canReachAndReturn(const AntWorld &world, const Ant &ant, Coord destination) {
        return pathCost(world.terrainMap, ant.position, destination) +
               pathCost(world.terrainMap, destination, world.homeCoordinates) <= ant.energy;
    }

    Coord bestVisibleFood(AntWorld &world, Ant &ant) {
        Coord best{-1, -1};
        int bestCost = 0;
        for (Coord food: ant.foodScan(world.foodMap)) {
            const int cost = pathCost(world.terrainMap, ant.position, food) +
                             pathCost(world.terrainMap, food, world.homeCoordinates);
            if (cost <= ant.energy && (best.first == -1 || cost < bestCost)) {
                best = food;
                bestCost = cost;
            }
        }
        return best;
    }

#if ENABLE_VISUALIZER
    void renderWorld(const AntWorld &world) {
        static std::size_t frame = 0;
        constexpr const char *terrainColors[] = {
            "\x1b[48;2;46;125;50m",  // height 0: green
            "\x1b[48;2;107;107;44m", // height 1: greenish-brown
            "\x1b[48;2;121;85;61m"   // height 2: brown
        };

        std::cout << "\x1b[2J\x1b[HFrame " << ++frame << " | Score: " << world.score << '\n';
        for (std::size_t row = 0; row < world.terrainMap.size(); ++row) {
            for (std::size_t col = 0; col < world.terrainMap[row].size(); ++col) {
                const Coord cell{static_cast<int>(row), static_cast<int>(col)};
                char symbol = cell == world.homeCoordinates ? 'H' :
                              world.foodMap[row][col] == 1 ? 'F' : ' ';
                for (const Ant &ant: world.ants) {
                    if (ant.position == cell) symbol = ant.carryingFood ? '@' : 'A';
                }
                std::cout << terrainColors[world.terrainMap[row][col]] << symbol << "\x1b[0m";
            }
            std::cout << '\n';
        }
        for (std::size_t index = 0; index < world.ants.size(); ++index) {
            const Ant &ant = world.ants[index];
            std::cout << "A" << index << "=(" << ant.position.first << ',' << ant.position.second << ")"
                      << " energy=" << ant.energy << (ant.carryingFood ? " carrying" : "") << '\n';
        }
        std::cout << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(VISUALIZER_DELAY_MS));
    }
#endif
} // namespace

/** @brief Chooses visible food first, then sweeps an energy-ranked sector. */
void AntWorld::forage() {
#if ENABLE_VISUALIZER
    renderWorld(*this);
#endif
    if (ants.empty()) return;

    // ponytail: one active world plan; key plans by world lifetime if concurrent simulations are needed.
    if (!isCurrentPlan(*this)) makePlan(*this);

    for (std::size_t index = 0; index < ants.size(); ++index) {
        Ant &ant = ants[index];
        if (ant.carryingFood) {
            ant.returnHome(terrainMap, foodMap);
            continue;
        }

        const Coord food = bestVisibleFood(*this, ant);
        if (food.first != -1) {
            ant.move(terrainMap, food, foodMap);
            continue;
        }

        if (plan.nextWaypoint[index] < plan.queues[index].size()) {
            const Coord waypoint = plan.queues[index][plan.nextWaypoint[index]];
            if (canReachAndReturn(*this, ant, waypoint)) {
                ++plan.nextWaypoint[index];
                ant.move(terrainMap, waypoint, foodMap);
                continue;
            }
        }

        if (ant.position != homeCoordinates) ant.returnHome(terrainMap, foodMap);
    }
}
