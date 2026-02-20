#include "Render.h"
#include <iostream>
#include <cstdio>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

static const char* VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=uMVP*vec4(aPos,0,1); }
)";

static const char* FS = R"(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform vec4  uColor;
uniform float uRound; 
uniform float uTime;
uniform int   uFx;
uniform int   uUseTex;
uniform sampler2D uTex;

void main(){
    vec2  p  = vUV - 0.5;
    float a  = 1.0;

    if(uRound > 0.001){
        float r = 0.5 * uRound;
        vec2  q = abs(p) - (0.5 - r);
        float d = length(max(q, 0.0)) - r;
        a = 1.0 - smoothstep(-0.02, 0.02, d);
    }

    vec4 c = uColor;

    if(uFx == 4){
        float t = vUV.y;
        vec3 top = vec3(0.53, 0.81, 0.92);
        vec3 bot = vec3(0.77, 0.91, 0.97);
        c.rgb = mix(top, bot, t);
        c.a   = 1.0;
    }
    if(uFx == 5){
        float grain = 0.5 + 0.5*sin(vUV.y * 28.0 + vUV.x * 3.0);
        c.rgb = mix(c.rgb, c.rgb * 1.15, grain * 0.35);
    }
    if(uFx == 1){
        // Clockwise spiral: negate time to flip rotation direction
        float angle = atan(p.y, p.x) - uTime * 2.8;
        float r2    = length(p);
        float spiral= 0.5 + 0.5*sin(angle * 4.0 - r2 * 18.0);
        c.rgb += vec3(0.6, 0.5, 1.0) * spiral * (1.0 - r2*2.0) * 0.55;
        c.rgb = clamp(c.rgb, 0.0, 1.0);
    }
    if(uFx == 2){
        float shine = smoothstep(0.0, 0.22, 0.30 - length(p - vec2(-0.13,-0.16)));
        c.rgb += shine * 0.5;
    }
    if(uFx == 3){
        float spots = smoothstep(0.0, 0.06, 0.09 - length(mod(p + 0.25, 0.25) - 0.125));
        c.rgb = mix(c.rgb, c.rgb * 0.62, spots);
    }
    if(uFx == 6){
        float hi = smoothstep(0.42, 0.50, vUV.y);
        c.rgb = mix(c.rgb, c.rgb * 1.22, hi);
        float sh = smoothstep(0.0, 0.08, vUV.y);
        c.rgb = mix(c.rgb * 0.70, c.rgb, sh);
    }

    if(uRound > 0.5){
        float v = 1.0 - 0.18*dot(p*1.6, p*1.6);
        c.rgb *= max(v, 0.75);
    }

    if (uUseTex == 1) {
        vec4 texCol = texture(uTex, vUV);
        FragColor = texCol * vec4(c.rgb, c.a * a);
    } else {
        FragColor = vec4(c.rgb, c.a * a);
    }
}
)";

static GLuint mkSh(GLenum t, const char* src){
    GLuint s=glCreateShader(t);
    glShaderSource(s,1,&src,nullptr); glCompileShader(s);
    int ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){ char b[512]; glGetShaderInfoLog(s,512,nullptr,b); std::cerr<<b; }
    return s;
}

Renderer::Renderer() : m_prog(0), m_vao(0), m_vbo(0), m_ebo(0), m_ox(0), 
    m_texApple(0), m_texBlock(0), m_texTrap(0), m_texPortal(0) {}

