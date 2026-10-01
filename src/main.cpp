#include <iostream>
#include <numeric>
#include "../include/antworld.h"

/** @brief The main function that will run the game. If you are not using a IDE gui, this is the executable you want to target when you build
 */
int main() {
    /**
     * default seed. you can change this to any number you want. for as long as the seed is the same, the same "random" world will
     * always be generated. You can use this to test reproducibly while developing.
     */
    const uint32_t SEED = 12345;

    // once you're confident and want to begin testing on random seeds, you can comment out the above line
    // uncomment the following ones.
    // std::random_device rd;
    // uint32_t SEED = rd();

    AntWorld gameInstance = AntWorld(SEED);

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

    gameInstance.renderWorld(false);

    if (gameOver) {
        printf("GAME OVER!! Total score: %d\n", gameInstance.score);
    } else if (idleSteps == 10) {
        printf("Game stopped after 10 turns without energy use. Steps: %d. Total score: %d\n",
               stepCount, gameInstance.score);
    } else {
        printf("Termination reached for unknown reason.");
    }
}
