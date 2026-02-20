#pragma once
#include "../core/Core.h"

// Encapsulates all game logic and state history.
// Replaces global functions and state.
class GameEngine {
public:
    GameEngine();

    void loadLevel(int idx);
    void tick(float dt);
    
    // Processes a grid movement command.
    // dir should be {0,-1}, {0,1}, {-1,0}, or {1,0}.
    // Returns true if the move was valid and executed.
    bool doMove(V2 dir);
    void undo();

    int getCurrentLevel() const { return m_levelIdx; }
    void nextLevel();
    void prevLevel();
    void restartLevel();

    // Read-only access for the renderer
    const GameState& getState() const { return m_state; }
    
    // Check global best stars
    int getBestStars(int levelIdx) const;

private:
    void applyGravity();
    void saveState();

    GameState m_state;
    int m_levelIdx;
    int m_bestStars[64];
};
