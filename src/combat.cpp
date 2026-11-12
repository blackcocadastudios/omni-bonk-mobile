// Weapons, projectiles, areas, damage pipeline, item procs, enemy AI and bosses
#include "i18n.h"
#include "game.h"
#include "audio.h"

// ====================================================================== queries
void Game::buildEnemyGrid() {
  if (egHead.empty()) return;
  std::fill(egHead.begin(), egHead.end(), -1);
  egNext.assign(enemies.size(), -1);
  for (int i = 0; i < (int)enemies.size(); i++) {
    Enemy& e = enemies[i];
    if (!e.alive) continue;
    int cx = clampi((int)((e.pos.x - egOrigin) / egCell), 0, egN - 1), cz = clampi((int)((e.pos.z - egOrigin) / egCell), 0, egN - 1);
    int c = cz * egN + cx; egNext[i] = egHead[c]; egHead[c] = i;
  }
}
int Game::nearestEnemy(v3 p, float maxR, int exclude, bool) const {
  int best = -1; float bd = maxR * maxR;
  for (int i = 0; i < (int)enemies.size(); i++) {
    const Enemy& e = enemies[i];
    if (!e.alive || e.id == exclude || e.spawnT < 0.2f) continue;
    float d = dist2XZ(e.pos, p);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}
int Game::randomEnemy(v3 p, float maxR) {
  int n = 0, pick = -1;
  for (int i = 0; i < (int)enemies.size(); i++) {
    const Enemy& e = enemies[i];
    if (!e.alive || e.spawnT < 0.2f || dist2XZ(e.pos, p) > maxR * maxR) continue;
    n++; if (rng.irange(n) == 0) pick = i;
  }
  return pick;
}

// ====================================================================== damage
void Game::areaDamage(v3 c, float r, float dmg, int src, float procCoef, float knock, int exceptId, bool canCrit) {
  std::vector<int> hit;
  forEnemiesNear(c, r, [&](Enemy& e) { if (e.id != exceptId) hit.push_back((int)(&e - enemies.data())); });
  for (int i : hit) if (enemies[i].alive) hitEnemy(enemies[i], dmg, src, procCoef, 0, c, knock, canCrit);
}

void Game::lightning(v3 at, float dmg, int src, float r, float stun) {
  Area& a = addArea(); a.kind = AK_LIGHT; a.pos = at; a.radius = r; a.life = a.maxLife = 0.25f; a.color = 0xbfe8ff; a.aux = 1;
  forEnemiesNear(at, r, [&](Enemy& e) { if (stun > 0) e.stunT = std::max(e.stunT, stun); });
  areaDamage(at, r, dmg, src, 0.4f, 0.3f);
  audioPlay(SFX_ZAP);
}

void Game::hitEnemy(Enemy& e, float dmg, int src, float procCoef, float critAdd, v3 from, float knock, bool canCrit, bool noProc) {
  if (!e.alive || e.spawnT < 0.05f) return;
  if (e.invuln) { if (rng.chance(0.1f)) floatText(e.pos + v3(0, e.scale * 2.5f, 0), tr("IMMUNE"), Col(200, 200, 255), 0.8f); return; }
  Player& p = P;
  float m = p.st[S_DAMAGE];
  m += p.items[IT_DEMONICSOUL] ? std::min(1.f, p.demonicKills * 0.001f * p.items[IT_DEMONICSOUL]) : 0;
  m += p.items[IT_BEEFYRING] ? 0.2f * p.items[IT_BEEFYRING] * p.st[S_MAXHP] / 100.f : 0;
  m += p.items[IT_JOESDAGGER] ? std::min(0.5f, p.joesExec * 0.01f) : 0;
  if (p.ch == C_OGRE) m += 0.015f * p.level;
  if (p.ch == C_CALCIUM) m += 0.0075f * p.level + 0.5f * (p.st[S_MOVESPD] - 1 + p.speedDemon);
  if (p.rageT > 0) m *= 1.5f;
  if (p.items[IT_IDLEJUICE]) m *= 1 + std::min(1.f, p.idleT / 5.f) * p.items[IT_IDLEJUICE];
  if (p.items[IT_GAMERGOGGLES]) m *= 1 + (1 - p.hp / p.st[S_MAXHP]) * 0.6f * p.items[IT_GAMERGOGGLES];
  if (p.items[IT_SCARF] && !p.onGround) m *= 1 + 0.33f * p.items[IT_SCARF];
  if (p.speedBoiT > 0) m *= 2.f;
  if (e.rank != RK_NORMAL || e.elite) m *= p.st[S_ELITEDMG];
  if (e.flying && p.items[IT_EAGLECLAW]) m *= 1 + 0.66f * p.items[IT_EAGLECLAW];
  if (p.items[IT_TACTICALGLASSES] && e.hp > e.maxHp * 0.9f) m *= 1 + 0.2f * p.items[IT_TACTICALGLASSES];
  if (p.items[IT_BRASSKNUCKLES] && distXZ(e.pos, p.pos) < 4.5f) m *= 1 + 0.2f * p.items[IT_BRASSKNUCKLES];
  if (p.nextDouble && canCrit) { m *= 2; p.nextDouble = false; }
  dmg *= m;
  bool crit = false; int critLvl = 0;
  if (canCrit) {
    float cc = p.st[S_CRIT] + critAdd + p.diceCrit;
    if (p.nextCrit) { cc = std::max(cc, 100.f); p.nextCrit = false; }
    while (cc > 0 && critLvl < 3) { float c1 = std::min(cc, 100.f); if (rng.f() * 100 < c1) critLvl++; else break; cc -= 100; }
    if (critLvl > 0) { crit = true; dmg *= powf(p.st[S_CRITDMG], (float)critLvl); }
    if (crit && p.items[IT_GIANTFORK] && rng.chance(0.15f * p.items[IT_GIANTFORK])) { dmg *= 2.f; critLvl = 3; }
  }
  bool bonk = false;
  if (p.items[IT_BIGBONK] && procCoef > 0 && rng.chance(0.02f * p.items[IT_BIGBONK] * procCoef)) { dmg *= 20; bonk = true; }
  float before = e.hp;
  e.hp -= dmg;
  e.hitFlash = 0.1f;
  dmgBySrc[std::min(src, SRC_COUNT - 1)] += std::min(dmg, std::max(0.f, before));
  if (src >= 0 && src < (int)P.weapons.size()) P.weapons[src].dmgDealt += std::min(dmg, std::max(0.f, before));
  // knockback
  if (knock > 0 && e.rank == RK_NORMAL) { v3 d = e.pos - from; d.y = 0; float l = len(d); if (l > 0.01f) e.knock += d / l * knock * 3.5f * (e.elite ? 0.4f : 1.f); }
  if (dmgNumbers && (crit || bonk || rng.chance(texts.size() > 60 ? 0.25f : 1.f))) {
    Col c = bonk ? Col(255, 120, 40) : critLvl >= 3 ? Col(255, 90, 220) : critLvl == 2 ? Col(255, 140, 60) : crit ? Col(255, 225, 70) : Col(255, 255, 255);
    floatText(e.pos + v3(0, 1.4f * e.scale + 0.6f, 0), bonk ? "BONK! " + fmtShort(dmg) : fmtShort(std::max(1.f, roundf(dmg))), c, crit ? 1.25f : 1.f);
  }
  if (!noProc && procCoef > 0) {
    int rolls = 1 + p.items[IT_OPLAMP];
    for (int r = 0; r < rolls; r++) {
      auto pc = [&](int item, float ch) { return p.items[item] && rng.f() < ch * procCoef * (1 + 0.15f * (p.items[item] - 1)); };
      if (pc(IT_SPICYMEATBALL, 0.25f)) { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = e.pos + v3(0, 0.8f, 0); a.radius = 2.4f; a.life = a.maxLife = 0.3f; a.color = 0xff7040; areaDamage(e.pos, 2.4f, dmg * 0.65f, SRC_ITEM + IT_SPICYMEATBALL, 0, 0.6f, -1, false); }
      if (pc(IT_ICECUBE, 0.20f)) { e.hp -= dmg * 0.3f; if (rng.chance(0.25f)) e.freezeT = std::max(e.freezeT, 1.5f); }
      if (pc(IT_ICECRYSTAL, 0.075f)) { e.freezeT = std::max(e.freezeT, 1.2f); }
      if (p.items[IT_ICECRYSTAL] && e.freezeT > 0 && e.hp < e.maxHp * 0.15f && e.rank == RK_NORMAL) e.hp = -1;
      if (pc(IT_MOLDYCHEESE, 0.40f)) { e.poisonDps += dmg * 0.25f; e.poisonT = 3; }
      if (pc(IT_MOLDYGLOVES, 0.15f)) { Area& a = addArea(); a.kind = AK_POISON; a.pos = e.pos; a.radius = 2.0f; a.life = a.maxLife = 2.0f; a.dmg = dmg * 0.25f; a.tick = 0.5f; a.src = SRC_ITEM + IT_MOLDYGLOVES; a.color = 0x8fd040; }
      if (p.items[IT_SNEK]) { e.poisonDps += dmg * 0.1f * p.items[IT_SNEK]; e.poisonT = 3; if (rng.chance(0.05f * procCoef)) { float burst = e.poisonDps * e.poisonT; e.poisonDps = 0; e.poisonT = 0; e.hp -= burst; floatText(e.pos + v3(0, 2, 0), fmtShort(burst), Col(130, 255, 90), 1.2f); } }
      if (pc(IT_DRAGONFIRE, 0.15f)) { Area& a = addArea(); a.kind = AK_DRAGONFIRE; a.pos = e.pos; a.radius = 2.2f; a.life = a.maxLife = 2.0f; a.dmg = dmg * 0.3f; a.tick = 0.4f; a.src = SRC_ITEM + IT_DRAGONFIRE; a.color = 0xff6020; }
      if (pc(IT_LIGHTNINGORB, 0.25f)) { int t = randomEnemy(e.pos, 7); if (t >= 0) lightning(enemies[t].pos, dmg * 0.8f, SRC_ITEM + IT_LIGHTNINGORB, 1.4f, 0.5f); }
      if (p.items[IT_THUNDERMITTS] && p.thunderCd <= 0) { p.thunderCd = 10.f / p.items[IT_THUNDERMITTS]; lightning(e.pos, dmg * 2.f + 20 * (1 + stage), SRC_ITEM + IT_THUNDERMITTS, 2.2f, 0.8f); }
      if (pc(IT_POWERGLOVES, 0.08f)) { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = e.pos + v3(0, 0.8f, 0); a.radius = 4.f; a.life = a.maxLife = 0.35f; a.color = 0xffa040; areaDamage(e.pos, 4.f, dmg * 1.5f, SRC_ITEM + IT_POWERGLOVES, 0, 3.f, -1, false); shake = std::max(shake, 0.2f); }
      float bloodCh = 0.27f * p.items[IT_UNSTABLETRANSFUSION] + (p.items[IT_BLOODYCLEAVER] ? 0.5f : 0);
      if (bloodCh > 0 && rng.f() < bloodCh * procCoef) { e.bleed++; }
      if (pc(IT_CURSEDGRABBIES, 0.05f)) { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = e.pos + v3(0, 0.8f, 0); a.radius = 3.f; a.life = a.maxLife = 0.3f; a.color = 0xb040ff; areaDamage(e.pos, 3.f, dmg, SRC_ITEM + IT_CURSEDGRABBIES, 0, 0.5f, -1, false); }
      if (p.items[IT_SLURPGLOVES] && p.slurpCd <= 0) { p.slurpCd = 9.f; Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = e.pos + v3(0, 0.8f, 0); a.radius = 3.5f; a.life = a.maxLife = 0.35f; a.color = 0xff2050; areaDamage(e.pos, 3.5f, dmg * 2.f * p.items[IT_SLURPGLOVES], SRC_ITEM + IT_SLURPGLOVES, 0, 0.5f, -1, false); heal(p.st[S_MAXHP] * 0.075f); }
      if (p.items[IT_JOESDAGGER] && e.rank == RK_NORMAL && rng.f() < 0.01f * p.items[IT_JOESDAGGER] * procCoef) { e.hp = -1; p.joesExec++; floatText(e.pos + v3(0, 2, 0), tr("EXECUTED"), Col(255, 60, 60), 1.1f); }
      if (P.mirrorCd < 99 && P.items[IT_KEVIN] && P.landT > -1 && rng.f() < 0.25f * procCoef && realTime - P.lastHitT > 0.5f && fmodf(realTime, 0.5f) < 0.05f) { damagePlayer(1.f + stage, nullptr, false, true); floatText(p.pos + v3(0, 2.4f, 0), "KEVIN!", Col(255, 100, 100), 1.0f); }
      // lifesteal: chance to heal
      float ls = p.st[S_LIFESTEAL];
      while (ls > 0) { if (rng.f() * 100 < std::min(100.f, ls) * procCoef) { heal(1.f + p.st[S_MAXHP] * 0.004f); if (p.items[IT_BLOODYCLEAVER]) e.bleed++; } ls -= 100; }
      if (crit && p.items[IT_DEMONICBLADE] && rng.chance(0.25f * procCoef)) heal(2.f + p.st[S_MAXHP] * 0.01f);
      if (crit && p.items[IT_GRANDMATONIC] && rng.chance(0.5f * procCoef)) { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = e.pos + v3(0, 0.8f, 0); a.radius = 2.f; a.life = a.maxLife = 0.25f; a.color = 0xffd060; areaDamage(e.pos, 2.f, dmg * 0.5f * p.items[IT_GRANDMATONIC], SRC_ITEM + IT_GRANDMATONIC, 0, 0.3f, e.id, false); }
      if (p.items[IT_EAGLECLAW] && !e.flying && e.rank == RK_NORMAL && rng.chance(0.08f * procCoef)) e.airT = 1.0f;
    }
  }
  if (e.hp <= 0 && e.alive) killEnemy(e, src);
}

void Game::killEnemy(Enemy& e, int src) {
  if (!e.alive) return;
  e.alive = false; e.deathT = 0;
  const EnemyDef& d = gEnemies[e.type];
  P.kills++;
  if (src >= 0 && src < (int)P.weapons.size()) P.weapons[src].kills++;
  float xpScale = (1 + 0.6f * stage + 0.4f * cfg.tier) * (1 + 0.25f * P.st[S_DIFFICULTY] / 100.f);
  float xp = d.xp * xpScale * (e.elite ? 6.f : 1.f);
  if (e.type == E_SWARMGHOST || e.type == E_GHOST_RED) xp *= finalSwarm ? 0.5f : 1.f;
  dropXp(e.pos, xp);
  if (P.items[IT_ECHOSHARD] && rng.chance(0.12f * P.items[IT_ECHOSHARD])) dropXp(e.pos, xp);
  float goldCh = 0.09f + (e.elite ? 1.f : 0);
  if (rng.f() < goldCh) dropPickup(PI_GOLD, 0, e.pos, (1.f + 2.f * u() + 2.5f * stage + 2.f * cfg.tier) * (e.elite ? 5.f : 1.f));
  if (e.type == E_SKELETON_GOLD) for (int k = 0; k < 10; k++) dropPickup(PI_GOLD, 0, e.pos, 4.f + 4.f * stage);
  if (rng.chance((0.006f + 0.01f * P.st[S_DIFFICULTY] / 100.f) * (e.elite ? 8 : 1))) dropPickup(PI_SILVER, 0, e.pos, 1);
  if (rng.chance(0.004f * P.st[S_POWERUPCH] * (e.elite ? 10 : 1))) dropPickup(PI_POWERUP, rng.irange(PU_COUNT), e.pos, 1);
  if (P.items[IT_BORGAR] && rng.chance(0.02f * P.items[IT_BORGAR])) dropPickup(PI_BORGAR, 0, e.pos, 1);
  if (P.items[IT_DEMONICBLOOD] && P.demonicBlood < 100.f * P.items[IT_DEMONICBLOOD]) { P.demonicBlood += 0.5f; statsDirty = true; }
  if (P.items[IT_DEMONICSOUL]) P.demonicKills++;
  if (src >= 0 && src < (int)P.weapons.size() && P.weapons[src].def == W_BLOODMAGIC && rng.chance(0.05f)) { P.bloodMagicHp += 1; statsDirty = true; }
  if (P.items[IT_SOULHARVESTER]) {
    for (int k = 0; k < 2; k++) { Projectile& pr = addProj(); pr.kind = PK_SOUL; pr.model = M_P_SOUL; pr.pos = e.pos + v3(0, 1, 0); pr.vel = rng.onSphere() * 6.f; pr.speed = 14; pr.homing = 7; pr.life = pr.maxLife = 3; pr.dmg = 8.f * P.items[IT_SOULHARVESTER] * (1 + stage) * (1 + u()); pr.src = SRC_ITEM + IT_SOULHARVESTER; pr.radius = 0.5f; pr.target = -1; pr.procCoef = 0.3f; }
  }
  if (P.items[IT_SLUTTYCANNON] && rng.chance(0.2f * P.items[IT_SLUTTYCANNON])) {
    Projectile& pr = addProj(); pr.kind = PK_HOMING; pr.model = M_P_ROCKET; pr.pos = e.pos + v3(0, 1.5f, 0); pr.vel = v3(0, 8, 0); pr.speed = 15; pr.homing = 6; pr.life = pr.maxLife = 3; pr.dmg = 15.f * (1 + stage) * (1 + u()); pr.explode = 2.4f; pr.src = SRC_ITEM + IT_SLUTTYCANNON; pr.radius = 0.5f; pr.target = -1; pr.procCoef = 0.3f; pr.color = 0xff5fa2;
  }
  // elites / minibosses / challenge
  if (e.elite && !e.challenge) {
    if (!firstEliteDone || rng.chance(0.2f)) { firstEliteDone = 1; Inter c; c.type = I_CHEST_GOLD; c.pos = e.pos; c.pos.y = world.T.height(e.pos.x, e.pos.z); inters.push_back(c); }
  }
  if (e.challenge) {
    int left = 0; for (auto& o : enemies) if (o.alive && o.challenge && o.aiState == e.aiState) left++;
    if (left == 0) { Inter c; c.type = I_CHEST_GOLD; c.pos = e.pos; c.pos.y = world.T.height(e.pos.x, e.pos.z); inters.push_back(c); alert(tr("CHALLENGE COMPLETE!"), tr("A golden chest appeared"), Col(194, 107, 255), 2.2f); }
  }
  if (e.rank == RK_MINIBOSS) {
    Inter c; c.type = I_CHEST_GOLD; c.pos = e.pos; c.pos.y = world.T.height(e.pos.x, e.pos.z); inters.push_back(c);
    for (int k = 0; k < 6; k++) dropPickup(PI_GOLD, 0, e.pos, 5.f + 5.f * stage);
    shake = 0.4f;
  }
  if (e.rank == RK_BOSS) { bossDied(e); shake = 0.8f; }
  Col pc = e.type == E_SLIME ? Col(90, 210, 255) : (e.type <= E_GOBLIN_TANK && e.type != E_BEE) ? Col(100, 200, 80) : Col(230, 225, 210);
  particles(e.pos + v3(0, 0.8f * e.scale, 0), e.rank ? 30 : 6, pc, e.rank ? 9.f : 4.f, 0.28f * e.scale, 0.45f, false, SP_WHITE, -14);
  particles(e.pos + v3(0, 0.6f, 0), e.rank ? 10 : 2, Col(255, 255, 255, 150), 2.f, 0.6f * e.scale, 0.4f, false, SP_SMOKE);
  if (rng.chance(0.5f)) audioPlay(SFX_KILL);
}

void Game::damagePlayer(float dmg, Enemy* src, bool canDodge, bool kevin) {
  Player& p = P;
  if (state != GS_PLAY || p.hp <= 0) return;
  if (!kevin && (p.iframes > 0 || p.shieldT > 0)) return;
  if (cfg.godMode) { dmg = 0; }
  if (canDodge && !kevin) {
    float ev = p.st[S_EVASION]; float ch = ev / (ev + 100.f);
    if (rng.f() < ch) {
      floatText(p.pos + v3(0, 2.3f, 0), tr("DODGE"), Col(170, 220, 255), 0.9f);
      if (p.items[IT_PHANTOMSHROUD]) { p.nextDouble = true; p.phantomT = 3.f; }
      if (p.ch == C_NINJA) p.nextCrit = true;
      p.iframes = 0.25f; return;
    }
  }
  if (!kevin) dmg *= 100.f / (100.f + std::max(0.f, p.st[S_ARMOR]));
  lastHitBy = kevin ? "Kevin" : src ? gEnemies[src->type].name : tr("projectile/area"); lastHitDmg = dmg;
  if (p.shield > 0) { float a = std::min(p.shield, dmg); p.shield -= a; dmg -= a; }
  if (kevin) { p.hp = std::max(1.f, p.hp - dmg); }
  else {
    if (p.hp - dmg <= 0 && p.items[IT_ZAWARUDO] > 0) { p.items[IT_ZAWARUDO]--; p.hp = 1; p.zaT = 3.f; p.iframes = 3.f; alert("ZA WARUDO!", tr("Time has stopped"), Col(255, 215, 60)); dmg = 0; statsDirty = true; }
    p.hp -= dmg;
  }
  p.lastHitT = stageTime;
  if (!kevin) p.iframes = 0.5f;
  if (dmg > 0.5f) floatText(p.pos + v3(0, 2.2f, 0), fmt("-%d", (int)ceilf(dmg)), Col(255, 70, 70), 1.0f);
  if (p.ch == C_CALCIUM) p.speedDemon *= 0.5f;
  shake = std::max(shake, 0.18f);
  audioPlay(SFX_HURT);
  plat_vibrate_safe(25);
  // on-damaged items
  if (p.items[IT_ELECTRICPLUG]) lightning(p.pos, 15.f * p.items[IT_ELECTRICPLUG] * (1 + stage) * p.st[S_DAMAGE], SRC_ITEM + IT_ELECTRICPLUG, 4.f, 0.6f);
  if (p.items[IT_TOXICBARREL]) { Area& a = addArea(); a.kind = AK_POISON; a.pos = p.pos; a.radius = 3.5f; a.life = a.maxLife = 3; a.dmg = 6.f * p.items[IT_TOXICBARREL] * (1 + stage) * (1 + u()); a.tick = 0.5f; a.src = SRC_ITEM + IT_TOXICBARREL; a.color = 0x80e040; }
  if (p.items[IT_GOLDENSHIELD]) p.gold += (2.f + 2.f * stage) * p.items[IT_GOLDENSHIELD] * p.st[S_GOLDGAIN];
  if (p.items[IT_COWARDSCLOAK]) p.cowardT = 2.f;
  if (p.items[IT_MIRROR] && p.mirrorCd <= 0) { p.mirrorCd = 5.f; p.iframes = 1.0f; if (src && src->alive) hitEnemy(*src, dmg * 3.f * p.items[IT_MIRROR] + 10, SRC_ITEM + IT_MIRROR, 0, 0, p.pos, 2.f, false); }
  if (p.items[IT_CACTUS]) { int n = 6 + 2 * p.items[IT_CACTUS]; for (int k = 0; k < n; k++) { float a = k * TAU / n; Projectile& pr = addProj(); pr.kind = PK_STRAIGHT; pr.model = M_P_SPIKE; pr.pos = p.pos + v3(0, 1, 0); pr.vel = v3(cosf(a), 0, sinf(a)) * 18.f; pr.life = pr.maxLife = 0.6f; pr.dmg = std::max(5.f, p.st[S_THORNS]) * (1 + 0.2f * p.items[IT_CACTUS]); pr.src = SRC_ITEM + IT_CACTUS; pr.radius = 0.4f; pr.pierce = 2; pr.procCoef = 0.2f; } }
  if (src && src->alive && p.st[S_THORNS] > 0) {
    float th = p.st[S_THORNS] * (1 + stage * 0.5f);
    hitEnemy(*src, th, SRC_ITEM + IT_COUNT - 1, 0, 0, p.pos, 0.5f, false);
    if (p.items[IT_QUINSMASK] && rng.chance(0.5f)) { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = src->pos + v3(0, 0.8f, 0); a.radius = 2.5f; a.life = a.maxLife = 0.3f; a.color = 0xff4040; areaDamage(src->pos, 2.5f, th * p.items[IT_QUINSMASK], SRC_ITEM + IT_QUINSMASK, 0, 1, -1, false); }
  }
  if (p.items[IT_SPEEDBOI] && p.hp < p.st[S_MAXHP] * 0.5f && p.speedBoiCd <= 0) { p.speedBoiCd = 15.f; p.speedBoiT = 3.f; alert("SPEED BOI", "", Col(0, 176, 255), 1.2f); }
  if (p.hp <= 0) { p.hp = 0; }
}

// ====================================================================== weapons
float Game::weaponDmg(const WeaponInst& w) const { return gWeapons[w.def].dmg + w.g[U_DMG]; }

void Game::updateWeapons(float dt) {
  for (size_t i = 0; i < P.weapons.size(); i++) fireWeapon(P.weapons[i], (int)i, dt);
}

void Game::fireWeapon(WeaponInst& w, int slot, float dt) {
  const WeaponDef& d = gWeapons[w.def];
  Player& p = P;
  float atk = 1 + (p.st[S_ATKSPD] + w.g[U_ATKSPD] + (p.rageT > 0 ? 50 : 0) + (p.phantomT > 0 ? 25 : 0)) / 100.f;
  if (p.items[IT_TURBOSKATES]) atk += std::min(0.4f * p.items[IT_TURBOSKATES], len(v2(p.vel.x, p.vel.z)) / 7.4f * 0.25f * p.items[IT_TURBOSKATES]);
  float cd = d.cd / std::max(0.2f, atk);
  int proj = std::max(1, d.proj + (int)w.g[U_PROJ] + (int)p.st[S_PROJ]);
  float size = d.size * (1 + w.g[U_SIZE]) * p.st[S_SIZE];
  float pspd = d.speed * (1 + w.g[U_PROJSPD]) * p.st[S_PROJSPD];
  float dur = d.dur * (1 + w.g[U_DUR]) * p.st[S_DURATION];
  float knock = d.knock * (1 + w.g[U_KNOCK]) * p.st[S_KNOCKBACK];
  float crit = w.g[U_CRIT];
  float dmg = weaponDmg(w);
  int bounce = d.bounce + (int)w.g[U_BOUNCE] + (int)p.st[S_BOUNCE];
  v3 origin = p.pos + v3(0, 1.1f, 0);
  w.t += dt;
  w.cd -= dt;
  auto camFwd = [&]() { return v3(sinf(camYaw), 0, cosf(camYaw)); };
  auto aimDir = [&](float range) -> v3 { int t = nearestEnemy(p.pos, range); if (t >= 0) { v3 dd = enemies[t].pos + v3(0, 0.8f * enemies[t].scale, 0) - origin; return norm(dd); } return v3(sinf(p.yaw), 0, cosf(p.yaw)); };
  auto slashFx = [&](v3 c, float yaw, float r, u32 col, float life = 0.18f) { Area& a = addArea(); a.kind = AK_SHOCKWAVE; a.pos = c; a.radius = r; a.life = a.maxLife = life; a.color = col; a.aux = yaw; a.target = 1; };
  switch (d.beh) {
    case WB_PROJ: {
      if (w.def == W_REVOLVER) {
        if (w.cd <= 0 && w.burst == 0) { w.burst = proj; w.burstT = 0; w.cd = cd; }
        if (w.burst > 0) {
          w.burstT -= dt;
          if (w.burstT <= 0) {
            w.burstT = 0.055f; w.burst--;
            v3 dir = aimDir(d.range); dir = norm(dir + v3(rng.range(-0.06f, 0.06f), rng.range(-0.02f, 0.03f), rng.range(-0.06f, 0.06f)));
            Projectile& pr = addProj(); pr.kind = PK_STRAIGHT; pr.model = M_P_BULLET; pr.pos = origin; pr.vel = dir * pspd; pr.life = pr.maxLife = dur; pr.dmg = dmg; pr.src = slot; pr.radius = 0.35f * size; pr.size = size; pr.crit = crit; pr.knock = knock; pr.color = d.color;
            audioPlay(SFX_SHOOT);
          }
        }
        return;
      }
      if (w.cd > 0) return;
      w.cd = cd;
      bool manual = d.manualAim;
      int pellets = w.def == W_SHOTGUN ? 5 + (proj - 1) * 2 : proj;
      v3 base = manual ? camFwd() : aimDir(d.range);
      if (manual) { int t = nearestEnemy(p.pos + camFwd() * 8.f, 10); if (t >= 0) { v3 dd = norm(enemies[t].pos + v3(0, 0.8f, 0) - origin); if (dot(dd, camFwd()) > 0.85f) base = dd; } base.y = std::max(base.y, -0.15f); }
      std::vector<int> used;
      for (int k = 0; k < pellets; k++) {
        v3 dir = base;
        if (w.def == W_SHOTGUN) { float a = (k - (pellets - 1) * 0.5f) * (0.5f / std::max(1, pellets - 1)); dir = norm(v3(base.x * cosf(a) - base.z * sinf(a), base.y + rng.range(-0.03f, 0.03f), base.x * sinf(a) + base.z * cosf(a))); }
        else if (k > 0 && !manual) {
          int t = -1; float bd = d.range * d.range;
          for (int i = 0; i < (int)enemies.size(); i++) { const Enemy& e = enemies[i]; if (!e.alive || e.spawnT < 0.2f) continue; if (std::find(used.begin(), used.end(), i) != used.end()) continue; float dd = dist2XZ(e.pos, p.pos); if (dd < bd) { bd = dd; t = i; } }
          if (t >= 0) { used.push_back(t); dir = norm(enemies[t].pos + v3(0, 0.8f * enemies[t].scale, 0) - origin); }
          else { float a = (k % 2 ? 1 : -1) * 0.2f * ((k + 1) / 2); dir = norm(v3(base.x * cosf(a) - base.z * sinf(a), base.y, base.x * sinf(a) + base.z * cosf(a))); }
        } else if (k == 0 && !manual) { int t = nearestEnemy(p.pos, d.range); if (t >= 0) used.push_back(t); }
        Projectile& pr = addProj();
        pr.pos = origin + dir * 0.4f; pr.speed = pspd; pr.vel = dir * pspd; pr.life = pr.maxLife = dur; pr.dmg = dmg; pr.src = slot; pr.size = size; pr.crit = crit; pr.knock = knock; pr.color = d.color;
        pr.radius = 0.4f * size; pr.pierce = d.pierce; pr.explode = d.explode * size; pr.model = d.projModel; pr.kind = PK_STRAIGHT; pr.bounces = bounce;
        switch (w.def) {
          case W_FIRESTAFF: pr.homing = 2.5f; pr.kind = PK_HOMING; pr.target = used.empty() ? -1 : used.back(); break;
          case W_BONE: pr.kind = PK_BOUNCE; pr.spin = 14; break;
          case W_BLOODMAGIC: case W_WIRELESS: case W_ROCKET: pr.kind = PK_HOMING; pr.homing = w.def == W_ROCKET ? 5 : 7; pr.target = used.empty() ? -1 : used.back(); if (w.def == W_ROCKET) pr.vel = v3(dir.x * 4, 7, dir.z * 4); break;
          case W_DEXECUTIONER: pr.spin = 0; pr.aux = 0.02f; break;
          case W_DICE: { int roll = 1 + rng.irange(6); pr.dmg = roll * dmg + w.g[U_DMG] * 0; pr.aux = (float)roll; pr.spin = 12; pr.kind = PK_BOUNCE; if (roll == 6) { pr.crit = 1000; p.diceCrit = std::min(50.f, p.diceCrit + 0.5f); } } break;
          case W_SNIPER: pr.radius = 0.5f * size; break;
          case W_SHOTGUN: pr.radius = 0.35f * size; break;
          default: break;
        }
      }
      audioPlay(w.def == W_FIRESTAFF ? SFX_FIRE : SFX_SHOOT);
    } break;
    case WB_MELEE: {
      if (w.burst > 0) {
        w.burstT -= dt;
        if (w.burstT <= 0) { w.burstT = 0.1f; w.burst--; w.angle += PI * 0.85f; goto doSlash; }
        return;
      }
      if (w.cd > 0) return;
      w.cd = cd; w.burst = proj - 1; w.burstT = 0.1f;
      { v3 ad = aimDir(w.def == W_KATANA ? d.range : d.range * size + 3); w.angle = atan2f(ad.x, ad.z); }
    doSlash:
      if (w.def == W_KATANA) {
        // slash centered on nearest enemies
        int t = -1; float bd = sq(d.range + size);
        for (int i = 0; i < (int)enemies.size(); i++) { const Enemy& e = enemies[i]; if (!e.alive || e.spawnT < 0.2f || e.hitCd[slot] > 0) continue; float dd = dist2XZ(e.pos, p.pos); if (dd < bd) { bd = dd; t = i; } }
        if (t < 0) return;
        v3 c = enemies[t].pos; float r = 1.9f * size;
        float yaw = atan2f(c.x - p.pos.x, c.z - p.pos.z) + rng.range(-0.6f, 0.6f);
        slashFx(c + v3(0, 0.9f, 0), yaw, r, d.color);
        forEnemiesNear(c, r, [&](Enemy& e) { e.hitCd[slot] = 0.08f; });
        areaDamage(c, r, dmg, slot, 1.f, knock, -1, true);
        audioPlay(SFX_SLASH);
        return;
      }
      {
        float r = d.range * size;
        int sides = w.def == W_CORRUPTED ? 2 : 1;
        float hpK = w.def == W_CORRUPTED ? 1 + (1 - p.hp / p.st[S_MAXHP]) : 1.f;
        for (int sd = 0; sd < sides; sd++) {
          float yaw = w.angle + sd * PI;
          v3 dir(sinf(yaw), 0, cosf(yaw));
          slashFx(p.pos + v3(0, 1.0f, 0), yaw, r, d.color);
          std::vector<int> hits;
          forEnemiesNear(p.pos, r, [&](Enemy& e) { v3 to = e.pos - p.pos; to.y = 0; float l = len(to); if (l < 0.8f || dot(to / l, dir) > cosf(70 * DEG)) hits.push_back((int)(&e - enemies.data())); });
          for (int i : hits) hitEnemy(enemies[i], dmg * hpK, slot, 1.f, crit, p.pos, knock);
        }
        if (w.def == W_HEROSWORD) {
          Projectile& pr = addProj(); pr.kind = PK_WAVE; pr.model = M_P_WAVE; v3 dir(sinf(w.angle), 0, cosf(w.angle));
          pr.pos = origin; pr.vel = dir * d.speed * p.st[S_PROJSPD]; pr.life = pr.maxLife = dur; pr.dmg = dmg * 0.8f; pr.src = slot; pr.size = size * 1.4f; pr.radius = 1.0f * size; pr.pierce = 99; pr.knock = knock; pr.color = d.color;
        }
        audioPlay(SFX_SLASH);
      }
    } break;
    case WB_SPIN: {
      if (w.cd > 0) return;
      w.cd = cd;
      float r = d.range * size;
      bool big = w.charge >= 3; if (big) w.charge = 0;
      for (int k = 0; k < proj; k++) slashFx(p.pos + v3(0, 0.9f + k * 0.3f, 0), w.t * 3 + k * PI, r * (1 + 0.1f * k), big ? 0xffe060 : d.color, 0.3f);
      std::vector<int> hits; forEnemiesNear(p.pos, r, [&](Enemy& e) { hits.push_back((int)(&e - enemies.data())); });
      for (int i : hits) { Enemy& e = enemies[i]; if (e.elite || e.rank) w.charge++; hitEnemy(e, dmg * (big ? 5.f : 1.f) * (1 + 0.25f * (proj - 1)), slot, 1.f, crit, p.pos, knock); }
      audioPlay(SFX_SLASH);
    } break;
    case WB_AURA: {
      float r = d.range * size;
      if (w.cd > 0) return;
      w.cd = cd;
      std::vector<int> hits; forEnemiesNear(p.pos, r, [&](Enemy& e) { hits.push_back((int)(&e - enemies.data())); });
      for (int i : hits) {
        Enemy& e = enemies[i];
        if (w.def == W_FROSTWALKER) { e.slowT = std::max(e.slowT, 1.2f); if (rng.chance(0.15f)) e.freezeT = std::max(e.freezeT, 1.4f); }
        hitEnemy(e, dmg, slot, 0.5f, crit, p.pos, knock);
      }
      if (w.def == W_FROSTWALKER) particles(p.pos + v3(rng.range(-r, r), 0.3f, rng.range(-r, r)), 3, Col(180, 230, 255), 1.5f, 0.25f, 0.8f, true, SP_STAR, 1);
    } break;
    case WB_ORBIT: {
      // Chunkers: rocks orbit while active, then rest
      if (!w.active && w.cd <= 0) {
        w.active = true; w.charge = dur;
        for (int k = 0; k < proj; k++) {
          Projectile& pr = addProj(); pr.kind = PK_ROCK_ORBIT; pr.model = M_P_ROCK; pr.src = slot; pr.dmg = dmg; pr.size = size; pr.radius = 0.6f * size; pr.knock = knock; pr.crit = crit;
          pr.life = pr.maxLife = dur; pr.aux = k * TAU / proj; pr.aux2 = d.range * size; pr.speed = d.speed * p.st[S_PROJSPD]; pr.pierce = 999; pr.pos = p.pos;
        }
      }
      if (w.active) { w.charge -= dt; if (w.charge <= 0) { w.active = false; w.cd = cd; } }
    } break;
    case WB_SPIRAL: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        Projectile& pr = addProj(); pr.kind = PK_SPIRAL; pr.model = M_P_AXE; pr.src = slot; pr.dmg = dmg; pr.size = size; pr.radius = 0.7f * size; pr.knock = knock; pr.crit = crit;
        pr.life = pr.maxLife = dur; pr.aux = k * TAU / proj + rng.f(); pr.aux2 = d.range * size; pr.speed = 3.5f * p.st[S_PROJSPD]; pr.pierce = 999; pr.pos = p.pos; pr.spin = 12;
      }
      audioPlay(SFX_SLASH);
    } break;
    case WB_AEGIS: {
      int have = 0; for (auto& pr : projs) if (!pr.dead && pr.kind == PK_SHIELD && pr.src == slot) have++;
      for (int k = have; k < proj; k++) { Projectile& pr = addProj(); pr.kind = PK_SHIELD; pr.model = M_P_SHIELD; pr.src = slot; pr.dmg = dmg * 0.4f; pr.size = size; pr.radius = 0.8f * size; pr.knock = knock; pr.life = pr.maxLife = 1e9f; pr.aux2 = d.range * size; pr.speed = d.speed; pr.pierce = 999; pr.pos = p.pos; }
      int idx = 0; for (auto& pr : projs) if (!pr.dead && pr.kind == PK_SHIELD && pr.src == slot) { pr.aux = w.t * pr.speed + idx * TAU / std::max(1, proj); pr.dmg = dmg * 0.4f; pr.size = size; pr.aux2 = d.range * size; idx++; if (idx > proj) pr.dead = true; }
      if (w.cd > 0) return;
      w.cd = cd;
      float r = d.explode * size;
      Area& a = addArea(); a.kind = AK_SHOCKWAVE; a.pos = p.pos + v3(0, 0.4f, 0); a.radius = r; a.life = a.maxLife = 0.35f; a.color = 0x8fc4ff; a.target = 0;
      areaDamage(p.pos, r, dmg, slot, 1.f, knock, -1, true);
    } break;
    case WB_TRAIL: {
      w.trailT -= dt;
      if (w.trailT > 0) return;
      w.trailT = cd;
      if (len(v2(p.vel.x, p.vel.z)) < 0.5f && rng.chance(0.6f)) return;
      for (int k = 0; k < proj; k++) {
        float off = (k - (proj - 1) * 0.5f) * 1.4f * size;
        v3 side(cosf(p.yaw), 0, -sinf(p.yaw));
        Area& a = addArea(); a.kind = AK_FIRE; a.pos = p.pos + side * off; a.pos.y = world.T.height(a.pos.x, a.pos.z); a.radius = d.range * size; a.life = a.maxLife = dur; a.dmg = dmg; a.tick = 0.3f; a.src = slot; a.color = 0xff7a1a;
      }
    } break;
    case WB_MINES: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        v3 o = rng.inDisk(3.f); Area& a = addArea(); a.kind = AK_MINE; a.pos = p.pos + o; a.pos.y = world.T.height(a.pos.x, a.pos.z); a.radius = d.explode * size; a.life = a.maxLife = dur; a.dmg = dmg; a.src = slot; a.aux = 0.5f; a.knock = knock; a.color = 0xff4040;
      }
    } break;
    case WB_FLASK: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        int t = randomEnemy(p.pos, d.range); v3 tgt = t >= 0 ? enemies[t].pos : p.pos + rng.inDisk(6.f);
        Projectile& pr = addProj(); pr.kind = PK_LOB; pr.model = M_P_FLASK; pr.origin = origin; pr.pos = origin; pr.aux3 = tgt; pr.life = pr.maxLife = 0.7f; pr.dmg = dmg; pr.src = slot; pr.size = size; pr.explode = d.explode * size; pr.aux = dur; pr.spin = 8; pr.color = 0x8fff4f;
      }
    } break;
    case WB_BLACKHOLE: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        int t = randomEnemy(p.pos, d.range); v3 tgt = t >= 0 ? enemies[t].pos : p.pos + v3(sinf(p.yaw), 0, cosf(p.yaw)) * 8.f;
        Area& a = addArea(); a.kind = AK_BLACKHOLE; a.pos = tgt + v3(0, 1.2f, 0); a.radius = d.explode * size; a.life = a.maxLife = dur; a.dmg = dmg; a.tick = 0.25f; a.src = slot; a.pull = 7.f; a.color = 0x8a4fff;
      }
      audioPlay(SFX_ZAP);
    } break;
    case WB_TORNADO: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        float a0 = rng.f() * TAU; Area& a = addArea(); a.kind = AK_TORNADO; a.pos = p.pos + v3(cosf(a0), 0, sinf(a0)) * 2.f; a.vel = v3(cosf(a0), 0, sinf(a0)) * pspd; a.radius = 1.6f * size; a.life = a.maxLife = dur; a.dmg = dmg; a.tick = 0.3f; a.src = slot; a.knock = knock; a.color = 0xdfe6ee;
      }
    } break;
    case WB_BOOMERANG: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        v3 dir = aimDir(d.range); float a = (k - (proj - 1) * 0.5f) * 0.35f; dir = norm(v3(dir.x * cosf(a) - dir.z * sinf(a), 0, dir.x * sinf(a) + dir.z * cosf(a)));
        Projectile& pr = addProj(); pr.kind = PK_BOOMERANG; pr.model = M_P_BANANA; pr.pos = origin; pr.vel = dir * pspd; pr.speed = pspd; pr.range = d.range * (1 + w.g[U_PROJSPD]); pr.life = pr.maxLife = 6; pr.dmg = dmg; pr.src = slot; pr.size = size; pr.radius = 0.6f * size; pr.pierce = 999; pr.spin = 16; pr.knock = knock; pr.crit = crit;
      }
    } break;
    case WB_LIGHTNING: {
      if (w.cd > 0) return;
      w.cd = cd;
      for (int k = 0; k < proj; k++) {
        int t = randomEnemy(p.pos, d.range); if (t < 0) break;
        v3 at = enemies[t].pos; int lastId = enemies[t].id;
        lightning(at, dmg, slot, d.explode * size, 0.3f);
        for (int b = 0; b < bounce; b++) {
          int n = nearestEnemy(at, 6, lastId); if (n < 0) break;
          Area& a = addArea(); a.kind = AK_LIGHT; a.pos = enemies[n].pos; a.vel = at; a.radius = 0.8f; a.life = a.maxLife = 0.2f; a.color = 0xbfe8ff; a.aux = 2;
          at = enemies[n].pos; lastId = enemies[n].id;
          hitEnemy(enemies[n], dmg * 0.7f, slot, 0.5f, crit, at, 0.2f);
        }
      }
    } break;
    case WB_BEAM: {
      // tether to target, damage along the segment
      Enemy* tgt = nullptr;
      if (w.target >= 0 && w.target < (int)enemies.size() && enemies[w.target].alive && distXZ(enemies[w.target].pos, p.pos) < d.range + 2) tgt = &enemies[w.target];
      if (!tgt) { w.target = nearestEnemy(p.pos, d.range); tgt = w.target >= 0 ? &enemies[w.target] : nullptr; }
      w.angle = tgt ? 1.f : 0.f;
      if (!tgt || w.cd > 0) return;
      w.cd = cd;
      v3 a = origin, b = tgt->pos + v3(0, 0.8f * tgt->scale, 0); v3 ab = b - a; float L2 = std::max(0.01f, dot(ab, ab));
      float wdt = 0.6f * size + 0.3f;
      std::vector<int> hits;
      forEnemiesNear((a + b) * 0.5f, sqrtf(L2) * 0.5f + wdt, [&](Enemy& e) { v3 pe = e.pos + v3(0, 0.6f, 0); float t = clampf(dot(pe - a, ab) / L2, 0, 1); if (len(pe - (a + ab * t)) < wdt + e.radius) hits.push_back((int)(&e - enemies.data())); });
      for (int i : hits) hitEnemy(enemies[i], dmg, slot, 0.35f, crit, a, 0);
    } break;
    case WB_CONE: {
      if (w.cd > 0 && w.burst <= 0) return;
      if (w.burst <= 0) { w.cd = cd; w.burst = 4; w.burstT = 0; v3 ad = aimDir(d.range * size + 2); w.angle = atan2f(ad.x, ad.z); }
      w.burstT -= dt;
      if (w.burstT > 0) return;
      w.burstT = dur / 4; w.burst--;
      float r = d.range * size; v3 dir(sinf(w.angle), 0, cosf(w.angle));
      Area& fx = addArea(); fx.kind = AK_SHOCKWAVE; fx.pos = origin; fx.radius = r; fx.life = fx.maxLife = 0.25f; fx.color = 0xff6a1a; fx.aux = w.angle; fx.target = 2;
      for (int k = 0; k < 6; k++) { Particle q; q.pos = origin + dir * 0.6f; float sp = rng.range(0.6f, 1.f) * r * 2.2f; float aa = w.angle + rng.range(-0.45f, 0.45f); q.vel = v3(sinf(aa) * sp, rng.range(-0.5f, 1.5f), cosf(aa) * sp); q.life = q.maxLife = 0.45f; q.size = 0.5f; q.size1 = 1.6f; q.c = rng.chance(0.5f) ? Col(255, 120, 30) : Col(255, 200, 60); q.sprite = SP_FLAME; q.add = true; parts.push_back(q); }
      std::vector<int> hits;
      forEnemiesNear(p.pos, r, [&](Enemy& e) { v3 to = e.pos - p.pos; to.y = 0; float l = len(to); if (l < 0.5f || dot(to / l, dir) > cosf(30 * DEG)) hits.push_back((int)(&e - enemies.data())); });
      for (int i : hits) { hitEnemy(enemies[i], dmg, slot, 0.5f, crit, p.pos, knock); enemies[i].burnDps = std::max(enemies[i].burnDps, dmg * 0.2f); enemies[i].burnT = 2; }
      audioPlay(SFX_FIRE);
    } break;
  }
}

