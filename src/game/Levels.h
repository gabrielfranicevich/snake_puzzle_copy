#pragma once

// Returns the total number of levels available.
int getNumLevels();

// Populates w, h, grid, apples, and snake based on the specified level index.
struct GameState;
void loadLevelData(int idx, GameState& state);
