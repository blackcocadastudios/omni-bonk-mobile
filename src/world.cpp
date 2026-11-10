#include "i18n.h"
#include "world.h"
#include "models.h"

float Terrain::height(float x, float z) const {
  float fx = (x + half()) / cell, fz = (z + half()) / cell;
  int i = (int)floorf(fx), j = (int)floorf(fz);
  float tx = fx - i, tz = fz - j;
  float a = hv(i, j), b = hv(i + 1, j), c = hv(i, j + 1), d = hv(i + 1, j + 1);
  return lerpf(lerpf(a, b, tx), lerpf(c, d, tx), tz);
}
v3 Terrain::normal(float x, float z) const {
  float e = cell * 0.5f;
  float hx = height(x + e, z) - height(x - e, z), hz = height(x, z + e) - height(x, z - e);
  return norm(v3(-hx, 2 * e, -hz));
}

static float heightFn(int map, float x, float z, float half, u32 seed) {
  float r = std::max(fabsf(x), fabsf(z)) / half;            // square falloff
  float edge = smooth01((r - 0.84f) / 0.14f);
  float h = 0;
  if (map == MAP_FOREST) {
    h = (fbm(x * 0.011f, z * 0.011f, seed, 4) - 0.45f) * 22.f;
    float m = fbm(x * 0.02f + 40, z * 0.02f - 13, seed + 9, 3);
    h += smooth01((m - 0.62f) * 9.f) * 7.f;                   // steep plateaus / mesas
    h += sinf(x * 0.05f) * cosf(z * 0.043f) * 1.5f;
  } else if (map == MAP_DESERT) {
    float d = sinf(x * 0.035f + fbm(x * 0.01f, z * 0.01f, seed, 2) * 6.f) * 0.5f + 0.5f;
    h = d * d * 6.f + (fbm(x * 0.008f, z * 0.008f, seed + 3, 3) - 0.45f) * 16.f;
    float m = fbm(x * 0.018f + 9, z * 0.018f + 5, seed + 11, 3);
    h += smooth01((m - 0.66f) * 10.f) * 9.f;
  } else {
    h = (fbm(x * 0.015f, z * 0.015f, seed, 3) - 0.45f) * 9.f;
    h += smooth01((fbm(x * 0.02f, z * 0.02f, seed + 4, 2) - 0.6f) * 8.f) * 4.f;
  }
  // flatten spawn area
  float ds = sqrtf(x * x + z * z);
  float flat = smooth01((ds - 8.f) / 18.f);
  h = lerpf(0.0f, h, flat);
  h += edge * 34.f + edge * fbm(x * 0.05f, z * 0.05f, seed + 2, 2) * 18.f;
  return h;
}

void World::buildGrid() {
  gN = (int)ceilf(T.size / gCell) + 1;
  grid.assign(gN * gN, {});
  for (size_t i = 0; i < props.size(); i++) {
    const PropInst& p = props[i];
    if (p.colR <= 0) continue;
    float rr = p.colTop > 0 ? std::max(p.colR, gModels.info[p.model].radius * p.scale * 1.05f) : p.colR;
    int x0 = (int)((p.pos.x - rr + T.half()) / gCell), x1 = (int)((p.pos.x + rr + T.half()) / gCell);
    int z0 = (int)((p.pos.z - rr + T.half()) / gCell), z1 = (int)((p.pos.z + rr + T.half()) / gCell);
    for (int z = std::max(0, z0); z <= std::min(gN - 1, z1); z++)
      for (int x = std::max(0, x0); x <= std::min(gN - 1, x1); x++) grid[z * gN + x].push_back((int)i);
  }
}

float World::groundAt(float x, float z, float yRef) const {
  float g = T.height(x, z);
  int cx = (int)((x + T.half()) / gCell), cz = (int)((z + T.half()) / gCell);
  if (cx < 0 || cz < 0 || cx >= gN || cz >= gN) return g;
  for (int idx : grid[cz * gN + cx]) {
    const PropInst& p = props[idx];
    if (p.colTop <= 0) continue;
    float d2 = sq(p.pos.x - x) + sq(p.pos.z - z);
    float rr = p.colR * 0.92f;
    if (d2 < rr * rr) {
      // dome-ish top so edges are lower
      float top = p.pos.y + p.colTop * (1.f - 0.35f * d2 / (rr * rr));
      if (top <= yRef + 0.45f && top > g) g = top;
    }
  }
  return g;
}

