// Rendering of the game world (entities, effects). World-space UI lives in ui.cpp.
#include "i18n.h"
#include "game.h"
#include "render.h"

static int interModel(const Game& g, const Inter& it) {
  switch (it.type) {
    case I_CHEST: return M_CHEST; case I_CHEST_GOLD: return M_CHEST_GOLD; case I_CHARGE_SHRINE: return M_SHRINE_CHARGE; case I_GREED_SHRINE: return M_SHRINE_GREED;
    case I_CURSED_SHRINE: return M_SHRINE_CURSED; case I_CHALLENGE_SHRINE: return M_SHRINE_CHALLENGE; case I_MOAI: return M_MOAI; case I_SHADY: return M_SHADY;
    case I_MICROWAVE: return M_MICROWAVE; case I_SUCC_SHRINE: return M_SHRINE_SUCC; case I_BOSS_PORTAL: return g.portalOpen ? M_PORTAL_EXIT : M_PORTAL_BOSS;
    case I_POT: return M_POT; case I_POT_BIG: return M_POT_BIG; case I_POT_SILVER: return M_POT_SILVER; case I_PUMPKIN: return M_PUMPKIN; case I_TUMBLEWEED: return M_TUMBLEWEED;
    case I_CRYPT_KEY: return M_CRYPT_KEY; case I_ALTAR: return M_ALTAR;
  }
  return M_CHEST;
}

static Col withA(u32 hex, int a) { Col c = hexc(hex); c.a = (u8)clampi(a, 0, 255); return c; }

