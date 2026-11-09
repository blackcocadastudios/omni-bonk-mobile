// Renderer: instanced flip-book models, terrain chunks, sky, decals, billboards, UI
#pragma once
#include "base.h"
#include "gfx.h"
#include "art.h"
#include "models.h"
#include "world.h"

struct Inst { float p[4]; float q[4]; float s[4]; u8 ta[4]; u8 tb[4]; }; // 56 B
struct DrawCmd { u32 key; Inst in; };
struct BillI { float x, y, z, size; u16 u0, v0, u1, v1; u8 r, g, b, a; float rot; }; // 32 B
struct DecalI { float x, z, radius, rot; u16 u0, v0, u1, v1; u8 r, g, b, a; float yoff; }; // 32 B
struct UIVert { float x, y, u, v; u8 r, g, b, a; };

struct RenderSettings {
  float renderScale = 0.75f; bool snap = true; bool shadows = true; float viewDist = 120; float fxAlpha = 1.0f; float vignette = 0.35f;
};

struct Renderer {
  RenderSettings set;
  int screenW = 1280, screenH = 720, rw = 0, rh = 0;
  GLuint screenFbo = 0; // 0 = default framebuffer
  struct Stats { int draws = 0, insts = 0, tris = 0; } stats, lastStats; // per frame (profiling)
  float time = 0;
  // camera
  v3 camPos, camTarget; float fov = 60 * DEG; m4 view, proj, vp; v3 camRight, camUp, camFwd;
  float frustum[6][4];
  // scene lighting
  Palette pal; float fogMul = 1.f;
  bool ready = false;
  const World* curWorld = nullptr; int curGen = -1;

  void init();
  void destroy();
  void setWorld(const World* w);
  void beginFrame(int w, int h, float dt);
  void setCamera(v3 eye, v3 target, float fovy);
  void model(int m, int frame, v3 pos, q4 rot, v3 scale, float flash = 0, Col ta = Col(255, 255, 255, 0), Col tb = Col(255, 255, 255, 255)) { FrameRef f; f.frame = f.next = frame; model(m, f, pos, rot, scale, flash, ta, tb); }
  void model(int m, FrameRef fr, v3 pos, q4 rot, v3 scale, float flash = 0, Col ta = Col(255, 255, 255, 0), Col tb = Col(255, 255, 255, 255));
  void billboard(v3 pos, float size, int sprite, Col c, bool additive, float rot = 0);
  void decal(v3 pos, float radius, int sprite, Col c, float rot = 0, float yoff = 0);
  void renderScene(bool drawWorld);
  // UI (pixel coordinates, origin top-left)
  void uiQuad(float x, float y, float w, float h, const ARect& r, Col c);
  void uiRect(float x, float y, float w, float h, Col c);
  void uiFlush();
  bool worldToScreen(v3 p, v2& out) const;
  bool sphereVisible(v3 c, float r) const;
  void readPixels(std::vector<u8>& out, int& w, int& h);
  UIAtlas atlas;

private:
  GLuint vao = 0;
  GLuint progModel = 0, progFx = 0, progSky = 0, progBill = 0, progDecal = 0, progPost = 0, progUI = 0;
  GLuint texLayers = 0, texAtlas = 0, texHeight = 0;
  GLuint mAnim = 0, mStat = 0, mIdx = 0;
  DynBuffer instBuf, billBuf, decalBuf, uiBuf, uiIdx;
  GLuint quadVbo = 0, gridVbo = 0, gridIbo = 0; int gridIdxCount = 0;
  RenderTarget scene;
  struct Chunk { GLuint vbo = 0, ibo = 0; int idx = 0; float x0, z0, x1, z1, y0, y1; std::vector<int> props; };
  std::vector<Chunk> chunks; int chunkN = 0;
  const World* world = nullptr;
  std::vector<DrawCmd> cmds, fxCmds;
  std::vector<BillI> billAdd, billAlpha;
  std::vector<DecalI> decals;
  std::vector<UIVert> uiVerts;
  std::vector<u16> uiIndices;
  struct U { GLint vp, cam, snap, time, tex, sun, sunCol, ambSky, ambGnd, fogCol, fogDen; } um, uf;
  void compile();
  void drawModelCmds(std::vector<DrawCmd>& list, bool fx);
  void setModelUniforms(GLuint prog, U& u);
  void bindModelStreams(const ModelInfo& mi, int frame, int next, size_t instOff);
  void destroyChunks();
};
extern Renderer gR;