bool World::pointBlocked(v3 p) const {
  int cx = (int)((p.x + T.half()) / gCell), cz = (int)((p.z + T.half()) / gCell);
  if (cx < 0 || cz < 0 || cx >= gN || cz >= gN) return false;
  for (int idx : grid[cz * gN + cx]) {
    const PropInst& q = props[idx];
    if (q.colTop <= 0) continue;
    const ModelInfo& mi = gModels.info[q.model];
    float r = std::max(q.colR * 1.15f, mi.radius * q.scale * 1.05f);
    float top = std::max(q.colTop, mi.height * q.scale) + 0.4f;
    if (dist2XZ(q.pos, p) < r * r && p.y < q.pos.y + top) return true;
  }
  return false;
}

bool World::collide(v3& pos, float r, float footY, v3* wallN) const {
  bool hit = false;
  int cx = (int)((pos.x + T.half()) / gCell), cz = (int)((pos.z + T.half()) / gCell);
  for (int dz = -1; dz <= 1; dz++)
    for (int dx = -1; dx <= 1; dx++) {
      int gx = cx + dx, gz = cz + dz;
      if (gx < 0 || gz < 0 || gx >= gN || gz >= gN) continue;
      for (int idx : grid[gz * gN + gx]) {
        const PropInst& p = props[idx];
        if (p.colTop > 0) {
          float top = p.pos.y + p.colTop;
          if (footY >= top - 0.5f) continue; // standing on it
        }
        float ddx = pos.x - p.pos.x, ddz = pos.z - p.pos.z;
        float d2 = ddx * ddx + ddz * ddz, rr = p.colR + r;
        if (d2 < rr * rr && d2 > 1e-6f) {
          float d = sqrtf(d2); float push = rr - d;
          pos.x += ddx / d * push; pos.z += ddz / d * push;
          if (wallN) *wallN = v3(ddx / d, 0, ddz / d);
          hit = true;
        }
      }
    }
  // world bounds (steep mountain edge) - soft wall
  float lim = playableHalf() + 6;
  if (fabsf(pos.x) > lim) { pos.x = signf(pos.x) * lim; hit = true; if (wallN) *wallN = v3(-signf(pos.x), 0, 0); }
  if (fabsf(pos.z) > lim) { pos.z = signf(pos.z) * lim; hit = true; if (wallN) *wallN = v3(0, 0, -signf(pos.z)); }
  return hit;
}

