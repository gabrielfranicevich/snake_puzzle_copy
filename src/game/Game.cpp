#include "Game.h"
#include "Levels.h"

GameEngine::GameEngine() : m_levelIdx(0) {
  for (int i = 0; i < 64; i++)
    m_bestStars[i] = 0;
}

void GameEngine::loadLevel(int idx) {
  if (idx < 0 || idx >= getNumLevels())
    return;
  m_levelIdx = idx;
  m_state = GameState();
  loadLevelData(idx, m_state);
  m_state.prevSnake = m_state.snake;
  m_state.moveTimer = 1.0f;
}

void GameEngine::nextLevel() {
  loadLevel(std::min(m_levelIdx + 1, getNumLevels() - 1));
}

void GameEngine::prevLevel() { loadLevel(std::max(m_levelIdx - 1, 0)); }

void GameEngine::restartLevel() { loadLevel(m_levelIdx); }

void GameEngine::saveState() {
  m_state.hist.push_back({m_state.snake, m_state.prevSnake, m_state.grid,
                          m_state.apples, m_state.moves});
}

void GameEngine::undo() {
  if (m_state.hist.empty())
    return;
  auto &s = m_state.hist.back();
  m_state.snake = s.snake;
  m_state.prevSnake = s.prevSnake;
  m_state.grid = s.grid;
  m_state.apples = s.apples;
  m_state.moves = s.moves;
  m_state.eatFlash = 0;
  m_state.moveTimer = 1.0f;
  m_state.won = false;
  m_state.dead = false;
  m_state.lastDir = {0, 0};
  m_state.hist.pop_back();
}

void GameEngine::tick(float dt) {
  m_state.moveTimer =
      std::min(1.0f, m_state.moveTimer + dt * 6.5f); // 0.15s per tile
  m_state.eatFlash = std::max(0.f, m_state.eatFlash - dt * 3.5f);
  m_state.fallShake = std::max(0.f, m_state.fallShake - dt * 5.0f);
  if (m_state.won)
    m_state.winTimer += dt;
  if (m_state.dead)
    m_state.deadTimer += dt;
}

int GameEngine::getBestStars(int levelIdx) const {
  if (levelIdx < 0 || levelIdx >= 64)
    return 0;
  return m_bestStars[levelIdx];
}

// Returns true if any snake segment occupies a Trap tile
static bool touchingTrap(const GameState &s) {
  for (const auto &seg : s.snake) {
    if (s.safeAt(seg.x, seg.y) == T::Trap)
      return true;
  }
  return false;
}

// Restore a tile after a box moves away: Trap if it was originally a trap, else
// Void
static void restoreTile(GameState &s, int x, int y) {
  if (x >= 0 && x < s.w && y >= 0 && y < s.h) {
    s.at(x, y) = s.trapMask[y * s.w + x] ? T::Trap : T::Void;
  }
}

void GameEngine::applyGravity() {
  // We use a hard limit so no infinite loops on bugs
  static constexpr int MAX_FALL = 256;

  for (int step = 0; step < MAX_FALL && !m_state.won && !m_state.dead; ++step) {
    bool changed = true;
    std::vector<bool> boxStable(m_state.w * m_state.h, false);
    bool snakeStable = false;

    // Iteratively discover stability
    while (changed) {
      changed = false;

      // Check snake stability
      if (!snakeStable) {
        for (auto &seg : m_state.snake) {
          T below = m_state.safeAt(seg.x, seg.y + 1);
          // Trap is intentionally NOT solid for snake so it falls into them!
          if (below == T::Floor || below == T::Apple || below == T::Portal ||
              (below == T::Box && boxStable[(seg.y + 1) * m_state.w + seg.x])) {
            snakeStable = true;
            changed = true;
            break;
          }
        }
      }

      // Check box stability
      for (int y = 0; y < m_state.h; ++y) {
        for (int x = 0; x < m_state.w; ++x) {
          if (m_state.at(x, y) == T::Box && !boxStable[y * m_state.w + x]) {
            T below = m_state.safeAt(x, y + 1);
            bool onStable = false;

            // Traps ARE solid for boxes (allows bridging gaps)
            if (below == T::Floor || below == T::Apple || below == T::Portal ||
                below == T::Trap) {
              onStable = true;
            } else if (below == T::Box && boxStable[(y + 1) * m_state.w + x]) {
              onStable = true;
            }

            // Check if it's resting on a stable snake
            if (!onStable && snakeStable) {
              for (auto &seg : m_state.snake) {
                if (seg.x == x && seg.y == y + 1) {
                  onStable = true;
                  break;
                }
              }
            }

            if (onStable) {
              boxStable[y * m_state.w + x] = true;
              changed = true;
            }
          }
        }
      }
    }

    bool anyBoxFalling = false;
    for (int y = 0; y < m_state.h; ++y) {
      for (int x = 0; x < m_state.w; ++x) {
        if (m_state.at(x, y) == T::Box && !boxStable[y * m_state.w + x]) {
          anyBoxFalling = true;
        }
      }
    }

    if (snakeStable && !anyBoxFalling)
      break; // everything is stable

    // Check if anything falls off the bottom of the map
    bool fallsOff = false;
    if (!snakeStable) {
      for (auto &seg : m_state.snake) {
        if (seg.y + 1 >= m_state.h) {
          fallsOff = true;
          break;
        }
      }
    }

    if (fallsOff) {
      m_state.dead = true;
      return;
    }

    // Apply fall
    // Boxes fall (bottom-up to avoid overwriting)
    if (anyBoxFalling) {
      for (int y = m_state.h - 1; y >= 0; --y) {
        for (int x = 0; x < m_state.w; ++x) {
          if (m_state.at(x, y) == T::Box && !boxStable[y * m_state.w + x]) {
            // Restore tile the box is leaving
            restoreTile(m_state, x, y);
            if (y + 1 < m_state.h) {
              m_state.at(x, y + 1) = T::Box;
            }
            m_state.fallShake = 1.0f;
          }
        }
      }
    }

    // Snake falls
    if (!snakeStable) {
      m_state.prevSnake = m_state.snake;
      m_state.moveTimer = 0.0f;
      for (auto &seg : m_state.snake)
        seg.y++;
      m_state.fallShake = 1.0f;
    }

    // Trap check ONLY for snake
    if (touchingTrap(m_state)) {
      m_state.dead = true;
      return;
    }
  }
}