Renderer::~Renderer() {
    if (m_prog) glDeleteProgram(m_prog);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

void Renderer::init(int width, int height) {
    m_W = width;
    m_H = height;
    
    GLuint vs=mkSh(GL_VERTEX_SHADER,VS), fs=mkSh(GL_FRAGMENT_SHADER,FS);
    m_prog=glCreateProgram();
    glAttachShader(m_prog,vs); glAttachShader(m_prog,fs);
    glLinkProgram(m_prog); glDeleteShader(vs); glDeleteShader(fs);
    m_lMVP =glGetUniformLocation(m_prog,"uMVP");
    m_lCol =glGetUniformLocation(m_prog,"uColor");
    m_lRnd =glGetUniformLocation(m_prog,"uRound");
    m_lTime=glGetUniformLocation(m_prog,"uTime");
    m_lFx  =glGetUniformLocation(m_prog,"uFx");
    m_lUseTex = glGetUniformLocation(m_prog, "uUseTex");
    glUseProgram(m_prog);
    glUniform1i(glGetUniformLocation(m_prog, "uTex"), 0);
    
    float v[]={0,0,0,0, 1,0,1,0, 1,1,1,1, 0,1,0,1};
    unsigned idx[]={0,1,2,0,2,3};
    glGenVertexArrays(1,&m_vao); glGenBuffers(1,&m_vbo); glGenBuffers(1,&m_ebo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER,m_vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,(void*)8);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // Load SVG textures from assets/
    m_texApple  = loadSVGTexture("assets/apple.svg",  128, 128);
    m_texBlock  = loadSVGTexture("assets/block.svg",  128, 128);
    m_texTrap   = loadSVGTexture("assets/trap.svg",   128, 128);
    m_texPortal = loadSVGTexture("assets/vortex.svg", 128, 128);
}

GLuint Renderer::loadSVGTexture(const char* filepath, int texW, int texH) {
    NSVGimage* img = nsvgParseFromFile(filepath, "px", 96.0f);
    if (!img) {
        std::cerr << "Failed to load SVG: " << filepath << "\n";
        return 0;
    }
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    std::vector<unsigned char> pixels(texW * texH * 4, 0);
    float scaleX = texW / img->width;
    float scaleY = texH / img->height;
    float scale = std::min(scaleX, scaleY);
    nsvgRasterize(rast, img, 0, 0, scale, pixels.data(), texW, texH, texW * 4);
    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void Renderer::resize(int w, int h) {
    m_W = w; m_H = h;
    glViewport(0,0,w,h);
    m_proj = glm::ortho(0.f, (float)w, (float)h, 0.f, -1.f, 1.f);
}

void Renderer::dR(float px, float py, float w, float h, const Opt& o){
    glm::mat4 m = glm::translate(glm::mat4(1.0f),{px,py,0});
    m = glm::scale(m,{w,h,1});
    glm::mat4 mvp = m_proj * m;
    glUseProgram(m_prog);
    glUniformMatrix4fv(m_lMVP,1,GL_FALSE,glm::value_ptr(mvp));
    glUniform4fv(m_lCol,1,glm::value_ptr(o.c));
    glUniform1f(m_lRnd,o.r);
    glUniform1f(m_lTime,o.t);
    glUniform1i(m_lFx,o.fx);
    if (o.tex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, o.tex);
        glUniform1i(m_lUseTex, 1);
    } else {
        glUniform1i(m_lUseTex, 0);
    }
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);
    glBindVertexArray(0);
}

void Renderer::dC(int gx, int gy, float sz, const Opt& o){
    dR(cx(gx)+(CELL-sz)*.5f, cy(gy)+(CELL-sz)*.5f, sz, sz, o);
}

static const uint8_t FONT[][7]={
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
    {0x1F,0x02,0x04,0x06,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // 10 R
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // 11 U
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // 12 E
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, // 13 N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // 14 O
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // 15 A
    {0x11,0x11,0x15,0x15,0x0A,0x0A,0x0A}, // 16 W
    {0x04,0x04,0x04,0x04,0x00,0x00,0x04}, // 17 !
    {0x1F,0x00,0x00,0x00,0x00,0x00,0x00}, // 18 -
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // 19 P
    {0x11,0x11,0x0A,0x0A,0x04,0x04,0x04}, // 20 V
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // 21 T
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // 22 L
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // 23 F
    {0x0E,0x11,0x10,0x1E,0x10,0x11,0x0E}, // 24 S
    {0x1F,0x01,0x01,0x07,0x01,0x01,0x1F}, // 25 Z
    {0x11,0x13,0x15,0x19,0x11,0x11,0x11}, // 26 M
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // 27 C
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, // 28 D
    {0x11,0x0A,0x04,0x04,0x04,0x04,0x04}, // 29 Y
    {0x11,0x0A,0x04,0x04,0x0A,0x11,0x00}, // 30 X
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}, // 31 I
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // 32 K
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // 33 B
    {0x1F,0x11,0x11,0x1F,0x11,0x11,0x11}, // 34 H
    {0x01,0x02,0x04,0x08,0x10,0x00,0x00}, // 35 /
    {0x0E,0x11,0x11,0x17,0x11,0x11,0x0E}, // 36 G
};