// ====================================================================== projectiles
void Game::updateProjectiles(float dt) {
  for (size_t pi = 0; pi < projs.size(); pi++) {
    Projectile& pr = projs[pi];
    if (pr.dead) continue;
    pr.t += dt; pr.life -= dt;
    if (pr.life <= 0) { if (pr.kind == PK_LOB) { pr.life = 0.0001f; } else { pr.dead = true; if (pr.explode > 0 && !pr.enemy) { areaDamage(pr.pos, pr.explode, pr.dmg, pr.src, 0.5f, pr.knock); Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = pr.pos; a.radius = pr.explode; a.life = a.maxLife = 0.3f; a.color = pr.color; } continue; } }
    auto resolveTarget = [&]() -> Enemy* {
      if (pr.target >= 0 && pr.target < (int)enemies.size() && enemies[pr.target].alive && (int)pr.aux2 == enemies[pr.target].id) return &enemies[pr.target];
      int t = nearestEnemy(pr.pos, 22); if (t < 0) { pr.target = -1; return nullptr; }
      pr.target = t; pr.aux2 = (float)enemies[t].id; return &enemies[t];
    };
    switch (pr.kind) {
      case PK_HOMING: case PK_SOUL: {
        Enemy* e = resolveTarget();
        if (e) { v3 want = norm(e->pos + v3(0, 0.8f * e->scale, 0) - pr.pos) * pr.speed; pr.vel = lerp3(pr.vel, want, std::min(1.f, dt * pr.homing)); }
        float l = len(pr.vel); if (l > 0.01f) pr.vel = pr.vel / l * std::max(l, pr.speed * 0.6f);
        pr.pos += pr.vel * dt;
      } break;
      case PK_ROCK_ORBIT: case PK_SHIELD: {
        if (pr.kind == PK_ROCK_ORBIT) pr.aux += pr.speed * dt;
        pr.pos = P.pos + v3(cosf(pr.aux) * pr.aux2, 1.0f + (pr.kind == PK_SHIELD ? 0.1f : sinf(pr.t * 3 + pr.aux) * 0.2f), sinf(pr.aux) * pr.aux2);
        if (pr.kind == PK_SHIELD) {
          for (auto& q : projs) if (!q.dead && q.enemy && len2(q.pos - pr.pos) < sq(pr.radius + 0.6f)) { q.dead = true; particles(q.pos, 6, Col(150, 200, 255), 3, 0.25f, 0.3f); }
        }
      } break;
      case PK_SPIRAL: {
        float k = pr.t / pr.maxLife;
        float r = 1.2f + (pr.aux2 - 1.2f) * sinf(std::min(1.f, k) * PI);
        pr.aux += pr.speed * dt;
        pr.pos = P.pos + v3(cosf(pr.aux) * r, 1.0f, sinf(pr.aux) * r);
      } break;
      case PK_BOOMERANG: {
        float out = len(pr.pos - P.pos);
        if (pr.aux == 0 && (out > pr.range || pr.t > pr.range / pr.speed)) { pr.aux = 1; pr.nhits = 0; }
        if (pr.aux == 1) { v3 want = norm(P.pos + v3(0, 1, 0) - pr.pos) * pr.speed * 1.2f; pr.vel = lerp3(pr.vel, want, std::min(1.f, dt * 5)); if (out < 1.2f) { pr.dead = true; continue; } }
        pr.pos += pr.vel * dt;
      } break;
      case PK_LOB: case PK_ENEMY_LOB: {
        float k = clampf(pr.t / pr.maxLife, 0, 1);
        v3 flat = lerp3(pr.origin, pr.aux3, k);
        pr.pos = flat + v3(0, sinf(k * PI) * 4.f, 0);
        if (k >= 1) {
          pr.dead = true;
          if (pr.kind == PK_LOB) { Area& a = addArea(); a.kind = AK_POISON; a.pos = pr.aux3; a.pos.y = world.T.height(a.pos.x, a.pos.z); a.radius = pr.explode; a.life = a.maxLife = pr.aux; a.dmg = pr.dmg; a.tick = 0.5f; a.src = pr.src; a.color = pr.color; particles(a.pos + v3(0, 0.3f, 0), 10, Col(140, 255, 90), 4, 0.3f, 0.5f); audioPlay(SFX_BREAK); }
          else { Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = pr.aux3; a.radius = pr.explode; a.life = a.maxLife = 0.3f; a.color = pr.color; if (distXZ(P.pos, pr.aux3) < pr.explode + 0.4f) damagePlayer(pr.dmg, nullptr); }
        }
        continue;
      }
      default: pr.pos += pr.vel * dt; break;
    }
    if (pr.enemy) {
      v3 pp = P.pos + v3(0, 1, 0);
      if (len2(pr.pos - pp) < sq(pr.radius + 0.5f)) { damagePlayer(pr.dmg, nullptr); if (pr.slow > 0) P.cowardT = 0; pr.dead = true; }
      if (pr.pos.y < world.T.height(pr.pos.x, pr.pos.z) - 0.2f) pr.dead = true;
      continue;
    }
    // ground hit
    if (pr.kind == PK_STRAIGHT || pr.kind == PK_BOUNCE || pr.kind == PK_HOMING) {
      float g = world.T.height(pr.pos.x, pr.pos.z);
      if (pr.pos.y < g + 0.05f) {
        if (pr.kind == PK_HOMING && pr.pos.y > g - 1.5f) pr.pos.y = g + 0.05f;
        else { pr.dead = true; if (pr.explode > 0) { areaDamage(pr.pos, pr.explode, pr.dmg, pr.src, 0.5f, pr.knock); Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = pr.pos; a.radius = pr.explode; a.life = a.maxLife = 0.3f; a.color = pr.color; } continue; }
      }
    }
    // enemy hits
    bool persistent = pr.kind == PK_ROCK_ORBIT || pr.kind == PK_SPIRAL || pr.kind == PK_SHIELD || pr.kind == PK_BOOMERANG;
    std::vector<int> cands;
    forEnemiesNear(pr.pos, pr.radius, [&](Enemy& e) { cands.push_back((int)(&e - enemies.data())); });
    for (int ei : cands) {
      Enemy& e = enemies[ei];
      if (!e.alive || pr.dead) continue;
      if (fabsf((e.pos.y + 0.8f * e.scale) - pr.pos.y) > 1.2f * e.scale + pr.radius + 0.6f) continue;
      int slot = pr.src >= 0 && pr.src < 8 ? pr.src : 7;
      if (persistent) { if (e.hitCd[slot] > 0) continue; e.hitCd[slot] = pr.kind == PK_BOOMERANG ? 0.3f : 0.45f; }
      else { bool seen = false; for (int h = 0; h < pr.nhits; h++) if (pr.hits[h] == e.id) seen = true; if (seen) continue; if (pr.nhits < 10) pr.hits[pr.nhits++] = e.id; }
      if (pr.src < 8 && pr.src < (int)P.weapons.size() && P.weapons[pr.src].def == W_DEXECUTIONER && e.rank == RK_NORMAL && rng.chance(0.02f)) { e.hp = 0; floatText(e.pos + v3(0, 2, 0), tr("EXECUTED"), Col(255, 60, 60), 1.0f); killEnemy(e, pr.src); }
      else hitEnemy(e, pr.dmg, pr.src, pr.procCoef, pr.crit, pr.pos - pr.vel * 0.05f, pr.knock);
      if (pr.explode > 0 && !persistent) {
        areaDamage(pr.pos, pr.explode, pr.dmg * 0.8f, pr.src, 0.3f, pr.knock, e.id);
        Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = pr.pos; a.radius = pr.explode; a.life = a.maxLife = 0.3f; a.color = pr.color;
        pr.dead = true; audioPlay(SFX_EXPLODE); break;
      }
      if (persistent) continue;
      if (pr.kind == PK_BOUNCE && pr.bounces > 0) {
        pr.bounces--;
        int n = -1; float bd = 64;
        for (int i = 0; i < (int)enemies.size(); i++) { const Enemy& o = enemies[i]; if (!o.alive || o.id == e.id) continue; bool seen = false; for (int h = 0; h < pr.nhits; h++) if (pr.hits[h] == o.id) seen = true; if (seen) continue; float dd = len2(o.pos - pr.pos); if (dd < bd) { bd = dd; n = i; } }
        if (n >= 0) { float sp = len(pr.vel); pr.vel = norm(enemies[n].pos + v3(0, 0.8f, 0) - pr.pos) * sp; pr.life = pr.maxLife; continue; }
        pr.dead = true; break;
      }
      if (pr.pierce > 0) { pr.pierce--; continue; }
      pr.dead = true; break;
    }
  }
}