void World::generate(int map_, int tier_, int stage_, u32 seed_, int extraShrines, int extraPots) {
  map = map_; tier = tier_; stage = stage_; seed = seed_;
  pal = gMaps[map].tiers[std::min(2, tier)].pal;
  // stage 2/3 inside a tier get progressively darker (same palette family)
  if (stage > 0) {
    float k = 1.f - 0.08f * stage;
    auto dk = [&](u32& c) { Col cc = mulc(hexc(c), k); c = (cc.r << 16) | (cc.g << 8) | cc.b; };
    dk(pal.skyTop); dk(pal.skyHorizon); dk(pal.fog); dk(pal.ambientSky);
    pal.fogDensity *= 1.0f + 0.1f * stage;
  }
  Rng rng(seed * 7919u + 13);
  T.N = 121; T.size = 250; T.cell = T.size / (T.N - 1);
  T.h.resize(T.N * T.N);
  for (int j = 0; j < T.N; j++)
    for (int i = 0; i < T.N; i++) {
      float x = -T.half() + i * T.cell, z = -T.half() + j * T.cell;
      T.h[j * T.N + i] = heightFn(map, x, z, T.half(), seed);
    }
  spawnPoint = v3(0, T.height(0, 0), 0);
  props.clear(); inters.clear();
  float H = playableHalf();
  Col fol1 = hexc(pal.foliage1), fol2 = hexc(pal.foliage2), trunk = hexc(pal.trunk), rock = hexc(pal.rock);
  auto slopeOk = [&](float x, float z, float maxS) { return T.normal(x, z).y > maxS; };
  auto add = [&](int model, float x, float z, float yaw, float sc, Col ta, Col tb, float colR, float colTop, float sink = 0.1f) {
    PropInst p; p.model = (u16)model; p.pos = v3(x, T.height(x, z) - sink * sc, z); p.yaw = yaw; p.scale = sc; p.tintA = ta; p.tintB = tb; p.colR = colR * sc; p.colTop = colTop * sc;
    props.push_back(p);
  };
  auto clearSpawn = [&](float x, float z, float r) { return x * x + z * z > r * r; };
  auto scatter = [&](int count, std::function<void(float, float, Rng&)> f, float minSpawn, float maxSlope) {
    for (int i = 0, tries = 0; i < count && tries < count * 6; tries++) {
      float x = rng.range(-H, H), z = rng.range(-H, H);
      if (!clearSpawn(x, z, minSpawn) || !slopeOk(x, z, maxSlope)) continue;
      f(x, z, rng); i++;
    }
  };
  auto mixc2 = [&](Col a, Col b, Rng& r) { return mixc(a, b, r.f()); };
  if (map == MAP_FOREST) {
    // tree clusters via density noise
    for (float z = -H; z < H; z += 4.2f)
      for (float x = -H; x < H; x += 4.2f) {
        float d = fbm(x * 0.02f, z * 0.02f, seed + 77, 3);
        float jx = x + rng.range(-1.8f, 1.8f), jz = z + rng.range(-1.8f, 1.8f);
        if (!clearSpawn(jx, jz, 12) || !slopeOk(jx, jz, 0.72f)) continue;
        if (d > 0.52f && rng.chance(0.55f)) {
          int m = rng.chance(0.55f) ? (rng.chance(0.5f) ? M_TREE_BLOB : M_TREE_BLOB2) : (rng.chance(0.5f) ? M_PINE : M_PINE2);
          float sc = rng.range(0.85f, 1.35f);
          add(m, jx, jz, rng.f() * TAU, sc, mixc2(fol1, fol2, rng), trunk, m >= M_PINE ? 0.35f : 0.45f, 0, 0.1f);
        } else if (d > 0.42f && rng.chance(0.08f)) {
          add(M_BUSH, jx, jz, rng.f() * TAU, rng.range(0.8f, 1.3f), mixc2(fol1, fol2, rng), trunk, 0, 0);
        }
      }
    scatter(34, [&](float x, float z, Rng& r) { add(M_ROCK_BIG, x, z, r.f() * TAU, r.range(0.8f, 1.5f), fol1, rock, 2.5f, 2.9f, 0.3f); }, 14, 0.6f);
    scatter(70, [&](float x, float z, Rng& r) { add(M_ROCK_MED, x, z, r.f() * TAU, r.range(0.8f, 1.4f), fol1, rock, 1.1f, 1.25f, 0.2f); }, 8, 0.5f);
    scatter(140, [&](float x, float z, Rng& r) { add(M_ROCK_SMALL, x, z, r.f() * TAU, r.range(0.7f, 1.4f), fol1, rock, 0, 0); }, 4, 0.4f);
    scatter(22, [&](float x, float z, Rng& r) { add(M_LOG, x, z, r.f() * TAU, r.range(0.8f, 1.2f), fol1, trunk, 0, 0, 0.1f); }, 8, 0.8f);
    scatter(22, [&](float x, float z, Rng& r) { add(M_STUMP, x, z, r.f() * TAU, r.range(0.8f, 1.3f), fol1, trunk, 0.5f, 0.55f); }, 8, 0.8f);
    scatter(70, [&](float x, float z, Rng& r) { add(M_MUSHROOM, x, z, r.f() * TAU, r.range(0.7f, 1.6f), fol1, trunk, 0, 0); }, 6, 0.6f);
    for (int s = 0; s < 8; s++) { // ruins sites
      float cx = rng.range(-H * 0.85f, H * 0.85f), cz = rng.range(-H * 0.85f, H * 0.85f);
      if (!clearSpawn(cx, cz, 25)) continue;
      for (int k = 0; k < 4; k++) {
        float a = rng.f() * TAU, d = rng.range(2, 7);
        if (rng.chance(0.5f)) add(M_RUIN_WALL, cx + cosf(a) * d, cz + sinf(a) * d, a + PI / 2, rng.range(0.8f, 1.2f), fol1, rock, 0, 0, 0.3f);
        else add(M_RUIN_PILLAR, cx + cosf(a) * d, cz + sinf(a) * d, a, rng.range(0.7f, 1.3f), fol1, rock, 0.6f, 3.2f, 0.2f);
      }
    }
    scatter(2400, [&](float x, float z, Rng& r) { PropInst& p = (add(M_GRASS, x, z, r.f() * TAU, r.range(0.7f, 1.4f), mixc(hexc(pal.ground2), fol2, r.f() * 0.5f), trunk, 0, 0, 0.05f), props.back()); p.grass = true; }, 2, 0.6f);
    scatter(380, [&](float x, float z, Rng& r) { PropInst& p = (add(M_FLOWERS, x, z, r.f() * TAU, r.range(0.7f, 1.2f), fol1, trunk, 0, 0, 0.05f), props.back()); p.grass = true; }, 4, 0.7f);
  } else if (map == MAP_DESERT) {
    scatter(170, [&](float x, float z, Rng& r) { add(r.chance(0.6f) ? M_CACTUS_PROP : M_CACTUS_PROP2, x, z, r.f() * TAU, r.range(0.8f, 1.3f), fol1, trunk, 0.4f, 0, 0.1f); }, 10, 0.75f);
    scatter(40, [&](float x, float z, Rng& r) { add(M_DEAD_TREE, x, z, r.f() * TAU, r.range(0.8f, 1.4f), fol1, trunk, 0.3f, 0); }, 10, 0.7f);
    scatter(60, [&](float x, float z, Rng& r) { add(M_SANDSTONE, x, z, r.f() * TAU, r.range(0.8f, 1.6f), fol1, rock, 2.0f, 2.3f, 0.3f); }, 14, 0.6f);
    scatter(6, [&](float x, float z, Rng& r) { float y = r.f() * TAU; float c = cosf(y), s = sinf(y); add(M_SANDSTONE_ARCH, x, z, y, 1.0f, fol1, rock, 0, 0, 0.2f);
      PropInst a = props.back(); for (int sd = -1; sd <= 1; sd += 2) { PropInst leg = a; leg.model = M_ROCK_SMALL; leg.scale = 0.001f; leg.pos.x = x + c * 2.0f * sd; leg.pos.z = z - s * 2.0f * sd; leg.colR = 0.8f; leg.colTop = 0; props.push_back(leg); } }, 25, 0.8f);
    scatter(7, [&](float x, float z, Rng& r) { add(M_OBELISK, x, z, r.f() * TAU, r.range(0.9f, 1.3f), fol1, rock, 0.7f, 0); }, 20, 0.8f);
    scatter(26, [&](float x, float z, Rng& r) { add(M_BONES, x, z, r.f() * TAU, r.range(0.8f, 1.5f), fol1, rock, 0, 0); }, 8, 0.7f);
    for (int o = 0; o < 4; o++) { float cx = rng.range(-H * 0.8f, H * 0.8f), cz = rng.range(-H * 0.8f, H * 0.8f); if (!clearSpawn(cx, cz, 20)) continue; for (int k = 0; k < 5; k++) { float a = rng.f() * TAU, d = rng.range(2, 6); add(M_PALM, cx + cosf(a) * d, cz + sinf(a) * d, rng.f() * TAU, rng.range(0.8f, 1.2f), mixc(fol1, fol2, rng.f()), trunk, 0.3f, 0); } }
    scatter(900, [&](float x, float z, Rng& r) { PropInst& p = (add(M_DESERT_GRASS, x, z, r.f() * TAU, r.range(0.6f, 1.2f), mixc(hexc(pal.ground3), fol2, 0.35f), trunk, 0, 0, 0.05f), props.back()); p.grass = true; }, 2, 0.6f);
    scatter(120, [&](float x, float z, Rng& r) { add(M_ROCK_SMALL, x, z, r.f() * TAU, r.range(0.7f, 1.4f), fol1, rock, 0, 0); }, 4, 0.4f);
  } else {
    // graveyard rows
    for (int row = 0; row < 26; row++) {
      float cz = rng.range(-H, H), cx = rng.range(-H, H), yaw = rng.chance(0.5f) ? 0 : PI / 2;
      for (int k = 0; k < 10; k++) {
        float x = cx + (yaw == 0 ? k * 2.6f : 0) + rng.range(-0.3f, 0.3f), z = cz + (yaw == 0 ? 0 : k * 2.6f) + rng.range(-0.3f, 0.3f);
        if (!clearSpawn(x, z, 14) || fabsf(x) > H || fabsf(z) > H || !slopeOk(x, z, 0.8f)) continue;
        add(rng.chance(0.75f) ? M_TOMB : M_TOMB_CROSS, x, z, yaw + rng.range(-0.2f, 0.2f), rng.range(0.8f, 1.2f), fol1, rock, 0.45f, 0, 0.1f);
      }
    }
    scatter(70, [&](float x, float z, Rng& r) { add(M_DEAD_TREE2, x, z, r.f() * TAU, r.range(0.9f, 1.5f), fol1, trunk, 0.35f, 0); }, 12, 0.7f);
    scatter(36, [&](float x, float z, Rng& r) { add(M_FENCE, x, z, r.f() * TAU, 1.0f, fol1, rock, 0, 0); }, 12, 0.8f);
    scatter(40, [&](float x, float z, Rng& r) { add(M_LANTERN, x, z, r.f() * TAU, 1.0f, fol1, rock, 0.15f, 0); }, 8, 0.8f);
    scatter(30, [&](float x, float z, Rng& r) { add(M_ROCK_MED, x, z, r.f() * TAU, r.range(0.8f, 1.3f), fol1, rock, 1.1f, 1.25f, 0.2f); }, 10, 0.5f);
    { float a = rng.f() * TAU; add(M_CRYPT, cosf(a) * 22, sinf(a) * 22, a + PI, 1.0f, fol1, rock, 0, 0, 0.1f);
      PropInst c = props.back(); for (int k = -1; k <= 1; k++) { PropInst q = c; q.model = M_ROCK_SMALL; q.scale = 0.001f; q.pos.x += cosf(a + PI / 2) * k * 1.6f; q.pos.z += sinf(a + PI / 2) * k * 1.6f; q.colR = 2.2f; q.colTop = 0; props.push_back(q); } }
    scatter(1500, [&](float x, float z, Rng& r) { PropInst& p = (add(M_GRAVE_GRASS, x, z, r.f() * TAU, r.range(0.7f, 1.3f), mixc(hexc(pal.ground2), fol2, 0.4f), trunk, 0, 0, 0.05f), props.back()); p.grass = true; }, 2, 0.6f);
  }

  // ---------------- interactables
  std::vector<v3> taken = {spawnPoint};
  auto place = [&](int type, int count, float minSpawn, float minSep, int rarity = 0) {
    for (int i = 0, tries = 0; i < count && tries < 400 * count; tries++) {
      float x = rng.range(-H * 0.95f, H * 0.95f), z = rng.range(-H * 0.95f, H * 0.95f);
      if (!clearSpawn(x, z, minSpawn) || !slopeOk(x, z, 0.85f)) continue;
      bool ok = true; for (auto& t : taken) if (dist2XZ(t, v3(x, 0, z)) < minSep * minSep) { ok = false; break; }
      if (!ok) continue;
      InterSpawn s; s.type = type; s.pos = v3(x, T.height(x, z), z); s.yaw = rng.f() * TAU; s.rarity = rarity >= 0 ? rarity : 0;
      if (type == I_SHADY || type == I_MICROWAVE) { float r = rng.f(); s.rarity = r < 0.45f ? R_COMMON : r < 0.75f ? R_RARE : r < 0.93f ? R_EPIC : R_LEGENDARY; }
      if (type == I_CHARGE_SHRINE && rng.chance(0.06f)) s.rarity = R_LEGENDARY; // golden shrine
      inters.push_back(s); taken.push_back(s.pos); i++;
    }
  };
  place(I_BOSS_PORTAL, 1, H * 0.55f, 10);
  if (map == MAP_GRAVEYARD) place(I_CRYPT_KEY, 4, 35, 30);
  place(I_CHARGE_SHRINE, 15 + extraShrines, 14, 14);
  place(I_GREED_SHRINE, 6, 14, 10);
  place(I_CURSED_SHRINE, 2, 25, 20);
  place(I_CHALLENGE_SHRINE, 3, 20, 20);
  place(I_MOAI, 3, 18, 18);
  place(I_SHADY, 3, 16, 18);
  place(I_MICROWAVE, map == MAP_GRAVEYARD ? 4 : 2, 18, 18);
  place(I_SUCC_SHRINE, 1, 20, 20);
  place(I_CHEST, 36, 6, 7);
  int potType = map == MAP_GRAVEYARD ? I_PUMPKIN : I_POT;
  place(potType, 40 + extraPots, 4, 4);
  place(map == MAP_GRAVEYARD ? I_PUMPKIN : I_POT_BIG, 15 + extraPots / 3, 6, 5);
  place(I_POT_SILVER, 6, 8, 6);
  if (map == MAP_DESERT) place(I_TUMBLEWEED, 30, 6, 5);
  // remove props overlapping interactables / spawn
  std::vector<PropInst> keep; keep.reserve(props.size());
  for (auto& p : props) {
    bool bad = false;
    float clearR = p.grass ? 1.2f : 3.0f + p.colR;
    for (auto& s : inters) if (dist2XZ(p.pos, s.pos) < clearR * clearR) { bad = true; break; }
    if (!p.grass && dist2XZ(p.pos, spawnPoint) < 36) bad = true;
    if (!bad) keep.push_back(p);
  }
  props.swap(keep);
  buildGrid();
}
