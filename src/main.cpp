#include <algorithm>
#include <iostream>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>
#include "../include/antworld.h"

/** @brief The main function that will run the game. If you are not using a IDE gui, this is the executable you want to target when you build
 */
// ---------- Changed from master: accept runtime world parameters ----------
int main(int argc, char *argv[]) {
    /**
     * default seed. you can change this to any number you want. for as long as the seed is the same, the same "random" world will
     * always be generated. You can use this to test reproducibly while developing.
     */
    // ---------- Changed from master: configurable defaults and argument parsing ----------
    uint32_t seed = 12345;
    int rows = 15;
    int cols = 15;
    int antCount = 8;
    double foodDensity = 0.4;

    if (argc != 1 && argc != 6) {
        std::cerr << "Usage: dev_challenge [seed rows cols ants foodDensity]\n";
        return 2;
    }
    if (argc == 6) {
        try {
            const auto parsedSeed = std::stoull(argv[1]);
            if (parsedSeed > std::numeric_limits<uint32_t>::max()) throw std::out_of_range("seed");
            seed = static_cast<uint32_t>(parsedSeed);
            rows = std::stoi(argv[2]);
            cols = std::stoi(argv[3]);
            antCount = std::stoi(argv[4]);
            foodDensity = std::stod(argv[5]);
            if (rows < 1 || cols < 1 || antCount < 1 || !std::isfinite(foodDensity) ||
                foodDensity < 0.0 || foodDensity > 1.0) throw std::invalid_argument("case parameters");
        } catch (const std::exception &) {
            std::cerr << "Invalid case: seed must be uint32, dimensions and ants positive, density in [0,1].\n";
            return 2;
        }
    }
    // ---------- End configurable parameters ----------

    // once you're confident and want to begin testing on random seeds, you can comment out the above line
    // uncomment the following ones.
    // std::random_device rd;
    // uint32_t seed = rd();

    // ---------- Changed from master: construct the selected world ----------
    AntWorld gameInstance = AntWorld(seed, rows, cols, antCount, foodDensity);

    // ---------- Added on this branch: energy-only score bound ----------
    std::vector<long long> roundTripCosts;
    const MapTemplate homeCosts = shortestDistances(gameInstance.terrainMap, gameInstance.homeCoordinates);
    for (std::size_t row = 0; row < gameInstance.foodMap.size(); ++row) {
        for (std::size_t col = 0; col < gameInstance.foodMap[row].size(); ++col) {
            if (gameInstance.foodMap[row][col] != 1) continue;
            roundTripCosts.push_back(2LL * homeCosts[row][col]);
        }
    }
    std::sort(roundTripCosts.begin(), roundTripCosts.end());
    const long long initialEnergy = std::accumulate(gameInstance.ants.begin(), gameInstance.ants.end(), 0LL,
                                                     [](long long total, const Ant &ant) { return total + ant.energy; });
    long long pooledEnergy = initialEnergy;
    int energyUpperBound = 0;
    for (long long cost: roundTripCosts) {
        if (cost > pooledEnergy) break;
        pooledEnergy -= cost;
        ++energyUpperBound;
    }
    std::cout << "Energy-only upper bound: " << energyUpperBound << " of " << roundTripCosts.size()
              << " initial food\n" << std::flush;
    // ---------- End energy-only score bound ----------

    // ---------- Changed from master: stop after 10 idle turns instead of a fixed step cap ----------
    bool gameOver = false;
    int stepCount = 0;
    int idleSteps = 0;
    while (not gameOver && idleSteps < 10) {
        const int energyBefore = std::accumulate(gameInstance.ants.begin(), gameInstance.ants.end(), 0,
                                                  [](int total, const Ant &ant) { return total + ant.energy; });
        gameOver = gameInstance.worldStep();
        ++stepCount;
        const int energyAfter = std::accumulate(gameInstance.ants.begin(), gameInstance.ants.end(), 0,
                                                 [](int total, const Ant &ant) { return total + ant.energy; });
        idleSteps = energyAfter == energyBefore ? idleSteps + 1 : 0;
    }
    // ---------- End idle-stop loop ----------

    // ---------- Changed from master: report idle-stop reason and score ----------
    if (gameOver) {
        printf("GAME OVER!! Total score: %d\n", gameInstance.score);
    } else if (idleSteps == 10) {
        printf("Game stopped after 10 turns without energy use. Steps: %d. Total score: %d\n",
               stepCount, gameInstance.score);
    } else {
        printf("Termination reached for unknown reason.");
    }
    // ---------- Added on this branch: machine-readable evaluation result ----------
    const long long remainingEnergy = std::accumulate(gameInstance.ants.begin(), gameInstance.ants.end(), 0LL,
                                                       [](long long total, const Ant &ant) { return total + ant.energy; });
    std::cout << "\nRESULT {\"score\":" << gameInstance.score
              << ",\"upper_bound\":" << energyUpperBound
              << ",\"initial_food\":" << roundTripCosts.size()
              << ",\"initial_energy\":" << initialEnergy
              << ",\"remaining_energy\":" << remainingEnergy
              << ",\"steps\":" << stepCount
              << ",\"stop_reason\":\"" << (gameOver ? "game_over" : idleSteps == 10 ? "idle" : "unknown")
              << "\"}\n";
    // ---------- End evaluation result ----------
}