// ====================================================================== areas
void Game::updateAreas(float dt) {
  for (size_t ai = 0; ai < areas.size(); ai++) {
    Area& a = areas[ai];
    if (a.dead) continue;
    if (a.delay > 0) { a.delay -= dt; continue; }
    a.life -= dt;
    if (a.life <= 0) {
      a.dead = true;
      if (a.kind == AK_TELEGRAPH) {
        // resolve telegraphed boss attack
        if (distXZ(P.pos, a.pos) < a.radius + 0.3f && P.pos.y - a.pos.y < 3.5f) damagePlayer(a.dmg, nullptr);
        Area& x = addArea(); x.kind = AK_EXPLOSION; x.pos = a.pos + v3(0, 0.3f, 0); x.radius = a.radius; x.life = x.maxLife = 0.35f; x.color = a.color;
        particles(a.pos + v3(0, 0.3f, 0), 12, hexc(a.color), 6, 0.4f, 0.5f, false, SP_WHITE, -10);
        shake = std::max(shake, 0.15f);
      }
      continue;
    }
    a.tickT -= dt;
    switch (a.kind) {
      case AK_FIRE: case AK_POISON: case AK_DRAGONFIRE: case AK_FROST:
        if (a.tickT <= 0) {
          a.tickT = a.tick;
          std::vector<int> hits; forEnemiesNear(a.pos, a.radius, [&](Enemy& e) { if (!e.flying || a.kind == AK_DRAGONFIRE) hits.push_back((int)(&e - enemies.data())); });
          for (int i : hits) {
            Enemy& e = enemies[i];
            if (a.kind == AK_POISON) { e.poisonDps += a.dmg * 1.5f; e.poisonT = 3; }
            hitEnemy(e, a.dmg, a.src, 0.25f, 0, a.pos, 0, true);
          }
        }
        break;
      case AK_BLACKHOLE: {
        std::vector<int> hits; forEnemiesNear(a.pos, a.radius, [&](Enemy& e) { hits.push_back((int)(&e - enemies.data())); });
        for (int i : hits) { Enemy& e = enemies[i]; if (e.rank == RK_NORMAL) { v3 d = a.pos - e.pos; d.y = 0; float l = len(d); if (l > 0.3f) e.pos += d / l * std::min(l, a.pull * dt); } }
        if (a.tickT <= 0) { a.tickT = a.tick; for (int i : hits) if (enemies[i].alive) hitEnemy(enemies[i], a.dmg, a.src, 0.2f, 0, a.pos, 0); }
      } break;
      case AK_TORNADO: {
        a.vel += v3(rng.range(-1, 1), 0, rng.range(-1, 1)) * 8.f * dt;
        float l = len(a.vel); if (l > 7) a.vel = a.vel / l * 7;
        a.pos += a.vel * dt; a.pos.y = world.T.height(a.pos.x, a.pos.z);
        if (a.tickT <= 0) {
          a.tickT = a.tick;
          std::vector<int> hits; forEnemiesNear(a.pos, a.radius, [&](Enemy& e) { hits.push_back((int)(&e - enemies.data())); });
          for (int i : hits) hitEnemy(enemies[i], a.dmg, a.src, 0.3f, 0, a.pos, a.knock);
        }
      } break;
      case AK_MINE: {
        if (a.aux > 0) { a.aux -= dt; break; }
        bool trig = false; forEnemiesNear(a.pos, 1.0f, [&](Enemy& e) { if (!e.flying) trig = true; });
        if (trig) {
          a.dead = true; areaDamage(a.pos, a.radius, a.dmg, a.src, 1.f, a.knock);
          Area& x = addArea(); x.kind = AK_EXPLOSION; x.pos = a.pos + v3(0, 0.4f, 0); x.radius = a.radius; x.life = x.maxLife = 0.35f; x.color = 0xff6030;
          audioPlay(SFX_EXPLODE);
        }
      } break;
      case AK_BOSSFIRE: {
        // rotating laser beams from boss position (aux = angle, aux2 via tick = rot speed)
        a.aux += a.tick * dt;
        int beams = (int)a.knock;
        if (a.target >= 0) { for (auto& e : enemies) if (e.id == a.target && e.alive) { a.pos = e.pos + v3(0, 1.5f, 0); } }
        for (int b = 0; b < beams; b++) {
          float ang = a.aux + b * TAU / beams; v3 dir(sinf(ang), 0, cosf(ang));
          v3 rel = P.pos - a.pos; rel.y = 0; float along = dot(rel, dir);
          if (along > 0 && along < a.radius) { float perp = len(rel - dir * along); if (perp < 0.9f && a.tickT <= 0) { damagePlayer(a.dmg, nullptr); a.tickT = 0.5f; } }
        }
      } break;
      case AK_HEALZONE: break;
      default: break;
    }
  }
}

