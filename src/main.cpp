#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "core/Core.h"
#include "game/Levels.h"
#include "game/Game.h"
#include "render/Render.h"

// The main Application state
struct App {
    GameEngine engine;
    Renderer   renderer;
    float      time = 0.0f;
};

// We need a global pointer just for the GLFW callback
static App* g_app = nullptr;

static void keyCB(GLFWwindow* win, int key, int, int act, int) {
    if (!g_app) return;
    if (act != GLFW_PRESS) return;

    if (key == GLFW_KEY_ESCAPE) { 
        glfwSetWindowShouldClose(win, true); 
        return; 
    }

    const GameState& state = g_app->engine.getState();

    if (state.dead) {
        if (key == GLFW_KEY_R || key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
            g_app->engine.restartLevel();
        }
        return;
    }

    if (state.won) {
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE || key == GLFW_KEY_N) {
            g_app->engine.nextLevel();
        }
        if (key == GLFW_KEY_R) {
            g_app->engine.restartLevel();
        }
        return;
    }

    switch (key) {
        case GLFW_KEY_UP:    case GLFW_KEY_W: g_app->engine.doMove({0, -1}); break;
        case GLFW_KEY_DOWN:  case GLFW_KEY_S: g_app->engine.doMove({0, 1}); break;
        case GLFW_KEY_LEFT:  case GLFW_KEY_A: g_app->engine.doMove({-1, 0}); break;
        case GLFW_KEY_RIGHT: case GLFW_KEY_D: g_app->engine.doMove({1, 0}); break;
        
        case GLFW_KEY_Z:
        case GLFW_KEY_U: g_app->engine.undo(); break;
        
        case GLFW_KEY_R: g_app->engine.restartLevel(); break;
        case GLFW_KEY_N: g_app->engine.nextLevel(); break;
        case GLFW_KEY_P: g_app->engine.prevLevel(); break;
    }
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // ... (rest of main initialization)
    GLFWwindow* win = glfwCreateWindow(1006, 577, "Snake Puzzle: Slither to Eat", nullptr, nullptr);
    if (!win) { std::cerr << "Window failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwSetKeyCallback(win, keyCB);

    auto mouseCB = [](GLFWwindow* w, int btn, int act, int) {
        if (!g_app || btn != GLFW_MOUSE_BUTTON_LEFT || act != GLFW_PRESS) return;
        
        double cx, cy;
        glfwGetCursorPos(w, &cx, &cy);
        int ww, wh, fw, fh;
        glfwGetWindowSize(w, &ww, &wh);
        glfwGetFramebufferSize(w, &fw, &fh);
        if (ww == 0 || wh == 0) return;
        
        float x = (float)cx * ((float)fw / ww);
        float y = (float)cy * ((float)fh / wh);

        // Reset button
        if (x >= 12 && x <= 52 && y >= 10 && y <= 50) {
            g_app->engine.restartLevel();
            return;
        }

        if (g_app->engine.getState().dead) {
            // Death overlay restart button: same position as win overlay next button
            float bw = 180, bh = 46;
            float bx = (fw - bw) * 0.5f;
            float ph = 240.f;
            float by = (fh - ph) * 0.5f + ph - 70.f;
            if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) {
                g_app->engine.restartLevel();
            }
            return;
        }

        if (g_app->engine.getState().won) {
            float bw = 180, bh = 46;
            float bx = (fw - bw) * 0.5f;
            float by = (fh - 260) * 0.5f + 190.f; // ppy + ph - 70 = (fh-260)/2 + 260 - 70 = (fh-260)/2 + 190
            
            if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) {
                g_app->engine.nextLevel();
            }
        }
    };
    glfwSetMouseButtonCallback(win, mouseCB);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "GLEW failed\n"; return 1; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Instantiate our core objects
    App app;
    g_app = &app; // Bind for the keyboard callback

    app.renderer.init(1006, 577);
    app.engine.loadLevel(0); // Start at level 0
    std::cout << "Level loaded. State config: W=" << app.engine.getState().w << " H=" << app.engine.getState().h << " SnakeLen=" << app.engine.getState().snake.size() << std::endl;

    double prev = glfwGetTime();
    
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - prev);
        prev = now;
        app.time += dt;

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        app.renderer.resize(w, h);

        app.engine.tick(dt);
        
        glfwPollEvents();
        
        // Render the current state cleanly
        int curLevel = app.engine.getCurrentLevel();
        int totLevel = getNumLevels();
        app.renderer.renderFrame(app.engine.getState(), curLevel, totLevel, app.time);
        
        glfwSwapBuffers(win);
    }

    g_app = nullptr;
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
