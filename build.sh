#!/usr/bin/env bash
# build.sh — Build GPU-accelerated Snake
set -e

OS=$(uname)
SRC="src/main.cpp src/game/Levels.cpp src/game/Game.cpp src/render/Render.cpp"
OUT=snake_puzzle

if [ "$OS" = "Linux" ]; then
  echo "[Linux] Building with g++..."
  g++ -Isrc $SRC -o $OUT \
    -std=c++17 -O2 \
    -lGL -lGLEW -lglfw \
    $(pkg-config --cflags glm 2>/dev/null || true)
  echo "Done. Run with: ./$OUT"

elif [ "$OS" = "Darwin" ]; then
  echo "[macOS] Building with clang++..."
  clang++ -Isrc $SRC -o $OUT \
    -std=c++17 -O2 \
    -framework OpenGL \
    $(pkg-config --cflags --libs glfw3 glew 2>/dev/null || \
      echo "-I/opt/homebrew/include -L/opt/homebrew/lib -lglfw -lGLEW")
  echo "Done. Run with: ./$OUT"

else
  echo "Windows: Use CMakeLists.txt or compile manually:"
  echo "  cl -Isrc src/*.cpp src/*/*.cpp /std:c++17 /O2 opengl32.lib glew32.lib glfw3.lib"
fi