// ====================================================================== enemies
void Game::updateEnemies(float dt) {
  float H = world.playableHalf();
  for (size_t ei = 0; ei < enemies.size(); ei++) {
    Enemy& e = enemies[ei];
    if (!e.alive) { e.deathT += dt; continue; }
    const EnemyDef& d = gEnemies[e.type];
    e.spawnT += dt;
    if (e.hitFlash > 0) e.hitFlash -= dt;
    for (int k = 0; k < 8; k++) if (e.hitCd[k] > 0) e.hitCd[k] -= dt;
    if (e.atkCd > 0) e.atkCd -= dt;
    // DoTs every 0.5s
    e.bleedT -= dt;
    if (e.bleedT <= 0) {
      e.bleedT = 0.5f;
      float dot = 0;
      if (e.poisonT > 0) { dot += e.poisonDps * 0.5f; e.poisonT -= 0.5f; if (e.poisonT <= 0) e.poisonDps = 0; }
      if (e.burnT > 0) { dot += e.burnDps * 0.5f; e.burnT -= 0.5f; }
      if (e.curseT > 0) { dot += e.curseDps * 0.5f; e.curseT -= 0.5f; }
      if (e.bleed > 0) dot += e.bleed * (1.5f + 1.5f * stage) * P.st[S_DAMAGE] * 0.5f;
      if (dot > 0) {
        int src = e.curseT > 0 ? SRC_ITEM + IT_CURSEDDOLL : e.bleed > 0 ? SRC_ITEM + IT_UNSTABLETRANSFUSION : SRC_ITEM + IT_MOLDYCHEESE;
        float before = e.hp; e.hp -= dot; dmgBySrc[src] += std::min(dot, std::max(0.f, before));
        if (dmgNumbers && rng.chance(0.3f)) floatText(e.pos + v3(0, 1.6f * e.scale, 0), fmtShort(dot), e.poisonT > 0 ? Col(140, 255, 100) : e.curseT > 0 ? Col(190, 110, 255) : Col(255, 90, 90), 0.8f);
        if (e.hp <= 0) { killEnemy(e, src); continue; }
      }
    }
    if (e.spawnT < 0.35f) { if (!e.flying) e.pos.y = world.T.height(e.pos.x, e.pos.z); continue; }
    if (e.freezeT > 0) { e.freezeT -= dt; continue; }
    if (e.stunT > 0) { e.stunT -= dt; continue; }
    if (e.airT > 0) { e.airT -= dt; e.pos.y = world.T.height(e.pos.x, e.pos.z) + sinf(sat(1 - e.airT) * PI) * 3.f; continue; }
    float spdMul = e.slowT > 0 ? 0.5f : 1.f; if (e.slowT > 0) e.slowT -= dt;
    v3 to = P.pos - e.pos; to.y = 0; float dist = len(to); v3 dir = dist > 0.01f ? to / dist : v3(0, 0, 1);
    // far away: recycle near the player
    if (dist > 75 && e.rank == RK_NORMAL && !e.challenge) { v3 np = spawnRingPos(26, 36); e.pos = np; if (e.flying) e.pos.y += e.hover; continue; }
    e.animT += dt;
    if (e.clip == CL_ATTACK) { e.clipT += dt; if (e.clipT > clipDuration(d.model, CL_ATTACK)) { e.clip = CL_MOVE; e.clipT = 0; } }
    if (e.rank == RK_BOSS) { bossAI(e, dt); }
    else if (e.rank == RK_MINIBOSS) { minibossAI(e, dt); }
    else {
      v3 mv = dir * e.speed * spdMul;
      switch (d.beh) {
        case EB_EXPLODE:
          if (e.aiState == 0 && dist < 2.4f) { e.aiState = 1; e.aiT = 0.8f; e.clip = CL_ATTACK; e.clipT = 0; }
          if (e.aiState == 1) {
            mv = v3(); e.aiT -= dt; e.hitFlash = fmodf(e.aiT, 0.2f) < 0.1f ? 0.08f : 0;
            if (e.aiT <= 0) {
              if (dist < 3.2f) damagePlayer(e.dmg, &e);
              Area& x = addArea(); x.kind = AK_EXPLOSION; x.pos = e.pos + v3(0, 0.8f, 0); x.radius = 3.2f; x.life = x.maxLife = 0.35f; x.color = 0xa0ff60;
              particles(e.pos + v3(0, 0.8f, 0), 20, Col(170, 230, 90), 7, 0.4f, 0.5f, false, SP_WHITE, -10);
              audioPlay(SFX_EXPLODE); killEnemy(e, -1); continue;
            }
          }
          break;
        case EB_SHOOT:
          if (dist < 13) { mv = mv * (dist < 9 ? -0.3f : 0.f); e.aiT -= dt; if (e.aiT <= 0) { e.aiT = 2.6f; e.clip = CL_ATTACK; e.clipT = 0;
              Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY; pr.model = M_P_SPIKE; pr.pos = e.pos + v3(0, 1.2f, 0); v3 aim = P.pos + v3(0, 1, 0) + P.vel * (dist / 10.f) * 0.5f - pr.pos; pr.vel = norm(aim) * 10.f; pr.life = pr.maxLife = 3; pr.dmg = e.dmg; pr.radius = 0.35f; } }
          break;
        case EB_ROLL: {
          v3 fwd = len(e.vel) > 0.1f ? norm(e.vel) : dir;
          fwd = norm(lerp3(fwd, dir, std::min(1.f, dt * 1.2f)));
          mv = fwd * e.speed * spdMul;
        } break;
        default: break;
      }
      if (d.beh != EB_EXPLODE || e.aiState == 0) {
        if (dist < e.radius + 0.55f && fabsf(P.pos.y - e.pos.y) < 1.8f + (e.flying ? 1.5f : 0)) {
          if (e.atkCd <= 0) { e.atkCd = 1.0f; e.clip = CL_ATTACK; e.clipT = 0; damagePlayer(e.dmg, &e); if (d.beh == EB_POISON) { P.regenAcc -= 0; } }
          mv = mv * 0.1f;
        }
      }
      e.vel = mv;
    }
    // separation
    {
      int cx = clampi((int)((e.pos.x - egOrigin) / egCell), 0, egN - 1), cz = clampi((int)((e.pos.z - egOrigin) / egCell), 0, egN - 1);
      int checks = 0;
      for (int i = egHead[cz * egN + cx]; i >= 0 && checks < 8; i = egNext[i]) {
        if (i == (int)ei || i >= (int)enemies.size()) continue;
        Enemy& o = enemies[i]; if (!o.alive) continue; checks++;
        v3 dd = e.pos - o.pos; dd.y = 0; float l2 = len2(dd); float rr = e.radius + o.radius;
        if (l2 < rr * rr && l2 > 1e-6f) { float l = sqrtf(l2); v3 push = dd / l * (rr - l) * 0.5f; float wE = e.rank ? 0.1f : 1.f; e.pos += push * wE; }
      }
    }
    e.pos += (e.vel + e.knock) * dt;
    e.knock *= std::max(0.f, 1 - dt * 6);
    e.pos.x = clampf(e.pos.x, -H - 4, H + 4); e.pos.z = clampf(e.pos.z, -H - 4, H + 4);
    if (!e.flying) {
      if (dist < 40 && e.rank == RK_NORMAL) world.collide(e.pos, e.radius * 0.8f, e.pos.y);
      e.pos.y = world.T.height(e.pos.x, e.pos.z);
    } else {
      float g = world.T.height(e.pos.x, e.pos.z);
      float want = std::max(g + e.hover, P.pos.y + 0.9f * (dist < 6 ? 1 : 0) + (dist < 6 ? 0.2f : e.hover * 0.5f));
      e.pos.y = lerpf(e.pos.y, want + sinf(e.animT * 2 + e.animPhase) * 0.2f, std::min(1.f, dt * 3));
    }
    if (len(e.vel) > 0.1f) e.yaw = angleLerp(e.yaw, atan2f(e.vel.x, e.vel.z), std::min(1.f, dt * 8));
  }
}