static int fi(char c){
    if(c>='0'&&c<='9') return c-'0';
    switch(c){
        case 'R': return 10; case 'U': return 11; case 'E': return 12;
        case 'N': return 13; case 'O': return 14; case 'A': return 15;
        case 'W': return 16; case '!': return 17; case '-': return 18;
        case 'P': return 19; case 'V': return 20; case 'T': return 21;
        case 'L': return 22; case 'F': return 23; case 'S': return 24;
        case 'Z': return 25; case 'M': return 26; case 'C': return 27;
        case 'D': return 28; case 'Y': return 29; case 'X': return 30;
        case 'I': return 31; case 'K': return 32; case 'B': return 33;
        case 'H': return 34; case '/': return 35; case 'G': return 36;
    }
    return -1;
}

void Renderer::dCh(char c, float px, float py, float sc, glm::vec4 col){
    int i=fi(c); if(i<0) return;
    Opt o; o.c=col;
    for(int r=0;r<7;r++)
        for(int b=0;b<5;b++)
            if(FONT[i][r]&(1<<(4-b)))
                dR(px+b*sc, py+r*sc, sc, sc, o);
}

float Renderer::dStr(const char* s, float px, float py, float sc, glm::vec4 col){
    float x=px;
    for(;*s;s++){ if(*s==' '){ x+=5*sc; continue; } dCh(*s,x,py,sc,col); x+=6*sc; }
    return x;
}

float Renderer::strW(const char* s, float sc){
    float w=0; for(;*s;s++) w+=6*sc; return w;
}

static const glm::vec4
    SKY    {0.53f,0.81f,0.92f,1},
    CITY   {0.47f,0.67f,0.78f,0.55f},
    SNK_B  {0.22f,0.68f,0.12f,1},
    SNK_D  {0.14f,0.48f,0.07f,1},
    SNK_EW {0.96f,0.96f,0.96f,1},
    SNK_EP {0.35f,0.25f,0.45f,0.9f},
    BOX_F  {0.65f,0.45f,0.20f,1},
    BOX_S  {0.40f,0.25f,0.08f,1},
    HUD_PL {0.25f,0.25f,0.32f,0.92f},
    HUD_TF {0.96f,0.96f,0.96f,1},
    HUD_BT {0.30f,0.30f,0.38f,0.88f},
    WIN_BG {0.76f,0.50f,0.26f,1},
    WIN_IB {0.96f,0.90f,0.76f,1},
    WIN_ST {0.98f,0.82f,0.12f,1},
    WIN_SE {0.30f,0.30f,0.36f,1},
    WIN_GN {0.22f,0.78f,0.22f,1},
    WIN_TN {0.45f,0.22f,0.06f,1},
    RIB    {0.80f,0.14f,0.14f,1},
    DIM    {0.40f,0.46f,0.56f,1},
    DEAD_R {0.72f,0.08f,0.08f,1};

void Renderer::drawCity(){
    struct Bld { float x, w, h; } buildings[] = {
        {0,   55, 130}, {40,  38, 180}, {80,  62, 110},
        {130, 45, 160}, {165, 70, 200}, {220, 40, 140},
        {255, 55, 170}, {300, 48, 130}, {340, 65, 190},
        {395, 42, 155}, {430, 60, 120}, {480, 75, 210},
        {540, 50, 145}, {580, 44, 175}, {615, 68, 130},
        {670, 55, 190}, {715, 40, 155}, {748, 72, 215},
        {808, 50, 140}, {848, 60, 170}, {895, 48, 125},
        {935, 65, 200}, {987, 43, 155},
    };
    Opt o; o.c=CITY;
    for(auto& b:buildings){
        float bh=std::min(b.h, (float)m_H);
        dR(b.x, (float)m_H-bh, b.w, bh, o);
    }
}

