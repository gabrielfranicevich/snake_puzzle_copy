#include "Game.h"
#include "Levels.h"
#include <iostream>

GameEngine::GameEngine() : m_levelIdx(0) {
    for (int i = 0; i < 64; i++) m_bestStars[i] = 0;
}

void GameEngine::loadLevel(int idx) {
    if (idx < 0 || idx >= getNumLevels()) return;
    m_levelIdx = idx;
    m_state = GameState(); 
    loadLevelData(idx, m_state);
}

void GameEngine::nextLevel() {
    loadLevel(std::min(m_levelIdx + 1, getNumLevels() - 1));
}

void GameEngine::prevLevel() {
    loadLevel(std::max(m_levelIdx - 1, 0));
}

void GameEngine::restartLevel() {
    loadLevel(m_levelIdx);
}

void GameEngine::saveState() {
    m_state.hist.push_back({m_state.snake, m_state.grid, m_state.apples, m_state.moves});
}

void GameEngine::undo() {
    if (m_state.hist.empty()) return;
    auto& s = m_state.hist.back();
    m_state.snake  = s.snake;
    m_state.grid   = s.grid;
    m_state.apples = s.apples;
    m_state.moves  = s.moves;
    m_state.eatFlash = 0;
    m_state.won  = false;
    m_state.dead = false;
    m_state.lastDir = {0, 0};
    m_state.hist.pop_back();
}

void GameEngine::tick(float dt) {
    m_state.eatFlash  = std::max(0.f, m_state.eatFlash  - dt * 3.5f);
    m_state.fallShake = std::max(0.f, m_state.fallShake - dt * 5.0f);
    if (m_state.won)  m_state.winTimer  += dt;
    if (m_state.dead) m_state.deadTimer += dt;
}

int GameEngine::getBestStars(int levelIdx) const {
    if (levelIdx < 0 || levelIdx >= 64) return 0;
    return m_bestStars[levelIdx];
}

// Returns true if any snake segment occupies a Trap tile
static bool touchingTrap(const GameState& s) {
    for (const auto& seg : s.snake) {
        if (s.safeAt(seg.x, seg.y) == T::Trap)
            return true;
    }
    return false;
}

void GameEngine::applyGravity() {
    // We use a hard limit so the snake can't fall infinitely on a bug
    static constexpr int MAX_FALL = 256;

    for (int step = 0; step < MAX_FALL && !m_state.won && !m_state.dead; ++step) {
        // Phase 1: does ANY segment have a solid tile directly below it?
        // Apples and Portals also count as solid — they only activate when the
        // head deliberately moves into them via doMove (key press).
        bool hasSupport = false;
        for (auto& seg : m_state.snake) {
            T below = m_state.safeAt(seg.x, seg.y + 1);
            if (below == T::Floor || below == T::Box ||
                below == T::Apple || below == T::Portal) {
                hasSupport = true;
                break;
            }
        }
        if (hasSupport) break; // supported — stop

        // Phase 2: would any segment fall off the bottom of the defined map?
        // (The abyss below y = h-1 is out-of-bounds void → death)
        bool fallsOff = false;
        for (auto& seg : m_state.snake) {
            if (seg.y + 1 >= m_state.h) {
                fallsOff = true;
                break;
            }
        }
        if (fallsOff) {
            m_state.dead = true;
            return;
        }

        // Phase 3: fall one row
        for (auto& seg : m_state.snake) seg.y++;
        m_state.fallShake = 1.0f;

        if (touchingTrap(m_state)) { m_state.dead = true; return; }
        // Note: NO apple/portal interaction here — they are opaque during gravity.
    }
}

bool GameEngine::doMove(V2 dir) {
    if (m_state.won || m_state.dead || m_state.snake.empty()) return false;

    // Block reversing direction when snake has ≥2 segments
    if (m_state.snake.size() > 1) {
        V2 ld = m_state.lastDir;
        if (dir.x == -ld.x && dir.y == -ld.y && (ld.x != 0 || ld.y != 0))
            return false;
    }

    V2 head = m_state.snake.front();
    V2 nh   = { head.x + dir.x, head.y + dir.y };
    // Infinite map: any position is valid — no bounds check on nh.
    // Only the tile at the destination matters.

    T t = m_state.safeAt(nh.x, nh.y);

    // Can't move into a solid floor block
    if (t == T::Floor) return false;

    if (t == T::Box) {
        // Push box: destination must be inside the defined map and empty
        V2 bh = { nh.x + dir.x, nh.y + dir.y };
        T tb = m_state.safeAt(bh.x, bh.y);
        // Block box push into walls or outside the defined grid
        if (tb != T::Void) return false;
        if (bh.x < 0 || bh.x >= m_state.w || bh.y < 0 || bh.y >= m_state.h) return false;

        saveState();
        m_state.at(nh.x, nh.y) = T::Void;
        m_state.at(bh.x, bh.y) = T::Box;
    } else {
        // Self-collision (exclude tail, which will move away)
        for (int i = 0; i < (int)m_state.snake.size() - 1; i++) {
            if (m_state.snake[i] == nh) return false;
        }
        saveState();
    }

    // Commit move
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

    if (!m_state.won && !m_state.dead) applyGravity();
    return true;
}