void Game::minibossAI(Enemy& e, float dt) {
  const EnemyDef& d = gEnemies[e.type];
  v3 to = P.pos - e.pos; to.y = 0; float dist = len(to); v3 dir = dist > 0.01f ? to / dist : v3(0, 0, 1);
  e.aiT -= dt;
  v3 mv = dir * e.speed;
  switch (d.beh) {
    case EB_MB_SLAM:
      if (e.aiT <= 0 && dist < 9) { e.aiT = 4.f; e.clip = CL_ATTACK; e.clipT = 0; Area& a = addArea(); a.kind = AK_TELEGRAPH; a.pos = e.pos; a.radius = 4.5f; a.life = a.maxLife = 1.0f; a.dmg = e.dmg * 1.3f; a.color = 0xff3030; }
      break;
    case EB_MB_CHARGE:
      if (e.aiState == 0 && e.aiT <= 0 && dist < 18) { e.aiState = 1; e.aiT = 0.9f; e.aiVec = dir; Area& a = addArea(); a.kind = AK_TELEGRAPH; a.pos = e.pos + dir * 6; a.radius = 2.f; a.life = a.maxLife = 0.9f; a.dmg = 0; a.color = 0xff3030; }
      if (e.aiState == 1) { mv = v3(); if (e.aiT <= 0) { e.aiState = 2; e.aiT = 1.1f; } }
      else if (e.aiState == 2) { mv = e.aiVec * e.speed * 3.2f; e.clip = CL_SPECIAL; if (dist < e.radius + 1 && e.atkCd <= 0) { damagePlayer(e.dmg * 1.4f, &e); e.atkCd = 1; } if (e.aiT <= 0) { e.aiState = 0; e.aiT = 4.f; e.clip = CL_MOVE; } }
      break;
    case EB_MB_THROW:
      if (e.aiT <= 0 && dist < 22) { e.aiT = 2.8f; e.clip = CL_ATTACK; e.clipT = 0;
        for (int k = 0; k < 3; k++) { Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY_LOB; pr.model = M_P_ROCK; pr.origin = e.pos + v3(0, 3, 0); pr.pos = pr.origin; pr.aux3 = P.pos + v3(rng.range(-3, 3), 0, rng.range(-3, 3)) + P.vel * 0.5f; pr.aux3.y = world.T.height(pr.aux3.x, pr.aux3.z); pr.life = pr.maxLife = 1.1f; pr.dmg = e.dmg; pr.explode = 2.2f; pr.color = 0xd0a050; pr.size = 1.6f;
          Area& a = addArea(); a.kind = AK_TELEGRAPH; a.pos = pr.aux3; a.radius = 2.2f; a.life = a.maxLife = 1.1f; a.dmg = 0; a.color = 0xffa040; }
      }
      break;
    case EB_MB_SPIT:
      if (e.aiT <= 0 && dist < 16) { e.aiT = 3.f; e.clip = CL_ATTACK; e.clipT = 0;
        for (int k = -2; k <= 2; k++) { float a = atan2f(dir.x, dir.z) + k * 0.22f; Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY; pr.model = M_P_ENEMYBALL; pr.pos = e.pos + v3(0, 2.5f, 0); pr.vel = v3(sinf(a), -0.1f, cosf(a)) * 11.f; pr.life = pr.maxLife = 2.5f; pr.dmg = e.dmg * 0.7f; pr.radius = 0.45f; }
      }
      break;
    case EB_MB_RING:
      if (e.aiT <= 0) { e.aiT = 3.5f; e.clip = CL_ATTACK; e.clipT = 0;
        int n = 14; for (int k = 0; k < n; k++) { float a = k * TAU / n + rng.f(); Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY; pr.model = M_P_ENEMYBALL; pr.pos = e.pos + v3(0, 0.5f, 0); pr.vel = v3(sinf(a), 0, cosf(a)) * 8.f; pr.life = pr.maxLife = 4; pr.dmg = e.dmg * 0.6f; pr.radius = 0.45f; }
      }
      break;
    case EB_MB_SUMMON:
      if (e.aiT <= 0) { e.aiT = 6.f; e.clip = CL_ATTACK; e.clipT = 0; for (int k = 0; k < 4; k++) { v3 p = e.pos + v3(cosf(k * 1.57f) * 3, 0, sinf(k * 1.57f) * 3); spawnEnemy(E_SKELETON, p); } return; }
      break;
  }
  if (dist < e.radius + 0.8f && e.atkCd <= 0 && e.aiState != 2) { e.atkCd = 1.2f; damagePlayer(e.dmg, &e); e.clip = CL_ATTACK; e.clipT = 0; }
  e.vel = mv;
}