void Renderer::drawTile(int gx, int gy){
    float px=cx(gx), py=cy(gy);
    // Background color first (earthy brown base)
    Opt bg; bg.c={0.45f,0.28f,0.10f,1.f};
    dR(px, py, CELL, CELL, bg);
    // Shadow behind
    Opt sh; sh.c={0,0,0,0.22f}; sh.r=0.15f;
    dR(px+4, py+5, CELL, CELL, sh);
    // SVG block texture on top — white tint = full color passthrough
    if (m_texBlock) {
        Opt svgo; svgo.c={1,1,1,1}; svgo.tex=m_texBlock;
        dR(px, py, CELL, CELL, svgo);
    } else {
        // Fallback: manual brick look
        Opt o2; o2.c={0.53f,0.33f,0.16f,1}; o2.fx=6;
        dR(px, py, CELL, CELL, o2);
        Opt sd; sd.c={0.32f,0.18f,0.06f,1};
        dR(px+CELL-6, py+6, 6, CELL-6, sd);
        dR(px+6, py+CELL-6, CELL-6, 6, sd);
    }
}

void Renderer::drawApple(int gx, int gy, float t){
    float bob = sinf(t*2.1f + gx*1.3f)*2.5f;
    float sz  = CELL * 0.72f;
    float px  = cx(gx)+(CELL-sz)*.5f;
    float py  = cy(gy)+(CELL-sz)*.5f + bob;
    if (m_texApple) {
        // Subtle drop shadow
        Opt sh; sh.c={0,0,0,0.20f}; sh.r=1.f;
        dR(px+3, py+5, sz, sz, sh);
        // SVG apple
        Opt o; o.c={1,1,1,1}; o.tex=m_texApple;
        dR(px, py, sz, sz, o);
    } else {
        // Fallback
        Opt sh; sh.c={0,0,0,0.18f}; sh.r=1.f;
        dR(px+3, py+5, sz, sz, sh);
        Opt o; o.c={0.88f,0.18f,0.14f,1}; o.r=1.f; o.fx=2; o.t=t;
        dR(px, py, sz, sz, o);
    }
}

void Renderer::drawPortal(int gx, int gy, float t){
    float sz = CELL * 0.86f;
    if (m_texPortal) {
        // Pulsing glow ring behind
        float pulse = 0.86f + 0.14f*sinf(t*3.0f);
        Opt glow; glow.c={0.45f,0.30f,0.80f,0.5f}; glow.r=1.f;
        dC(gx, gy, sz*pulse*1.15f, glow);
        // SVG vortex — clockwise spin via uTime (shader negates time for CW)
        Opt o; o.c={1,1,1,1}; o.tex=m_texPortal; o.t=t; o.fx=1;
        dC(gx, gy, sz*pulse, o);
    } else {
        // Fallback procedural
        float pulse = 0.86f + 0.14f*sinf(t*3.0f);
        Opt ot; ot.c={0.45f,0.30f,0.80f,0.9f}; ot.r=1.f; ot.t=t;
        dC(gx, gy, sz*pulse, ot);
        Opt in; in.c={0.08f,0.08f,0.16f,1}; in.r=1.f; in.fx=1; in.t=t;
        dC(gx, gy, sz*0.72f, in);
        Opt co; co.c={0.75f,0.60f,1.0f,0.7f}; co.r=1.f;
        dC(gx, gy, sz*0.30f, co);
    }
}

void Renderer::drawTrap(int gx, int gy, float t){
    float px=cx(gx), py=cy(gy);
    // Danger red background
    Opt bg; bg.c={0.25f,0.04f,0.04f,1.f};
    dR(px, py, CELL, CELL, bg);
    if (m_texTrap) {
        // Slight pulse to make it feel threatening
        float pulse = 1.0f + 0.06f*sinf(t*4.0f + gx*0.9f + gy*1.1f);
        float sz = CELL * 0.80f * pulse;
        Opt o; o.c={1,1,1,1}; o.tex=m_texTrap;
        dR(cx(gx)+(CELL-sz)*.5f, cy(gy)+(CELL-sz)*.5f, sz, sz, o);
    } else {
        // Fallback: red X
        Opt o; o.c={0.90f,0.10f,0.10f,1};
        dR(px+8, py+8, CELL-16, CELL-16, o);
    }
}

void Renderer::drawBox(int gx, int gy){
    float sz = CELL*0.80f;
    Opt sh; sh.c={0,0,0,0.20f}; sh.r=0.12f;
    dC(gx, gy, sz+6, sh);
    Opt o; o.c=BOX_F; o.r=0.12f;
    dC(gx, gy, sz, o);
    float px=cx(gx)+(CELL-sz)*.5f, py=cy(gy)+(CELL-sz)*.5f;
    Opt x; x.c=BOX_S;
    dR(px, py+sz*.48f, sz, 2, x);
    dR(px+sz*.48f, py, 2, sz, x);
    dR(px+sz-5, py+5, 5, sz-5, x);
    dR(px+5, py+sz-5, sz-5, 5, x);
}

