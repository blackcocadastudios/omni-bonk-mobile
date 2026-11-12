// Procedural stage generation: terrain, props, interactable spawns, collision
#pragma once
#include "base.h"
#include "data.h"

struct Terrain {
  int N = 0;            // vertices per side
  float size = 0;       // world extent (centered at origin)
  float cell = 1;
  std::vector<float> h; // N*N heights
  float height(float x, float z) const;
  v3 normal(float x, float z) const;
  float hv(int i, int j) const { i = clampi(i, 0, N - 1); j = clampi(j, 0, N - 1); return h[j * N + i]; }
  float half() const { return size * 0.5f; }
};

struct PropInst {
  u16 model; v3 pos; float yaw; float scale; Col tintA, tintB;
  float colR = 0;    // collision radius (0 = none)
  float colTop = 0;  // standable top height above pos.y (0 = not standable, blocks only)
  bool grass = false;
};

struct InterSpawn { int type; v3 pos; float yaw; int rarity; };

struct World {
  int map = 0, tier = 0, stage = 0; u32 seed = 1; float interDensity = 1;
  Palette pal;
  Terrain T;
  std::vector<PropInst> props;
  std::vector<InterSpawn> inters;
  v3 spawnPoint;
  // spatial hash of collidable props
  float gCell = 8; int gN = 0;
  std::vector<std::vector<int>> grid;
  void generate(int map, int tier, int stage, u32 seed, int extraShrines, int extraPots);
  void buildGrid();
  // highest walkable surface under (x,z) not above yRef+step
  float groundAt(float x, float z, float yRef) const;
  // push a circle out of blocking props; returns true if hit a wall (for Monke climb)
  bool collide(v3& pos, float r, float footY, v3* wallNormal = nullptr) const;
  bool pointBlocked(v3 p) const;
  bool inBounds(float x, float z, float margin = 0) const { float hh = T.half() - margin; return fabsf(x) < hh && fabsf(z) < hh; }
  float playableHalf() const { return T.half() * 0.86f; }
};