void Game::bossAI(Enemy& e, float dt) {
  v3 to = P.pos - e.pos; to.y = 0; float dist = len(to); v3 dir = dist > 0.01f ? to / dist : v3(0, 0, 1);
  e.aiT -= dt; e.aiT2 -= dt;
  float hpf = e.hp / e.maxHp;
  v3 mv = dir * e.speed * (dist > 4 ? 1.f : 0.2f);
  auto ring = [&](int n, float spd, float dm, int model = M_P_ENEMYBALL, float y = 1.5f) {
    for (int k = 0; k < n; k++) { float a = k * TAU / n + rng.f() * 0.3f; Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY; pr.model = model; pr.pos = e.pos + v3(0, y, 0); pr.vel = v3(sinf(a), 0, cosf(a)) * spd; pr.life = pr.maxLife = 5; pr.dmg = e.dmg * dm; pr.radius = 0.5f; }
  };
  auto tele = [&](v3 p, float r, float delay, float dm, u32 col) { Area& a = addArea(); a.kind = AK_TELEGRAPH; a.pos = p; a.pos.y = world.T.height(p.x, p.z); a.radius = r; a.life = a.maxLife = delay; a.dmg = e.dmg * dm; a.color = col; };
  auto summon = [&](int type, int n) { for (int k = 0; k < n; k++) { v3 p = e.pos + v3(cosf(k * TAU / n) * 4, 0, sinf(k * TAU / n) * 4); p.y = world.T.height(p.x, p.z); spawnEnemy(type, p); } };
  int t = e.type;
  if (t == E_B_GHOSTKING) {
    // phases: invulnerable at 66% / 33% until 2 altars are captured
    int phase = hpf > 0.66f ? 0 : hpf > 0.33f ? 1 : 2;
    if ((int)e.phase < phase) {
      e.phase = (float)phase; e.invuln = true; int c = 0;
      for (auto& it : inters) if (it.type == I_ALTAR && it.used && c < 2) { it.used = false; it.progress = 0; c++; }
      alert(tr("THE BOSS IS IMMUNE"), tr("Capture 2 altars to break the shield"), Col(255, 120, 120), 2.6f);
    }
    if (e.invuln) { int open = 0; for (auto& it : inters) if (it.type == I_ALTAR && !it.used) open++; if (open == 0) { e.invuln = false; alert(tr("SHIELD BROKEN!"), "", Col(120, 255, 140)); } }
    mv = dir * e.speed * (dist > 8 ? 1.f : -0.3f);
    if (e.aiT <= 0) { e.aiT = 3.0f - phase * 0.6f; ring(14 + phase * 6, 7 + phase, 0.6f); e.clip = CL_ATTACK; e.clipT = 0; }
    if (e.aiT2 <= 0) { e.aiT2 = 6.f; for (int k = 0; k < 3 + phase; k++) tele(P.pos + rng.inDisk(4.f), 3.f, 1.2f, 1.2f, 0xc060ff); summon(E_SWARMGHOST, 3 + phase * 2); }
    e.pos.y = world.T.height(e.pos.x, e.pos.z) + 2.0f + sinf(e.animT) * 0.4f;
    e.vel = mv; return;
  }
  switch (t) {
    case E_B_LILBARK:
      mv = v3();
      if (e.aiT <= 0) { e.aiT = 3.f; ring(10, 7, 0.5f, M_P_ROCK, 2.5f); e.clip = CL_ATTACK; e.clipT = 0; }
      if (e.aiT2 <= 0) { e.aiT2 = 6.5f; for (int k = 0; k < 3; k++) tele(P.pos + rng.inDisk(3.5f), 2.4f, 1.0f, 1.0f, 0xff4030); summon(rng.chance(0.5f) ? E_SLIME : E_GOBLIN, 3); }
      break;
    case E_B_CHADBARK: case E_B_BARKVADER:
      if (e.aiT <= 0 && dist < 10) { e.aiT = t == E_B_BARKVADER ? 3.2f : 4.f; tele(e.pos, 5.5f, 1.1f, 1.4f, 0xff3030); e.clip = CL_SPECIAL; e.clipT = 0; }
      if (e.aiT2 <= 0) { e.aiT2 = 5.f; ring(t == E_B_BARKVADER ? 18 : 12, 8, 0.5f, M_P_ROCK, 2.5f); if (t == E_B_BARKVADER) { Area& a = addArea(); a.kind = AK_BOSSFIRE; a.pos = e.pos + v3(0, 1.5f, 0); a.radius = 14; a.life = a.maxLife = 3.5f; a.tick = 1.6f; a.knock = 2; a.dmg = e.dmg * 0.6f; a.target = e.id; a.color = 0xff2020; } }
      break;
    case E_B_LILANUBIS: case E_B_ANUBRUH: case E_B_JUGEANUBIS: {
      if (e.aiT <= 0) {
        e.aiT = 2.4f;
        for (int k = -1; k <= 1; k++) { float a = atan2f(dir.x, dir.z) + k * 0.25f; Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY; pr.model = M_P_ENEMYBALL; pr.pos = e.pos + v3(0, 2.5f, 0); pr.vel = v3(sinf(a), -0.05f, cosf(a)) * 12.f; pr.life = pr.maxLife = 3; pr.dmg = e.dmg * 0.6f; pr.radius = 0.5f; }
        e.clip = CL_ATTACK; e.clipT = 0;
      }
      if (e.aiT2 <= 0) {
        e.aiT2 = 7.f;
        if (t != E_B_LILANUBIS) { Area& a = addArea(); a.kind = AK_BOSSFIRE; a.pos = e.pos + v3(0, 1.5f, 0); a.radius = 16; a.life = a.maxLife = 4.5f; a.tick = t == E_B_JUGEANUBIS ? 1.4f : 1.0f; a.knock = t == E_B_JUGEANUBIS ? 4.f : 2.f; a.dmg = e.dmg * 0.5f; a.target = e.id; a.color = 0x3080ff; }
        if (t == E_B_JUGEANUBIS) for (int k = 0; k < 4; k++) tele(P.pos + rng.inDisk(5.f), 3.f, 1.4f, 1.3f, 0xffd040);
        summon(E_MUMMY, 3);
        if (dist > 14) { e.pos = P.pos - dir * 9.f; e.pos.y = world.T.height(e.pos.x, e.pos.z); particles(e.pos + v3(0, 1, 0), 20, Col(255, 210, 80), 6, 0.4f, 0.6f); }
        e.clip = CL_SPECIAL; e.clipT = 0;
      }
    } break;
    case E_B_BIGBOB:
      if (e.aiT <= 0 && dist < 11) { e.aiT = 3.6f; tele(e.pos + dir * 4, 4.5f, 1.0f, 1.4f, 0x80ff60); e.clip = CL_ATTACK; e.clipT = 0; }
      if (e.aiT2 <= 0) { e.aiT2 = 6.f; summon(rng.chance(0.5f) ? E_ZOMBIE : E_HEAD, 4); ring(10, 6, 0.5f); }
      break;
  }
  if (dist < e.radius + 0.8f && e.atkCd <= 0) { e.atkCd = 1.3f; damagePlayer(e.dmg, &e); }
  e.vel = mv;
}