void Renderer::drawSnakeSegment(int gx, int gy, float sz, glm::vec4 col, bool isHead, bool isTail, float t){
    Opt sh; sh.c={0,0,0,0.17f}; sh.r=1.f;
    dC(gx, gy, sz+8, sh);
    Opt o; o.c=col; o.r=1.f; o.fx=3;
    if (isTail) o.fx = 0; // plain for tail
    dC(gx, gy, sz, o);

    if(isHead){
        float px=cx(gx)+(CELL-sz)*.5f;
        float py=cy(gy)+(CELL-sz)*.5f;
        Opt ew; ew.c=SNK_EW; ew.r=1.f;
        float ew_sz=sz*0.24f;
        float eye_y=py+sz*0.24f;
        float el=px+sz*0.20f, er=px+sz*0.58f;
        dR(el, eye_y, ew_sz, ew_sz*1.2f, ew);
        dR(er, eye_y, ew_sz, ew_sz*1.2f, ew);
        Opt ep; ep.c=SNK_EP; ep.r=0.4f;
        float lid_h=ew_sz*0.45f;
        dR(el, eye_y, ew_sz, lid_h, ep);
        dR(er, eye_y, ew_sz, lid_h, ep);
        Opt pu; pu.c={0.05f,0.05f,0.10f,1}; pu.r=1.f;
        float pu_sz=ew_sz*0.42f;
        dR(el+ew_sz*0.29f, eye_y+ew_sz*0.28f, pu_sz, pu_sz, pu);
        dR(er+ew_sz*0.29f, eye_y+ew_sz*0.28f, pu_sz, pu_sz, pu);
    }
    if (isTail) {
        // Small tapered tip indicator
        float px=cx(gx)+(CELL-sz)*.5f;
        float py=cy(gy)+(CELL-sz)*.5f;
        Opt tip; tip.c={col.r*0.7f, col.g*0.7f, col.b*0.7f, 0.8f}; tip.r=1.f;
        dR(px+sz*0.3f, py+sz*0.3f, sz*0.4f, sz*0.4f, tip);
    }
}

void Renderer::drawStar(float cx2, float cy2, float r, bool filled){
    glm::vec4 col = filled ? WIN_ST : WIN_SE;
    Opt o; o.r=1.f;
    for(int i=0;i<5;i++){
        float a=glm::radians(-90.f+i*72.f);
        o.c=col;
        dR(cx2+r*cosf(a)-r*.38f, cy2+r*sinf(a)-r*.38f, r*.76f, r*.76f, o);
    }
    o.c=filled ? glm::vec4{0.98f,0.72f,0.05f,1} : WIN_SE;
    dR(cx2-r*.32f, cy2-r*.32f, r*.64f, r*.64f, o);
}

void Renderer::drawButton(float px, float py, float w, float h, glm::vec4 bg, const char* label, glm::vec4 tc, float sc){
    Opt bg_o; bg_o.c=bg; bg_o.r=0.28f;
    dR(px,py,w,h,bg_o);
    float tw=strW(label,sc);
    dStr(label, px+(w-tw)*.5f, py+(h-7*sc)*.5f, sc, tc);
}

