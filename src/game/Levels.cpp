#include "Levels.h"
#include "../core/Core.h"
#include <cstring>

struct LvDef {
  const char *name;
  int w, h;
  const char *rows[MG];
};

// Snake format: H=head, M=mid-body segment, B=tail
// The parser adds them in order: head first, then all M segments (left→right
// top→bottom), then tail last. Trap tiles use 'X'.

// Trap tiles use 'X'. Box tiles use '#'. Portal tiles use 'P'.
// Apple tiles use 'A'. Floor tiles use '='. Void tiles use ' '.

static const LvDef LEVELS[] = {
    // 0 — example
    {"Example",
     12,
     4,
     {
         "  #BMH A X  ",
         "  ======== P",
         "  X      ===",
         "   X=       ",
     }},
    // 1 — slide right, gravity demo, no apples
    {"Slide Right",
     12,
     4,
     {
         "  BMH       ",
         "  ======== P",
         "         ===",
         "            ",
     }},
    // 2 — eat one apple then portal
    {"First Bite",
     11,
     4,
     {
         " BMH       ",
         "  ======   ",
         "       A  P",
         "       ====",
     }},
    // 3 — staircase drop
    {"Staircase",
     10,
     5,
     {
         " BMH      ",
         "  ==== A  ",
         "      ====",
         "         P",
         "         =",
     }},
    // 4 — apple above, must climb up
    {"Up and Over",
     9,
     6,
     {
         "    A    ",
         "   ===   ",
         "  =====  ",
         " =======P",
         "   HMB  =",
         "   ==    ",
     }},
    // 5 — two apples, then find portal below
    {"Double Dip",
     11,
     6,
     {
         "  BMH      ",
         "  =======  ",
         "        A  ",
         "        =  ",
         "    A   =P ",
         "    ======",
     }},
    // 6 — gap jump with gravity
    {"The Gap",
     12,
     5,
     {
         "  BMH       ",
         "  ====      ",
         "         ===",
         "         A P",
         "         ===",
     }},
    // 7 — U-turn platform
    {"U-Turn",
     11,
     7,
     {
         " BMH       ",
         "  ======   ",
         "         = ",
         "     A   = ",
         "   =======P",
         "          =",
         "           ",
     }},
    // 8 — introduce box pushing
    {"Push It",
     12,
     6,
     {
         "  BMH      ",
         "  ====     ",
         "       #   ",
         "       =  =",
         "       ===P",
         "          =",
     }},
    // 9 — multi-apple cascade
    {"Cascade",
     13,
     7,
     {
         "  BMH        ",
         "  ======     ",
         "         =   ",
         "       A ====",
         "       ======",
         "     A    ==P",
         "     ========",
     }},
    // 10 — long snake navigation
    {"Long Way",
     14,
     8,
     {
         "  BBMH        ",
         "     ======== ",
         "           =  ",
         "        A  =  ",
         "        ====  ",
         "     A  =  =  ",
         "     =======P ",
         "            = ",
     }},
    // 11 — box bridge over gap, traps on sides
    {"Box Bridge",
     13,
     6,
     {
         " BMH         ",
         "  =====      ",
         "       =     ",
         "       #  ==P",
         "       =X = =",
         "         X   ",
     }},
    // 12 — zigzag with apples and a trap
    {"Zigzag",
     12,
     8,
     {
         " BMH        ",
         "  =====     ",
         "       =    ",
         "    A  =    ",
         "    ====    ",
         "      =  X= ",
         "    A =====P",
         "    ========",
     }},
};
static const int NL_COUNT = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));

int getNumLevels() { return NL_COUNT; }

void loadLevelData(int idx, GameState &state) {
  if (idx < 0 || idx >= NL_COUNT)
    return;
  const LvDef &d = LEVELS[idx];
  state.w = d.w;
  state.h = d.h;
  state.grid.assign(state.w * state.h, T::Void);
  state.snake.clear();
  state.apples = 0;

  V2 head{-1, -1};
  V2 tail{-1, -1};
  std::vector<V2> mids;  // 'M' mid segments
  std::vector<V2> extra; // 'B' extra body (for long snakes with multi-B)

  for (int gy = 0; gy < state.h; gy++) {
    const char *row = d.rows[gy];
    int rl = (int)strlen(row);
    for (int gx = 0; gx < state.w; gx++) {
      char c = (gx < rl) ? row[gx] : ' ';
      switch (c) {
      case '=':
        state.at(gx, gy) = T::Floor;
        break;
      case 'A':
        state.at(gx, gy) = T::Apple;
        state.apples++;
        break;
      case 'P':
        state.at(gx, gy) = T::Portal;
        break;
      case '#':
        state.at(gx, gy) = T::Box;
        break;
      case 'X':
        state.at(gx, gy) = T::Trap;
        break;
      case 'H':
        head = {gx, gy};
        break;
      case 'M':
        mids.push_back({gx, gy});
        break;
      case 'B':
        extra.push_back({gx, gy});
        break;
      case ' ':
      case '.':
        break; // Void space
      }
    }
  }

  // Build snake: head first, then mids, then extra bodies, tail last
  // 'B' now acts as extra body segments (behind M), maintaining the
  // order they appear in the map (left→right, top→bottom).
  if (head.x >= 0)
    state.snake.push_back(head);
  for (auto &m : mids)
    state.snake.push_back(m);
  for (auto &b : extra)
    state.snake.push_back(b);

  // Build permanent trap mask — used to restore T::Trap when a box moves off
  // one
  state.trapMask.assign(state.w * state.h, false);
  for (int i = 0; i < state.w * state.h; i++)
    if (state.grid[i] == T::Trap)
      state.trapMask[i] = true;
}
