cmake_minimum_required(VERSION 3.10)
project(SnakePuzzle)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add source files
set(SOURCES
    src/main.cpp
    src/game/Levels.cpp
    src/game/Game.cpp
    src/render/Render.cpp
)

add_executable(snake_puzzle ${SOURCES})

# Include directory for src/ reference
target_include_directories(snake_puzzle PRIVATE src)

# Find and link dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Try GLEW (Linux/Windows often uses it)
find_package(GLEW)
if(GLEW_FOUND)
    target_link_libraries(snake_puzzle PRIVATE GLEW::glew)
else()
    # Fallback to direct linking if module not found
    target_link_libraries(snake_puzzle PRIVATE GLEW)
endif()

# We might need GLM (usually header only)
find_path(GLM_INCLUDE_DIR glm/glm.hpp)
if(GLM_INCLUDE_DIR)
    target_include_directories(snake_puzzle PRIVATE ${GLM_INCLUDE_DIR})
endif()

target_link_libraries(snake_puzzle PRIVATE
    OpenGL::GL
    glfw
)