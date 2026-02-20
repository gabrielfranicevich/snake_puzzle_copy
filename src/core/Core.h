#pragma once
#include <vector>
#include <deque>
#include <cstdint>
#include <algorithm>

// ─── Layout Constants ────────────────────────────────────────────────────────
constexpr int CELL   = 76;   // pixels per grid cell
constexpr int HUD_H  = 60;   // top HUD bar height
constexpr int BOT_H  = 38;   // bottom hint bar height
constexpr int MG     = 24;   // max grid dimension

// ─── Tile Types ──────────────────────────────────────────────────────────────
enum class T : uint8_t { Void, Floor, Apple, Portal, Box, Trap };

struct V2 {
    int x, y;
    bool operator==(const V2& o) const { return x == o.x && y == o.y; }
    V2   operator+(const V2& o) const  { return {x + o.x, y + o.y}; }
};

// ─── Game State ──────────────────────────────────────────────────────────────
struct Snap {
    std::deque<V2> snake;
    std::deque<V2> prevSnake;
    std::vector<T> grid;
    int apples, moves;
};

// Pure state, no loading logic (moved to engine)
struct GameState {
    int w = 0, h = 0;
    std::vector<T> grid;
    std::deque<V2> snake;  // [0]=head
    std::deque<V2> prevSnake; 
    int apples = 0, moves = 0, stars = 0;
    bool won = false, dead = false;
    V2 lastDir = {0, 0};
    float winTimer = 0, deadTimer = 0, eatFlash = 0, fallShake = 0, moveTimer = 1.0f;
    std::vector<Snap> hist;

    T& at(int x, int y)       { return grid[y * w + x]; }
    T  at(int x, int y) const { return grid[y * w + x]; }
    // safeAt returns T::Void for any coordinate outside the defined map grid
    T safeAt(int x, int y) const {
        if (x < 0 || x >= w || y < 0 || y >= h) return T::Void;
        return grid[y * w + x];
    }
};
