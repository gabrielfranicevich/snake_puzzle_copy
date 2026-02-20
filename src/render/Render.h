#pragma once
#include "../core/Core.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Encapsulates all OpenGL rendering state and logic
class Renderer {
public:
  Renderer();
  ~Renderer();

  // Initializes OpenGL state, compiles shaders, sets up VAO/VBOs
  void init(int width, int height);

  // Updates logic and renders the current frame
  // Takes pure game state and time as input to draw
  void renderFrame(const GameState &state, int currentLevel, int totalLevels,
                   float time);

  // Window resize callback
  void resize(int w, int h);

private:
  struct Opt {
    glm::vec4 c = {1, 1, 1, 1};
    float r = 0, t = 0;
    int fx = 0;
    GLuint tex = 0;
  };

  void dR(float px, float py, float w, float h, const Opt &o);
  // Draw rect centered at (cx,cy) with given size and rotation angle in radians
  void dRot(float cx, float cy, float w, float h, float angle, const Opt &o);
  void dC(int gx, int gy, float sz, const Opt &o);

  void dCh(char c, float px, float py, float sc, glm::vec4 col);
  float dStr(const char *s, float px, float py, float sc, glm::vec4 col);
  float strW(const char *s, float sc);

  void drawCity();
  void drawTile(int gx, int gy);
  void drawApple(int gx, int gy, float t);
  void drawPortal(int gx, int gy, float t);
  void drawBox(int gx, int gy);
  void drawTrap(int gx, int gy, float angle);
  void drawSnakeSegmentF(float gx, float gy, float sz, glm::vec4 col,
                         bool isHead, bool isTail, float angle, float t);
  void drawStar(float cx2, float cy2, float r, bool filled);
  void drawButton(float px, float py, float w, float h, glm::vec4 bg,
                  const char *label, glm::vec4 tc, float sc);

  GLuint m_prog;
  GLuint m_vao, m_vbo, m_ebo;
  GLint m_lMVP, m_lCol, m_lRnd, m_lTime, m_lFx;
  GLint m_lUseTex;
  glm::mat4 m_proj;

  // Textures
  GLuint m_texApple;
  GLuint m_texBlock;
  GLuint m_texTrap;
  GLuint m_texPortal;
  GLuint m_texSnakeHead;
  GLuint m_texSnakeMid;
  GLuint m_texSnakeTail;

  GLuint loadSVGTexture(const char *filepath, int texWidth, int texHeight);

  int m_W, m_H;
  int m_ox; // x-offset to center board horizontally

  float cx(int gx) { return m_ox + gx * CELL; }
  float cy(int gy) { return HUD_H + gy * CELL; }
};