void Renderer::renderFrame(const GameState& state, int currentLevel, int totalLevels, float time) {
    glClear(GL_COLOR_BUFFER_BIT);

    // ── Sky ──────────────────────────────────────────────────────────────────
    Opt sky; sky.c=SKY; sky.fx=4;
    dR(0, 0, (float)m_W, (float)m_H, sky);
    drawCity();

    // ── HUD ──────────────────────────────────────────────────────────────────
    Opt hb; hb.c={0.86f,0.92f,0.96f,0.55f};
    dR(0, 0, (float)m_W, (float)HUD_H, hb);
    Opt sep; sep.c={0.55f,0.70f,0.80f,0.4f};
    dR(0, (float)HUD_H-2, (float)m_W, 2, sep);

    drawButton(12, 10, 40, 40, HUD_BT, "R", HUD_TF, 2.5f);

    {
        char buf[32]; snprintf(buf,sizeof(buf),"LEVEL %d", currentLevel + 1);
        float sc=2.8f;
        float tw=strW(buf,sc);
        float pw=tw+24, ph=36.f;
        float ppx=(m_W-pw)*.5f, ppy=(HUD_H-ph)*.5f;
        Opt pl; pl.c=HUD_PL; pl.r=0.45f;
        dR(ppx,ppy,pw,ph,pl);
        dStr(buf, ppx+(pw-tw)*.5f, ppy+(ph-7*sc)*.5f, sc, HUD_TF);
    }

    {
        char buf[16]; snprintf(buf,sizeof(buf),"%d", state.moves);
        float sc=2.5f;
        float tw=strW(buf,sc);
        dStr(buf, m_W-14-tw, (HUD_H-7*sc)*.5f, sc, HUD_TF);
    }

    // ── Board ────────────────────────────────────────────────────────────────
    m_ox = (m_W - state.w * CELL) / 2;

    // 1. Floor tiles (SVG block)
    for(int gy=0;gy<state.h;gy++)
        for(int gx=0;gx<state.w;gx++)
            if(state.at(gx,gy)==T::Floor) drawTile(gx,gy);

    // 2. Boxes
    for(int gy=0;gy<state.h;gy++)
        for(int gx=0;gx<state.w;gx++)
            if(state.at(gx,gy)==T::Box) drawBox(gx,gy);

    // 3. Portal / vortex (SVG, clockwise)
    for(int gy=0;gy<state.h;gy++)
        for(int gx=0;gx<state.w;gx++)
            if(state.at(gx,gy)==T::Portal) drawPortal(gx,gy,time);

    // 4. Apples (SVG)
    for(int gy=0;gy<state.h;gy++)
        for(int gx=0;gx<state.w;gx++)
            if(state.at(gx,gy)==T::Apple) drawApple(gx,gy,time);

    // 5. Trap tiles (SVG)
    for(int gy=0;gy<state.h;gy++)
        for(int gx=0;gx<state.w;gx++)
            if(state.at(gx,gy)==T::Trap) drawTrap(gx,gy,time);

    // 6. Snake (back-to-front: tail first, head last)
    int sLen=(int)state.snake.size();
    for(int i=sLen-1;i>=0;i--){
        auto& seg = state.snake[i];
        float t = (sLen>1) ? (float)i/(sLen-1) : 0.f;
        glm::vec4 col = glm::mix(SNK_B, SNK_D*1.3f, t*0.4f);
        bool isHead = (i == 0);
        bool isTail = (i == sLen - 1);
        float extra = (isHead && state.eatFlash>0.f) ? state.eatFlash*7.f : 0.f;
        float sz;
        if (isHead)       sz = CELL*0.82f + extra;
        else if (isTail)  sz = CELL*0.58f;  // tapered tail
        else              sz = CELL*0.70f - t*CELL*0.06f;
        drawSnakeSegment(seg.x, seg.y, sz, col, isHead, isTail, time);
    }

    // ── Footer ───────────────────────────────────────────────────────────────
    Opt bb; bb.c={0.86f,0.92f,0.96f,0.55f};
    dR(0, (float)(m_H-BOT_H), (float)m_W, (float)BOT_H, bb);
    Opt bsep; bsep.c={0.55f,0.70f,0.80f,0.40f};
    dR(0, (float)(m_H-BOT_H), (float)m_W, 2, bsep);
    { float sc=2.f, y=(float)(m_H-BOT_H)+(BOT_H-7*sc)*.5f;
      dStr("WASD MOVE", 14,       y, sc, DIM);
      dStr("R RESET",   m_W*.28f, y, sc, DIM);
      dStr("Z UNDO",    m_W*.50f, y, sc, DIM);
      dStr("N SKIP",    m_W*.70f, y, sc, DIM); }

    // ── Death Overlay ─────────────────────────────────────────────────────────
    if(state.dead){
        float alp=std::min(1.f, state.deadTimer*2.5f);
        Opt ov; ov.c={0.f,0.f,0.f, 0.55f*alp};
        dR(0, 0, (float)m_W, (float)m_H, ov);

        float pw=340, ph=240;
        float ppx=(m_W-pw)*.5f, ppy=(m_H-ph)*.5f;
        // Dark red border
        Opt bd; bd.c={0.50f,0.04f,0.04f,alp}; bd.r=0.18f;
        dR(ppx-4, ppy-4, pw+8, ph+8, bd);
        // Panel
        Opt panel; panel.c={0.18f,0.04f,0.04f,alp}; panel.r=0.18f;
        dR(ppx, ppy, pw, ph, panel);
        // Red ribbon accent
        Opt rib2; rib2.c={0.72f,0.08f,0.08f,alp}; rib2.r=0.10f;
        dR(ppx+20, ppy-14, pw-40, 36, rib2);

        { float sc=3.6f; const char* txt="YOU DIED";
          float tw=strW(txt,sc);
          dStr(txt, (m_W-tw)*.5f, ppy+50, sc, {1.0f,0.88f,0.88f,alp}); }

        { float sc=2.0f; const char* sub="PRESS R TO RESTART";
          float tw=strW(sub,sc);
          dStr(sub, (m_W-tw)*.5f, ppy+110, sc, {0.80f,0.55f,0.55f,alp}); }

        // Restart button
        { float bw=180,bh=46,bx=(m_W-bw)*.5f,by=ppy+ph-70;
          Opt bn; bn.c=DEAD_R; bn.c.a=alp; bn.r=0.45f;
          dR(bx,by,bw,bh,bn);
          float sc=3.0f; const char* rl="RESTART";
          float tw=strW(rl,sc);
          dStr(rl, (m_W-tw)*.5f, by+(bh-7*sc)*.5f, sc, {1,1,1,alp}); }
    }

    // ── Win Overlay ──────────────────────────────────────────────────────────
    if(state.won){
        float alp=std::min(1.f, state.winTimer*2.5f);
        Opt ov; ov.c={0.f,0.f,0.f, 0.62f*alp};
        dR(0, 0, (float)m_W, (float)m_H, ov);

        float pw=360, ph=260;
        float ppx=(m_W-pw)*.5f, ppy=(m_H-ph)*.5f;
        Opt bd; bd.c={0.40f,0.22f,0.06f, alp}; bd.r=0.18f;
        dR(ppx-4, ppy-4, pw+8, ph+8, bd);
        Opt wood; wood.c=WIN_BG; wood.r=0.18f; wood.fx=5;
        dR(ppx, ppy, pw, ph, wood);
        Opt inn; inn.c=WIN_IB; inn.r=0.14f;
        dR(ppx+16, ppy+80, pw-32, ph-100, inn);

        float ry=ppy-14;
        Opt rib; rib.c=RIB; rib.r=0.1f;
        dR(ppx+20, ry, pw-40, 38, rib);

        float sr=22.f, sy=ppy-2;
        float sx=(float)m_W*.5f - sr*2.6f;
        for(int i=0;i<3;i++) drawStar(sx+i*sr*2.6f, sy, sr, i < state.stars);

        { char buf[32]; snprintf(buf,sizeof(buf),"LEVEL %d", currentLevel + 1);
          float sc=3.2f, tw=strW(buf,sc);
          dStr(buf, (m_W-tw)*.5f, ppy+76, sc, WIN_TN); }

        { float sc=3.6f; const char* txt="COMPLETED!";
          float tw=strW(txt,sc);
          dStr(txt, (m_W-tw)*.5f, ppy+102, sc, WIN_TN); }

        { float bw=180,bh=46,bx=(m_W-bw)*.5f,by=ppy+ph-70;
          Opt bn; bn.c=WIN_GN; bn.r=0.45f;
          dR(bx,by,bw,bh,bn);
          Opt bhi; bhi.c={0.35f,0.88f,0.35f,1}; bhi.r=0.45f;
          dR(bx+4,by+4,bw-8,18,bhi);
          float sc=3.0f; const char* nl="NEXT";
          float tw=strW(nl,sc);
          dStr(nl, (m_W-tw)*.5f, by+(bh-7*sc)*.5f, sc, {1,1,1,1}); }

        if(currentLevel == totalLevels - 1){
            float sc=2.5f; const char* fin="ALL LEVELS DONE!";
            float tw=strW(fin,sc);
            dStr(fin, (m_W-tw)*.5f, ppy+ph-26, sc, {0.98f,0.80f,0.18f,1});
        }
    }
}