void Game::render(Renderer& R) {
  if (R.curWorld != &world || R.curGen != worldGen) { R.setWorld(&world); R.curWorld = &world; R.curGen = worldGen; }
  float rdt = clampf(realTime - lastRenderT, 0, 0.1f); lastRenderT = realTime;
  R.pal = world.pal;
  // desert sandstorms: periodic heavy fog
  stormK = 0;
  if (cfg.map == MAP_DESERT && !finalArena) { float c = fmodf(stageTime, 110.f); stormK = smooth01((c - 70) / 6.f) * (1 - smooth01((c - 96) / 6.f)); }
  R.fogMul = 1.f + stormK * 2.2f;
  R.setCamera(camEye, camTgt, fovDeg * DEG);
  float viewD = std::min(R.set.viewDist, 2.2f / std::max(0.002f, world.pal.fogDensity * R.fogMul)) + 6;
  bool frozen = P.zaT > 0 || freezeEnemiesT > 0;

  // ---------------- player
  {
    int pm = reverse() ? M_OVERLORD : gChars[P.ch].model;
    float hs = len(v2(P.vel.x, P.vel.z));
    int clip = !P.onGround ? CL_JUMP : P.sliding ? CL_SLIDE : (hs > 0.8f ? CL_MOVE : CL_IDLE);
    playerAnimT += rdt * (clip == CL_MOVE ? std::max(0.6f, hs / 7.4f) : 1.f);
    FrameRef frame = modelFrameRef(pm, clip, playerAnimT, animFps);
    float flash = (P.iframes > 0.05f && fmodf(realTime, 0.12f) < 0.06f) ? 0.55f : 0.f;
    Col ta(255, 255, 255, 0);
    if (P.shieldT > 0) ta = Col(120, 220, 255, 120);
    if (P.rageT > 0) ta = Col(255, 90, 90, 110);
    float ps = mut(MU_GIANT) ? 1.35f : mut(MU_POCKET) ? 0.62f : 1.f;
    R.model(pm, frame, P.pos, q4::yaw(P.yaw), v3(ps, ps, ps), flash, ta);
    if (shadowsOn) R.decal(v3(P.pos.x, 0, P.pos.z), 0.65f, SP_SHADOW, Col(0, 0, 0, 120), 0, std::max(0.f, P.pos.y - world.T.height(P.pos.x, P.pos.z)) * 0);
    if (P.shieldT > 0) R.model(M_FX_SPHERE, 0, P.pos + v3(0, 1, 0), q4(), v3(1.3f, 1.3f, 1.3f), 0, Col(120, 220, 255, 70));
  }
  // persistent weapon visuals
  for (size_t i = 0; i < P.weapons.size(); i++) {
    const WeaponInst& w = P.weapons[i]; const WeaponDef& d = gWeapons[w.def];
    float size = d.size * (1 + w.g[U_SIZE]) * P.st[S_SIZE];
    if (d.beh == WB_AURA) {
      float r = d.range * size;
      R.decal(P.pos, r, SP_DECAL_RING, withA(d.color, 150), realTime * 0.5f);
      if (particleLevel > 0 && rng.chance(0.3f)) particles(P.pos + rng.inDisk(r) + v3(0, 0.2f, 0), 1, hexc(d.color), 1.2f, 0.25f, 0.6f, true, SP_SOFT, 2);
    }
    if (d.beh == WB_BEAM && w.angle > 0 && w.target >= 0 && w.target < (int)enemies.size() && enemies[w.target].alive) {
      v3 a = P.pos + v3(0, 1.1f, 0), b = enemies[w.target].pos + v3(0, 0.8f * enemies[w.target].scale, 0);
      int n = (int)(len(b - a) / 0.5f) + 2;
      for (int k = 0; k <= n; k++) {
        float t = k / (float)n; v3 p = lerp3(a, b, t) + v3(0, sinf(t * 12 - realTime * 14) * 0.25f, 0);
        R.billboard(p, 0.55f * size, SP_SOFT, k % 2 ? Col(255, 120, 216, 220) : Col(120, 216, 255, 220), true);
      }
    }
  }
  // ---------------- enemies
  for (auto& e : enemies) {
    float d2 = dist2XZ(e.pos, camEye);
    if (d2 > viewD * viewD) continue;
    float rad = 1.2f * e.scale * (e.rank ? 3.f : 1.f) + 1;
    if (!R.sphereVisible(e.pos + v3(0, rad * 0.5f, 0), rad)) continue;
    const EnemyDef& d = gEnemies[e.type];
    int m = d.model;
    int clip = e.clip;
    float t = clip == CL_ATTACK || clip == CL_SPECIAL ? e.clipT : e.animT + e.animPhase;
    FrameRef frame = modelFrameRef(m, clip, t, animFps, clip == CL_ATTACK);
    if (frozen) frame = modelFrameRef(m, CL_IDLE, 0, 6);
    v3 pos = e.pos; float sc = e.scale;
    v3 s(sc, sc, sc);
    if (e.spawnT < 0.35f) { float k = e.spawnT / 0.35f; s = s * (0.3f + 0.7f * k); if (!e.flying) pos.y -= (1 - k) * 0.8f * sc; }
    if (!e.alive) { float k = sat(e.deathT / 0.3f); s = v3(sc * (1 + k * 0.5f), sc * std::max(0.05f, 1 - k), sc * (1 + k * 0.5f)); }
    Col ta(255, 255, 255, 0);
    if (e.freezeT > 0 || frozen) ta = Col(130, 210, 255, 200);
    else if (e.curseT > 0) ta = Col(190, 100, 255, 130);
    else if (e.poisonT > 0) ta = Col(120, 255, 90, 100);
    else if (e.burnT > 0) ta = Col(255, 140, 60, 100);
    else if (e.challenge) ta = Col(210, 140, 255, 90);
    float flash = e.hitFlash > 0 ? 0.8f : 0.f;
    if (e.elite) flash = std::max(flash, 0.08f + 0.06f * sinf(realTime * 6));
    R.model(m, frame, pos, q4::yaw(e.yaw), s, flash, ta);
    if (shadowsOn && d2 < 45 * 45 && e.alive) R.decal(v3(e.pos.x, 0, e.pos.z), e.radius * (e.flying ? 0.9f : 1.3f), SP_SHADOW, Col(0, 0, 0, e.flying ? 70 : 110));
    if (e.invuln) R.model(M_FX_SPHERE, 0, e.pos + v3(0, 2.0f * e.scale, 0), q4(), v3(3.2f, 3.2f, 3.2f) * e.scale, 0, Col(180, 160, 255, 70));
  }
  // ---------------- pickups
  for (auto& p : pickups) {
    if (p.dead || dist2XZ(p.pos, camEye) > sq(viewD * 0.7f)) continue;
    int m = M_XP_SMALL; float s = 1;
    switch (p.kind) {
      case PI_XP: m = p.value < 6 ? M_XP_SMALL : p.value < 30 ? M_XP_MED : M_XP_BIG; if (p.value > 100) s = 1.4f; break;
      case PI_GOLD: m = M_GOLD_COIN; break;
      case PI_SILVER: m = M_SILVER_COIN; break;
      case PI_BORGAR: m = M_BORGAR; s = 1.3f; break;
      case PI_POWERUP: m = M_PU_SHIELD + p.sub; s = 1.2f; break;
    }
    float bob = p.magnet ? 0 : sinf(p.t * 3 + p.pos.x) * 0.12f;
    R.model(m, gModels.info[m].clip[CL_IDLE].first, p.pos + v3(0, bob, 0), q4::yaw(p.t * 3.f + p.pos.z), v3(s, s, s));
    if (p.kind == PI_POWERUP) R.billboard(p.pos + v3(0, bob, 0), 1.6f, SP_GLOW, withA(gPowerups[p.sub].color, 140), true);
  }
  // ---------------- interactables
  for (auto& it : inters) {
    float d2 = dist2XZ(it.pos, camEye);
    if (d2 > viewD * viewD) continue;
    int m = interModel(*this, it);
    if (!R.sphereVisible(it.pos + v3(0, 1.5f, 0), gModels.info[m].radius + 2)) continue;
    FrameRef frame;
    if (it.open) frame = modelFrameRef(m, CL_SPECIAL, it.openT, animFps, true);
    else frame = modelFrameRef(m, CL_IDLE, it.animT, animFps);
    Col ta(255, 255, 255, 0), tb(255, 255, 255, 255);
    if (it.type == I_SHADY) ta = withA(kRarityColor[it.rarity], 0);
    bool dim = it.used && (it.type == I_CHARGE_SHRINE || it.type == I_GREED_SHRINE || it.type == I_CURSED_SHRINE || it.type == I_CHALLENGE_SHRINE || it.type == I_MOAI || it.type == I_SUCC_SHRINE || (it.type == I_MICROWAVE && it.uses <= 0));
    if (dim) ta = Col(90, 90, 100, 200);
    if (it.type == I_CHARGE_SHRINE && it.rarity == R_LEGENDARY && !it.used) ta = Col(255, 220, 80, 120);
    float yawv = it.yaw; if (it.type == I_SHADY || it.type == I_MOAI || it.type == I_BOSS_PORTAL || it.type == I_MICROWAVE || it.type == I_CHEST || it.type == I_CHEST_GOLD) yawv = atan2f(P.pos.x - it.pos.x, P.pos.z - it.pos.z) * (it.type == I_SHADY || it.type == I_MOAI ? 1.f : 0.f) + (it.type == I_SHADY || it.type == I_MOAI ? 0 : it.yaw);
    R.model(m, frame, it.pos, q4::yaw(yawv), v3(1, 1, 1), 0, ta, tb);
    if (shadowsOn && d2 < 50 * 50 && it.type != I_BOSS_PORTAL) R.decal(it.pos, gModels.info[m].radius * 1.2f + 0.3f, SP_SHADOW, Col(0, 0, 0, 90));
    switch (it.type) {
      case I_CHARGE_SHRINE:
        if (!it.used) {
          u32 col = it.rarity == R_LEGENDARY ? 0xffd040 : 0x40d0ff;
          R.decal(it.pos, 4.2f, SP_DECAL_RING, withA(col, 200), realTime * 0.3f);
          if (it.progress > 0) R.decal(it.pos, 4.2f * it.progress, SP_DECAL_DISC, withA(col, 120));
        } else if (it.healT > 0) R.decal(it.pos, 5.f, SP_DECAL_DISC, Col(90, 255, 120, 80));
        break;
      case I_ALTAR:
        if (!it.used) { R.decal(it.pos, 4.f, SP_DECAL_RING, Col(255, 70, 70, 220), realTime); if (it.progress > 0) R.decal(it.pos, 4.f * it.progress, SP_DECAL_DISC, Col(255, 70, 70, 120)); R.billboard(it.pos + v3(0, 1.3f, 0), 3.f, SP_GLOW, Col(255, 60, 60, 160), true); }
        break;
      case I_CHEST_GOLD: if (!it.used) R.billboard(it.pos + v3(0, 0.8f, 0), 2.6f, SP_GLOW, Col(255, 210, 80, 150), true); break;
      case I_BOSS_PORTAL: R.billboard(it.pos + v3(0, 1.8f, 0), 5.f, SP_GLOW, portalOpen ? Col(80, 190, 255, 140) : Col(255, 60, 70, 120), true); break;
      case I_CRYPT_KEY: R.billboard(it.pos + v3(0, 1.2f, 0), 2.4f, SP_GLOW, Col(120, 255, 150, 150), true); break;
      case I_MICROWAVE: if (it.uses > 0) R.billboard(it.pos + v3(0, 0.6f, 0.5f), 1.2f, SP_GLOW, withA(kRarityColor[it.rarity], 150), true); break;
      default: break;
    }
  }
  // ---------------- projectiles
  for (auto& pr : projs) {
    if (pr.dead || (pr.model <= 0 && pr.kind != PK_ENEMY)) continue;
    if (dist2XZ(pr.pos, camEye) > viewD * viewD) continue;
    v3 dir = len(pr.vel) > 0.01f ? norm(pr.vel) : v3(0, 0, 1);
    if (pr.kind == PK_ROCK_ORBIT || pr.kind == PK_SPIRAL || pr.kind == PK_SHIELD) dir = norm(v3(-sinf(pr.aux), 0, cosf(pr.aux)));
    float yaw = atan2f(dir.x, dir.z), pitch = -asinf(clampf(dir.y, -1, 1));
    q4 q = qEuler(yaw, pitch, 0);
    if (pr.kind == PK_SPIRAL || pr.kind == PK_BOOMERANG) q = q4::yaw(pr.t * (pr.spin != 0 ? pr.spin : 10));
    else if (pr.kind == PK_LOB || pr.kind == PK_ENEMY_LOB) q = qEuler(pr.t * 7, pr.t * 9, 0);
    else if (pr.spin != 0) q = q * q4::axis({1, 0, 0}, pr.t * pr.spin);
    float s = std::max(0.5f, pr.size);
    int m = pr.model;
    if (pr.enemy && m == 0) m = M_P_ENEMYBALL;
    R.model(m, 0, pr.pos, q, v3(s, s, s));
    if (m == M_P_FIREBALL || m == M_P_ENEMYBALL || m == M_P_BLOODORB || m == M_P_ORB || m == M_P_SOUL || m == M_P_ROCKET) {
      u32 c = m == M_P_FIREBALL ? 0xff8030 : m == M_P_ENEMYBALL ? 0xd040ff : m == M_P_BLOODORB ? 0xff2040 : m == M_P_ROCKET ? 0xff5fa2 : 0x40f0ff;
      R.billboard(pr.pos, 1.3f * s, SP_GLOW, withA(c, 160), true);
      if (particleLevel > 1 && rng.chance(0.35f)) { Particle q2; q2.pos = pr.pos; q2.vel = rng.onSphere() * 0.5f; q2.life = q2.maxLife = 0.35f; q2.size = 0.35f * s; q2.size1 = 0.05f; q2.c = withA(c, 200); q2.sprite = SP_SOFT; parts.push_back(q2); }
    }
    if (m == M_P_BULLET) R.billboard(pr.pos, 0.6f, SP_GLOW, Col(255, 230, 150, 160), true);
  }
  // ---------------- areas
  for (auto& a : areas) {
    if (a.dead) continue;
    if (dist2XZ(a.pos, camEye) > viewD * viewD) continue;
    float k = a.maxLife > 0 ? 1 - a.life / a.maxLife : 1; // 0 -> 1
    Col c = hexc(a.color);
    switch (a.kind) {
      case AK_FIRE: case AK_DRAGONFIRE:
        R.decal(a.pos, a.radius, SP_DECAL_DISC, Col(255, 100, 30, (u8)(70 * (1 - k * 0.6f))));
        { int nf = areas.size() > 50 ? 1 : 2; for (int f = 0; f < nf; f++) { float ph = realTime * 9 + f * 2.1f + a.pos.x; R.billboard(a.pos + v3(cosf(ph * 0.3f) * a.radius * 0.35f, 0.35f + 0.1f * sinf(ph), sinf(ph * 0.37f) * a.radius * 0.35f), a.radius * (0.5f + 0.12f * sinf(ph * 1.3f)) * (1 - k * 0.5f), SP_FLAME, Col(255, (u8)(90 + f * 50), 30, 150), true); } }
        break;
      case AK_POISON:
        R.decal(a.pos, a.radius, SP_DECAL_DISC, Col(c.r, c.g, c.b, (u8)(110 * (1 - k * 0.5f))));
        if (rng.chance(0.25f)) particles(a.pos + rng.inDisk(a.radius) + v3(0, 0.1f, 0), 1, Col(150, 255, 90, 200), 1.f, 0.25f, 0.7f, true, SP_SOFT, 1.5f);
        break;
      case AK_BLACKHOLE:
        R.model(M_P_BLACKHOLE, 0, a.pos, q4::yaw(realTime * 4) * q4::axis({1, 0, 0}, 0.3f), v3(1, 1, 1) * (0.6f + 0.4f * sinf(std::min(1.f, k * 6) * PI * 0.5f)) * (a.radius / 4.f));
        R.decal(a.pos, a.radius, SP_SWIRL, Col(140, 70, 255, 150), -realTime * 3);
        R.billboard(a.pos, a.radius * 1.2f, SP_GLOW, Col(120, 60, 255, 120), true);
        break;
      case AK_TORNADO: R.model(M_P_TORNADO, 0, a.pos, q4::yaw(realTime * 10), v3(1, 1, 1) * (a.radius / 1.6f)); if (rng.chance(0.4f)) particles(a.pos + v3(0, 0.3f, 0), 1, Col(220, 210, 190, 160), 3, 0.5f, 0.6f, false, SP_SMOKE); break;
      case AK_MINE: R.model(M_P_MINE, 0, a.pos + v3(0, 0.1f, 0), q4(), v3(1, 1, 1)); if (fmodf(realTime, 0.6f) < 0.15f) R.billboard(a.pos + v3(0, 0.4f, 0), 0.6f, SP_GLOW, Col(255, 40, 40, 220), true); break;
      case AK_EXPLOSION: case AK_LIGHT:
        if (a.kind == AK_LIGHT && a.aux >= 1) {
          if (a.aux == 1) {
            for (int y = 0; y < 9; y++) { v3 p = a.pos + v3(sinf(y * 2.3f + realTime * 40) * 0.25f, y * 1.5f + 0.6f, cosf(y * 1.7f + realTime * 37) * 0.25f); R.billboard(p, 1.8f, SP_BOLT, Col(190, 230, 255, 230), true, sinf(y * 1.3f) * 0.3f); }
            R.billboard(a.pos + v3(0, 0.5f, 0), a.radius * 2.2f, SP_GLOW, Col(170, 220, 255, (u8)(220 * (1 - k))), true);
          } else {
            v3 s0 = a.vel + v3(0, 1, 0), s1 = a.pos + v3(0, 1, 0); int n = std::max(3, (int)(len(s1 - s0) / 0.6f));
            for (int i = 0; i <= n; i++) { float t = i / (float)n; v3 p = lerp3(s0, s1, t) + v3(sinf(t * 20 + realTime * 50) * 0.2f, cosf(t * 17) * 0.2f, 0); R.billboard(p, 0.7f, SP_SOFT, Col(190, 230, 255, 230), true); }
          }
          break;
        }
        R.model(M_FX_SPHERE, 0, a.pos, q4::yaw(a.pos.x), v3(1, 1, 1) * a.radius * (0.35f + 0.65f * sqrtf(k)), 0, Col(c.r, c.g, c.b, (u8)(170 * (1 - k))));
        if (a.kind == AK_LIGHT) R.decal(a.pos, a.radius, SP_DECAL_DISC, Col(255, 250, 200, (u8)(160 * (1 - k))));
        else R.decal(a.pos, a.radius * (0.5f + 0.5f * k), SP_DECAL_RING, Col(c.r, c.g, c.b, (u8)(200 * (1 - k))));
        break;
      case AK_SHOCKWAVE:
        if (a.target == 1) R.model(M_FX_SLASH, 0, a.pos, qEuler(a.aux, 0, sinf(a.pos.x * 3) * 0.25f), v3(a.radius, 1, a.radius) * (0.85f + 0.15f * k), 0, Col((u8)(c.r * 0.85f), (u8)(c.g * 0.85f), (u8)(c.b * 0.9f), (u8)(150 * (1 - k * k))));
        else if (a.target == 2) R.model(M_FX_CONE, 0, a.pos, q4::yaw(a.aux), v3(a.radius, a.radius * 0.8f, a.radius) * (0.4f + 0.6f * k), 0, Col(c.r, c.g, c.b, (u8)(200 * (1 - k))));
        else R.model(M_FX_RING, 0, a.pos, q4(), v3(a.radius * (0.3f + 0.7f * k), 1, a.radius * (0.3f + 0.7f * k)), 0, Col(c.r, c.g, c.b, (u8)(240 * (1 - k))));
        break;
      case AK_TELEGRAPH:
        R.decal(a.pos, a.radius, SP_DECAL_RING, Col(c.r, c.g, c.b, 220));
        R.decal(a.pos, a.radius * k, SP_DECAL_DISC, Col(c.r, c.g, c.b, 110));
        break;
      case AK_BOSSFIRE: {
        int beams = (int)a.knock;
        for (int b = 0; b < beams; b++) { float ang = a.aux + b * TAU / beams; R.model(M_FX_BEAM, 0, a.pos, q4::yaw(ang), v3(0.7f, 0.7f, a.radius), 0, Col(c.r, c.g, c.b, 220)); R.model(M_FX_BEAM, 0, a.pos, q4::yaw(ang), v3(0.3f, 0.3f, a.radius), 0, Col(255, 255, 255, 200)); }
      } break;
      default: break;
    }
  }
  // ---------------- allies
  for (auto& a : allies) {
    if (a.kind == 0) R.model(M_GHOST, modelFrameRef(M_GHOST, CL_MOVE, a.t, animFps), a.pos - v3(0, 0.7f, 0), q4::yaw(atan2f(a.vel.x, a.vel.z)), v3(0.55f, 0.55f, 0.55f), 0, Col(150, 230, 255, 120));
    else if (a.kind == 2) R.model(M_GOBLIN, modelFrameRef(M_GOBLIN, CL_MOVE, a.t, animFps), a.pos, q4::yaw(atan2f(a.vel.x, a.vel.z)), v3(0.8f, 0.8f, 0.8f), 0, Col(190, 120, 255, 90));
    else R.model(M_P_BOB, modelFrameRef(M_P_BOB, CL_MOVE, a.t, animFps), a.pos, q4::yaw(atan2f(a.vel.x, a.vel.z)), v3(1.4f, 1.4f, 1.4f));
  }
  // ---------------- particles
  for (auto& p : parts) {
    float k = 1 - p.life / p.maxLife;
    float s = lerpf(p.size, p.size1 > 0 ? p.size1 : p.size, k);
    Col c = p.c; c.a = (u8)(p.c.a * (1 - k * k));
    R.billboard(p.pos, s, p.sprite, c, p.add, p.rot);
  }
  if (stormK > 0.05f && particleLevel > 0) {
    for (int i = 0; i < (int)(stormK * 6); i++) { Particle q; q.pos = camEye + v3(rng.range(-14, 14), rng.range(-2, 5), rng.range(-14, 14)); q.vel = v3(9, 0.5f, 3); q.life = q.maxLife = 0.8f; q.size = 1.4f; q.c = Col(230, 200, 150, 90); q.sprite = SP_SMOKE; q.add = false; parts.push_back(q); }
  }
}