bool GameEngine::doMove(V2 dir) {
  if (m_state.won || m_state.dead || m_state.snake.empty())
    return false;
  // Input throttling: ignore new moves until previous animation finishes
  if (m_state.moveTimer < 0.85f)
    return false;

  // Block reversing direction when snake has ≥2 segments
  if (m_state.snake.size() > 1) {
    V2 ld = m_state.lastDir;
    if (dir.x == -ld.x && dir.y == -ld.y && (ld.x != 0 || ld.y != 0))
      return false;
  }

  V2 head = m_state.snake.front();
  V2 nh = {head.x + dir.x, head.y + dir.y};
  // Infinite map: any position is valid — no bounds check on nh.
  // Only the tile at the destination matters.

  T t = m_state.safeAt(nh.x, nh.y);

  // Can't move into a solid floor block
  if (t == T::Floor)
    return false;

  if (t == T::Box) {
    // Push box: destination must be inside the defined map and Void or Trap
    V2 bh = {nh.x + dir.x, nh.y + dir.y};
    T tb = m_state.safeAt(bh.x, bh.y);
    // Allow pushing box onto Trap tiles (box covers the trap)
    if (tb != T::Void && tb != T::Trap)
      return false;
    if (bh.x < 0 || bh.x >= m_state.w || bh.y < 0 || bh.y >= m_state.h)
      return false;

    saveState();
    // Restore the tile the box is leaving (Trap if it was originally a trap)
    restoreTile(m_state, nh.x, nh.y);
    m_state.at(bh.x, bh.y) = T::Box;
  } else {
    // Self-collision (exclude tail, which will move away)
    for (int i = 0; i < (int)m_state.snake.size() - 1; i++) {
      if (m_state.snake[i] == nh)
        return false;
    }
    saveState();
  }

  // Commit move
  m_state.prevSnake = m_state.snake;
  m_state.moveTimer = 0.0f;
  m_state.lastDir = dir;
  m_state.snake.push_front(nh);
  m_state.moves++;

  T cur = m_state.safeAt(nh.x, nh.y);

  if (cur == T::Apple) {
    // Growth: head moves to apple, old head becomes mid, tail stays → +1 length
    m_state.at(nh.x, nh.y) = T::Void;
    m_state.apples--;
    m_state.eatFlash = 1.0f;
    // Do NOT pop_back — tail stays in place

  } else if (cur == T::Portal) {
    m_state.won = true;
    m_state.stars = 3;
    if (m_state.stars > m_bestStars[m_levelIdx])
      m_bestStars[m_levelIdx] = m_state.stars;
    m_state.snake.pop_back();

  } else if (cur == T::Trap) {
    m_state.snake.pop_back();
    m_state.dead = true;
    return true;

  } else {
    m_state.snake.pop_back(); // normal move
  }

  if (!m_state.won && !m_state.dead && touchingTrap(m_state)) {
    m_state.dead = true;
    return true;
  }

  if (!m_state.won && !m_state.dead)
    applyGravity();
  return true;
}
