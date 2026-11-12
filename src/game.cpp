#include "i18n.h"
#include "game.h"
#include "audio.h"

// ====================================================================== run flow
float Game::stageDuration(int s) const {
  float m = cfg.mode == MODE_ENDLESS ? 10.f : cfg.stageMinutes <= 0 ? 15.f : cfg.stageMinutes;
  float base = m * 60.f;
  if (cfg.scaleStages) base *= (s == 0 ? 1.0f : s == 1 ? 0.9f : 0.8f);
  return base;
}

void Game::start(const RunConfig& c) {
  cfg = c; cfg.baseMap = c.map; cfg.baseTier = c.tier;
  rng = Rng(c.seed * 2654435761u + 7);
  P = Player();
  enemies.reserve(1400); projs.reserve(4000); areas.reserve(1500); pickups.reserve(2000); inters.reserve(600); parts.reserve(2000);
  P.ch = c.character; P.rerolls = c.rerolls; P.skips = c.skips; P.banishes = c.banishes;
  memset(dmgBySrc, 0, sizeof(dmgBySrc));
  numStages = c.mode == MODE_BOSSRUSH ? BOSSRUSH_ROUNDS : c.mode == MODE_GAUNTLET ? 3 : (c.mode == MODE_ENDLESS || c.mode == MODE_REVERSE) ? 1 : c.tier + 1;
  endlessBossT = 0; bossesBeaten = 0; endlessCycle = 0; gauntletSeen = {c.character}; pendingLevels = 0;
  stage = 0; runTime = 0; chestsBought = 0; firstEliteDone = 0; pumpkinBonus = 0; beaconBonus = 0; totalSilverEarned = 0;
  if (c.mode == MODE_REVERSE) { addWeapon(W_SWORD); addWeapon(W_AURA); }   // the Overlord: mace swings and a dread aura
  else addWeapon(weaponAllowed(gChars[P.ch].weapon) ? gChars[P.ch].weapon : W_SWORD);
  recomputeStats();
  P.hp = P.st[S_MAXHP]; P.shield = P.st[S_SHIELD];
  if (c.mode == MODE_SHRINEHUNT) P.gold = 120;
  if (c.mode == MODE_BOSSRUSH) pendingLevels = 6;   // a head start: the first boss arrives right away
  P.xpNext = 8;
  state = GS_PLAY; running = true;
  loadStage(0);
}

void Game::loadStage(int s) {
  stage = s; finalArena = false;
  setupStageFor(s);
  world.interDensity = cfg.mode == MODE_SHRINEHUNT ? 2.5f : 1.f;
  enemies.clear(); projs.clear(); areas.clear(); pickups.clear(); inters.clear(); parts.clear(); texts.clear(); allies.clear();
  world.generate(cfg.map, cfg.tier, cfg.mode == MODE_BOSSRUSH ? s % 3 : s, cfg.seed * 31 + s * 977 + 1, beaconBonus, pumpkinBonus);
  worldGen++;
  for (auto& is : world.inters) {
    Inter it; it.type = is.type; it.pos = is.pos; it.yaw = is.yaw; it.rarity = is.rarity;
    if (it.type == I_MICROWAVE) it.uses = it.rarity == R_COMMON ? 3 : it.rarity == R_LEGENDARY ? 1 : 2;
    if (it.type == I_SHADY) { for (int k = 0; k < 3; k++) { it.stock.push_back(rollItem(it.rarity)); it.sold.push_back(false); } }
    inters.push_back(it);
  }
  if (cfg.mode == MODE_ENDLESS || cfg.mode == MODE_BOSSRUSH || reverse())   // no key hunt; Endless and Reverse have no exit at all
    inters.erase(std::remove_if(inters.begin(), inters.end(), [&](const Inter& it) { return it.type == I_CRYPT_KEY || ((cfg.mode == MODE_ENDLESS || reverse()) && it.type == I_BOSS_PORTAL); }), inters.end());
  P.pos = world.spawnPoint + v3(0, 0.5f, 0); P.vel = v3(); P.onGround = false;
  stageTime = 0; stageDur = stageDuration(s);
  bossSummoned = bossDefeated = portalOpen = finalSwarm = false; finalSwarmT = 0; bossCount = mut(MU_BOSSDUO) ? 2 : 1; bossesAlive = 0; cryptKeys = 0; eventsDone = 0; spawnAcc = 0; cursedUsed = 0;
  monoType = -1;
  championsAlive = 0; legendSpawned = false; partyT = 45; minionT = 4;
  hazardT = 60; meteorT = 0; meteorsLeft = 0; hazardOn = false;
  if (mut(MU_MONO) && reverse()) monoType = E_H_FOX + rng.irange(10);
  else if (mut(MU_MONO)) { const auto& tab = gSpawnTables[cfg.map]; monoType = tab[rng.irange(std::min((int)tab.size(), 4 + s * 2 + cfg.tier * 2))].enemy; }   // rolled only when used: keeps classic runs identical per seed
  camYaw = 0; camEye = P.pos + v3(0, 3, -7); camTgt = P.pos;
  egN = (int)(world.T.size / egCell) + 2; egOrigin = -world.T.half() - egCell;
  egHead.assign(egN * egN, -1);
  const char* sname = gMaps[cfg.map].tiers[std::min(2, cfg.tier)].name;
  if (cfg.mode == MODE_BOSSRUSH) {
    eventsDone = 0xff;   // no swarms or minibosses: the boss is the round
    for (auto& it : inters) if (it.type == I_BOSS_PORTAL) { summonBoss(it); break; }
    for (auto& e : enemies) if (e.rank == RK_BOSS) { e.pos = spawnRingPos(15, 19); if (e.flying) e.pos.y += e.hover; }
    banners.clear();
    int bt = gBosses[cfg.map][std::min(2, cfg.tier)];
    banner(gEnemies[bt].name, fmt(tr("Boss %d/%d  •  %s"), s + 1, numStages + 1, sname), Col(255, 80, 80), 3.5f);
  } else if (reverse()) {
    eventsDone = 0xff;   // hero parties replace swarms and minibosses
    banner(tr("The heroes are coming"), sname, Col(192, 112, 255), 3.5f);
  } else {
    banner(sname, cfg.mode == MODE_ENDLESS ? std::string(tr("Endless Siege")) : fmt(tr("Stage %d/%d  •  Tier %d"), s + 1, numStages, cfg.tier + 1), Col(255, 255, 255), 3.5f);
    if (cfg.map == MAP_GRAVEYARD && cfg.mode != MODE_ENDLESS) banner(tr("Find 4 Crypt Keys"), tr("to open the boss portal"), Col(127, 255, 159), 3.5f);
  }
  // a few starting enemies
  for (int i = 0; i < 6; i++) spawnEnemy(reverse() ? heroEnemy() : gSpawnTables[cfg.map][i % 2].enemy, spawnRingPos(22, 30));
}

void Game::nextStage() {
  if (stage + 1 >= numStages) {
    if (cfg.tier == 2 && !finalArena) { startFinalArena(); return; }
    state = GS_VICTORY; deathT = 0; audioPlay(SFX_VICTORY);
    return;
  }
  // carry over: pumpkin/beacon effects
  pumpkinBonus = P.items[IT_PUMPKIN] * 10; beaconBonus = P.items[IT_BEACON] * 2;
  loadStage(stage + 1);
  heal(P.st[S_MAXHP] * 0.3f);
  if (cfg.mode == MODE_GAUNTLET) gauntletSwap();
  if (mut(MU_SHUFFLE)) shuffleWeapons();
}

const char* Game::heroName() const { return reverse() ? "Overlord" : gChars[P.ch].name; }

int Game::heroEnemy() {
  if (mut(MU_MONO) && monoType >= E_H_FOX) return monoType;
  return E_H_FOX + rng.irange(std::min(10, 4 + (int)(u() * 7)));   // tougher heroes join as the run goes on
}

void Game::setupStageFor(int s) {
  if (cfg.mode == MODE_BOSSRUSH) { cfg.map = s % MAP_COUNT; cfg.tier = std::min(2, s / 3); }   // three rounds per tier, walking through every map
  else if (cfg.mode == MODE_GAUNTLET) { cfg.map = (cfg.baseMap + s) % MAP_COUNT; cfg.tier = cfg.baseTier; }
}

bool Game::weaponAllowed(int w) const {
  if (!mut(MU_MELEE)) return true;
  int b = gWeapons[w].beh;
  return b == WB_MELEE || b == WB_AURA || b == WB_SPIN || b == WB_ORBIT || b == WB_CONE || b == WB_TRAIL || b == WB_AEGIS;
}

void Game::gauntletSwap() {
  int old = P.ch, nc = old;
  for (int k = 0; k < 40; k++) {
    nc = rng.irange(C_COUNT);
    if (std::find(gauntletSeen.begin(), gauntletSeen.end(), nc) == gauntletSeen.end()) break;
  }
  if (nc == old) nc = (old + 1 + rng.irange(C_COUNT - 1)) % C_COUNT;
  gauntletSeen.push_back(nc); P.ch = nc;
  int w = gChars[nc].weapon; if (!weaponAllowed(w)) w = W_SWORD;
  bool own = false; for (auto& x : P.weapons) if (x.def == w) own = true;
  if (!own) {
    if ((int)P.weapons.size() < maxWeapons()) addWeapon(w);
    else {   // full: the old hero's signature weapon (or the first slot) becomes the new one, upgrades kept
      int slot = 0; for (size_t i = 0; i < P.weapons.size(); i++) if (P.weapons[i].def == gChars[old].weapon) slot = (int)i;
      P.weapons[slot].def = w; P.weapons[slot].cd = 0.3f;
    }
  }
  statsDirty = true; recomputeStats(); P.hp = std::min(P.hp, P.st[S_MAXHP]);
  banner(fmt(tr("You are now %s"), gChars[nc].name), gChars[nc].passive, Col(79, 195, 247), 3.5f);
}

void Game::shuffleWeapons() {
  for (auto& wi : P.weapons) {
    for (int k = 0; k < 40; k++) {
      int w = rng.irange(W_COUNT); bool own = false;
      for (auto& x : P.weapons) if (x.def == w) own = true;
      if (own || !weaponAllowed(w)) continue;
      wi.def = w; wi.cd = 0.3f; wi.t = 0; wi.target = -1; wi.active = false; wi.charge = 0; break;
    }
  }
  alert(tr("WEAPON SHUFFLE"), tr("Your weapons changed"), Col(255, 214, 80), 2.5f);
}

void Game::surprisePot(v3 at) {
  switch (rng.irange(7)) {
    case 0: for (int k = 0; k < 6; k++) dropPickup(PI_GOLD, 0, at, 3 + 3 * u() + 3 * stage); floatText(at + v3(0, 2, 0), tr("Jackpot!"), Col(255, 214, 80)); break;
    case 1: dropXp(at, 25 * (1 + stage)); break;
    case 2: { areaDamage(at, 5.f, 40.f * (1 + stage) * (1 + u()), SRC_COUNT - 1, 0, 3); Area& a = addArea(); a.kind = AK_EXPLOSION; a.pos = at; a.radius = 5; a.life = a.maxLife = 0.35f; a.color = 0xff8a1a; audioPlay(SFX_EXPLODE); } break;
    case 3: forEnemiesNear(at, 9.f, [&](Enemy& e) { e.freezeT = std::max(e.freezeT, 2.5f); }); particles(at + v3(0, 1, 0), 20, Col(150, 230, 255), 6, 0.3f, 0.7f); floatText(at + v3(0, 2, 0), tr("Freeze!"), Col(150, 230, 255)); break;
    case 4: P.rageT = std::max(P.rageT, 6.f); floatText(at + v3(0, 2, 0), tr("Rage!"), Col(255, 90, 90)); break;
    case 5: P.shieldT = std::max(P.shieldT, 4.f); floatText(at + v3(0, 2, 0), tr("Shield!"), Col(120, 220, 255)); break;
    default: { Ally a; a.kind = 0; a.pos = at + v3(0, 1.5f, 0); a.life = 15; allies.push_back(a); floatText(at + v3(0, 2, 0), tr("A ghost friend!"), Col(200, 220, 255)); } break;
  }
}

void Game::meteor(v3 at, float dmg, float fall) {
  at.y = world.T.height(at.x, at.z);
  Projectile& pr = addProj(); pr.enemy = true; pr.kind = PK_ENEMY_LOB; pr.model = M_P_ROCK; pr.origin = at + v3(rng.range(-6.f, 6.f), 24, rng.range(-6.f, 6.f)); pr.pos = pr.origin;
  pr.aux3 = at; pr.life = pr.maxLife = fall; pr.dmg = dmg; pr.explode = 2.6f; pr.color = 0xff6a1a; pr.size = 2.f;
  Area& a = addArea(); a.kind = AK_TELEGRAPH; a.pos = at; a.radius = 2.6f; a.life = a.maxLife = fall; a.dmg = 0; a.color = 0xff6a1a;
}

void Game::updateModes(float dt) {
  // map hazards: Frostpeak blizzards (fog + snow, see render) and Caldera meteor showers
  if (!finalArena && cfg.map == MAP_FROST) {
    float c = fmodf(stageTime + 40.f, 100.f); bool on = c > 62 && c < 92;
    if (on && !hazardOn) alert(tr("BLIZZARD!"), tr("Visibility drops"), Col(220, 240, 255), 2.2f);
    hazardOn = on;
  }
  if (!finalArena && cfg.map == MAP_EMBER) {
    hazardT -= dt;
    if (hazardT <= 0) { hazardT = 65; meteorsLeft = 6 + 2 * (stage % 3) + 2 * cfg.tier; alert(tr("METEOR SHOWER!"), tr("Watch the red circles"), Col(255, 120, 50), 2.2f); }
    if (meteorsLeft > 0 && (meteorT -= dt) <= 0) { meteorT = 0.35f; meteorsLeft--; meteor(P.pos + rng.inDisk(9.f) + P.vel * 0.6f, 15.f * dmgMult() * (1 + 0.3f * cfg.tier), 1.3f); }
  }
  if (reverse()) {
    partyT -= dt; minionT -= dt;
    if (!legendSpawned && partyT <= 0) {   // a hero party: a small group charging from one side, led by an elite
      partyT = 75;
      int n = 3 + std::min(3, (int)(u() * 4)); v3 c = spawnRingPos(26, 32);
      for (int k = 0; k < n; k++) { v3 p = c + v3(rng.range(-3.f, 3.f), 0, rng.range(-3.f, 3.f)); p.y = world.T.height(p.x, p.z); spawnEnemy(heroEnemy(), p, k == 0); }
      alert(tr("HERO PARTY!"), fmt(tr("%d heroes storm your lair"), n), Col(120, 200, 255), 2.4f); audioPlay(SFX_SWARM);
    }
    if (!legendSpawned && u() >= 1.f) {   // the end: five different champions; beating them wins the run
      int pick[10]; for (int i = 0; i < 10; i++) pick[i] = i;
      for (int i = 9; i > 0; i--) std::swap(pick[i], pick[rng.irange(i + 1)]);
      v3 c = spawnRingPos(20, 24); championsAlive = 0;
      for (int k = 0; k < 5; k++) {
        float a = k * TAU / 5; v3 p = c + v3(cosf(a) * 3, 0, sinf(a) * 3); p.y = world.T.height(p.x, p.z);
        Enemy* e = spawnEnemy(E_H_FOX + pick[k], p, true);
        if (e) { e->champion = true; e->maxHp *= 7; e->hp = e->maxHp; e->dmg *= 1.4f; e->scale *= 1.15f; championsAlive++; }
      }
      if (championsAlive > 0) {
        legendSpawned = true; bossesAlive = championsAlive;
        banner(tr("THE LEGENDARY PARTY"), tr("Five champions came to end your reign"), Col(255, 214, 80), 4); audioPlay(SFX_BOSS);
      }
    }
    if (minionT <= 0) {   // the Overlord's goblin minions
      minionT = 14;
      int n = 2 + std::min(3, P.level / 12);
      for (int k = 0; k < n; k++) { Ally a; a.kind = 2; a.pos = P.pos + v3(rng.range(-2.f, 2.f), 0, rng.range(-2.f, 2.f)); a.pos.y = world.T.height(a.pos.x, a.pos.z); a.life = 12; allies.push_back(a); }
    }
  }
  if (cfg.mode == MODE_ENDLESS && !finalArena) {
    endlessBossT += dt;
    if (endlessBossT >= 300) {
      endlessBossT = 0;
      int wave = std::max(1, (int)(stageTime / 300));
      int bt = gBosses[cfg.map][std::min(2, (wave - 1) / 2)];
      Enemy* e = spawnEnemy(bt, spawnRingPos(16, 22)); if (e) bossesAlive++;
      alert(gEnemies[bt].name, tr("A boss joins the siege"), Col(255, 80, 80), 2.6f); audioPlay(SFX_BOSS);
    }
  }
  if (mut(MU_ROULETTE)) {
    P.rouletteT -= dt;
    if (P.rouletteT <= 0) {
      static const struct { int st; float good, bad; } kR[] = {{S_DAMAGE, .4f, -.25f}, {S_ATKSPD, 30, -20}, {S_MOVESPD, .25f, -.15f}, {S_CRIT, 25, -5}, {S_SIZE, .4f, -.2f}, {S_ARMOR, 25, -15}, {S_PICKUP, 1.f, -.4f}, {S_LUCK, 30, -10}};
      const int n = (int)(sizeof(kR) / sizeof(kR[0]));
      int g = rng.irange(n), b = rng.irange(n - 1); if (b >= g) b++;
      for (auto& v : P.roulette) v = 0;
      P.roulette[kR[g].st] = kR[g].good; P.roulette[kR[b].st] = kR[b].bad; P.rouletteT = 60; statsDirty = true;
      alert(tr("STAT ROULETTE"), statValueStr(kR[g].st, kR[g].good) + " " + tr(kStats[kR[g].st].name) + "   " + statValueStr(kR[b].st, kR[b].bad) + " " + tr(kStats[kR[b].st].name), Col(255, 214, 80), 2.6f);
    }
  }
  if (mut(MU_STREAK)) {
    P.streakT -= dt;
    if (P.streakT <= 0 && P.streak > 0) { P.streak = 0; if (P.streakTier) { P.streakTier = 0; statsDirty = true; } }
    int tier = P.streak >= 80 ? 3 : P.streak >= 40 ? 2 : P.streak >= 15 ? 1 : 0;
    if (tier > P.streakTier) {
      static const char* kT[4] = {"", "+20% Move Speed", "+30% Damage", "+30 Regeneration"};
      P.streakTier = tier; statsDirty = true;
      alert(fmt(tr("KILL STREAK x%d"), P.streak), tr(kT[tier]), Col(255, 140, 60), 1.6f);
    }
  }
}

void Game::startFinalArena() {
  finalArena = true;
  enemies.clear(); projs.clear(); areas.clear(); pickups.clear(); inters.clear(); allies.clear();
  World& W = world;
  // flatten a circular arena in the current world
  for (int j = 0; j < W.T.N; j++)
    for (int i = 0; i < W.T.N; i++) {
      float x = -W.T.half() + i * W.T.cell, z = -W.T.half() + j * W.T.cell;
      float d = sqrtf(x * x + z * z);
      W.T.h[j * W.T.N + i] = d < 34 ? 0.f : (d - 34) * 2.2f + fbm(x * 0.05f, z * 0.05f, 3, 2) * 6.f;
    }
  W.props.erase(std::remove_if(W.props.begin(), W.props.end(), [](const PropInst& p) { return sqrtf(p.pos.x * p.pos.x + p.pos.z * p.pos.z) < 40; }), W.props.end());
  for (auto& p : W.props) p.pos.y = W.T.height(p.pos.x, p.pos.z) - 0.1f;
  W.pal = gMaps[cfg.map].tiers[2].pal; W.pal.fogDensity *= 0.8f; W.pal.stars = true;
  W.buildGrid(); worldGen++;
  for (int k = 0; k < 4; k++) { Inter a; a.type = I_ALTAR; float an = k * TAU / 4 + PI / 4; a.pos = v3(cosf(an) * 22, 0, sinf(an) * 22); a.used = true; inters.push_back(a); }
  P.pos = v3(0, 0.5f, -20); P.vel = v3();
  stageTime = 0; stageDur = stageDuration(0) * (10.f / 15.f) * (cfg.stageMinutes > 0 ? cfg.stageMinutes / 10.f * 15.f / cfg.stageMinutes : 1);
  stageDur = (cfg.stageMinutes > 0 ? cfg.stageMinutes : 15.f) * 60.f;
  finalSwarm = false; finalSwarmT = 0; bossSummoned = true; bossesAlive = 0; eventsDone = 0xff;
  Enemy* b = spawnEnemy(E_B_GHOSTKING, v3(0, 0, 8));
  if (b) { b->hp = b->maxHp = 60000 * (1 + 0.5f * stage) * (1 + P.st[S_DIFFICULTY] / 200.f) * (P.level / 40.f + 0.6f); bossesAlive = 1; }
  banner(tr("FINAL BOSS"), "Ghost King", Col(216, 160, 255), 4);
  audioPlay(SFX_BOSS);
}

float Game::hpMult() const {
  float uu = finalArena ? 1.f : uCap(1.6f);
  float tierM = cfg.tier == 0 ? 1.f : cfg.tier == 1 ? 2.2f : 4.5f;
  float stageM = 1.f + 0.9f * (cfg.mode == MODE_BOSSRUSH ? stage % 3 : stage);
  float d = P.st[S_DIFFICULTY] / 100.f;
  float longBonus = 1.f + 0.15f * std::max(0.f, (cfg.stageMinutes > 0 ? cfg.stageMinutes : 15.f) - 10.f) / 5.f * uu;
  return tierM * stageM * (1 + 2.2f * uu + 5.5f * uu * uu) * (1 + 0.6f * d) * longBonus;
}
float Game::dmgMult() const {
  float uu = uCap(1.6f);
  float tierM = cfg.tier == 0 ? 1.f : cfg.tier == 1 ? 1.5f : 2.1f;
  float early = stage == 0 && cfg.tier == 0 ? lerpf(0.6f, 1.f, sat(uu * 4.f)) : 1.f;
  return 0.85f * early * tierM * (1 + 0.35f * stage) * (1 + 0.7f * uu + 0.5f * uu * uu) * (1 + 0.3f * P.st[S_DIFFICULTY] / 100.f);
}

// ====================================================================== stats
void Game::recomputeStats() {
  float* s = P.st;
  for (int i = 0; i < S_COUNT; i++) s[i] = kStats[i].base;
  for (auto& m : gChars[P.ch].stats) s[m.stat] += m.v;
  for (int i = 0; i < P.ntomes; i++) { const TomeDef& td = gTomes[P.tomes[i].def]; if (td.stat >= 0) s[td.stat] += P.tomes[i].value; }
  for (int i = 0; i < S_COUNT; i++) s[i] += P.chaos[i] + P.shrineBonus[i];
  for (int it = 0; it < IT_COUNT; it++) if (P.items[it]) for (auto& m : gItems[it].stats) s[m.stat] += m.v * P.items[it];
  // per-level passives
  int L = P.level;
  if (P.ch == C_FOX) s[S_LUCK] += L * 1.f;
  if (P.ch == C_NINJA) s[S_EVASION] += L * 1.f;
  if (P.ch == C_MONKE) s[S_MAXHP] += L * 2.f;
  if (P.ch == C_ROCCO) { s[S_PICKUP] += L * 0.03f; s[S_GOLDGAIN] += L * 0.01f; }
  if (P.ch == C_BRAMBLE) { s[S_REGEN] += L * 2.f; s[S_THORNS] += L * 1.f; }
  if (P.ch == C_SIZZLE) s[S_ATKSPD] += L * 1.5f;
  if (P.ch == C_DIGBY) s[S_ARMOR] += L * 0.75f;
  if (P.ch == C_HOOT) s[S_XPGAIN] += L * 0.015f;
  // run mutators (additive part)
  for (int i = 0; i < S_COUNT; i++) s[i] += P.roulette[i];
  if (mut(MU_GLASS)) s[S_DAMAGE] += 1.f;
  if (mut(MU_BRUTE)) { s[S_DAMAGE] += 1.f; s[S_ATKSPD] -= 50; }
  if (mut(MU_TRIGGER)) s[S_ATKSPD] += 100;
  if (mut(MU_SPECIALIST)) s[S_DAMAGE] += .75f;
  if (mut(MU_MADNESS)) s[S_CRITDMG] += 1.f;
  if (mut(MU_SURESHOT)) s[S_CRIT] += 100;
  if (mut(MU_GIANT)) { s[S_MAXHP] += 50; s[S_ARMOR] += 10; s[S_MOVESPD] -= .15f; s[S_KNOCKBACK] += .3f; }
  if (mut(MU_POCKET)) { s[S_MOVESPD] += .15f; s[S_EVASION] += 15; }
  if (mut(MU_MOON)) { s[S_JUMPH] += .8f; s[S_JUMPS] += 1; }
  if (reverse()) { s[S_MAXHP] += 140; s[S_ARMOR] += 12; s[S_MOVESPD] -= .08f; s[S_SIZE] += .25f; s[S_PICKUP] += .4f; s[S_KNOCKBACK] += .5f; }
  if (P.streakTier >= 1) s[S_MOVESPD] += .2f;
  if (P.streakTier >= 2) s[S_DAMAGE] += .3f;
  if (P.streakTier >= 3) s[S_REGEN] += 30;
  s[S_MAXHP] += P.demonicBlood + P.bloodMagicHp;
  s[S_DIFFICULTY] += P.greedDiff;
  // multiplicative item effects
  s[S_MAXHP] *= powf(0.95f, (float)P.items[IT_BEER]) * powf(1.5f, (float)P.items[IT_LEECHINGCRYSTAL]) * powf(0.8f, (float)P.items[IT_CURSEDGRABBIES]);
  s[S_REGEN] *= powf(0.5f, (float)P.items[IT_LEECHINGCRYSTAL]);
  if (P.items[IT_SPIKYSHIELD]) s[S_THORNS] += 2.f * s[S_ARMOR] * P.items[IT_SPIKYSHIELD];
  if (mut(MU_GLASS)) s[S_MAXHP] *= .5f;
  if (mut(MU_POCKET)) s[S_MAXHP] *= .75f;
  if (mut(MU_TRIGGER)) s[S_DAMAGE] *= .5f;
  if (mut(MU_SURESHOT)) s[S_CRITDMG] = 1 + (s[S_CRITDMG] - 1) * .5f;
  if (mut(MU_MOON)) s[S_FALLRED] = 100;
  if (int sp = mutatorSilverPct(cfg.mutators)) s[S_SILVERGAIN] *= 1 + sp / 100.f;
  s[S_ATKSPD] = std::max(-80.f, s[S_ATKSPD]);
  s[S_MAXHP] = std::max(1.f, s[S_MAXHP]);
  s[S_MOVESPD] = std::max(0.3f, s[S_MOVESPD]);
  s[S_SIZE] = std::max(0.3f, s[S_SIZE]);
  s[S_PICKUP] = std::max(0.3f, s[S_PICKUP]);
  float cap = s[S_MAXHP] * (1 + s[S_OVERHEAL] / 100.f);
  if (P.hp > cap) P.hp = cap;
  statsDirty = false;
}

// ====================================================================== helpers
Projectile& Game::addProj() {
  static Projectile dummy;
  if (projs.size() >= projs.capacity()) { dummy = Projectile(); dummy.dead = true; return dummy; }
  projs.emplace_back(); return projs.back();
}
Area& Game::addArea() {
  static Area dummy;
  if (areas.size() >= areas.capacity()) { dummy = Area(); dummy.dead = true; return dummy; }
  areas.emplace_back(); return areas.back();
}
void Game::particles(v3 pos, int n, Col c, float speed, float size, float life, bool add, int sprite, float grav) {
  if (parts.size() > 1500) n = std::min(n, 2);
  for (int i = 0; i < n; i++) {
    Particle p; p.pos = pos; p.vel = rng.onSphere() * speed * rng.range(0.4f, 1.f); p.vel.y = fabsf(p.vel.y) * 0.8f + speed * 0.2f;
    p.life = p.maxLife = life * rng.range(0.7f, 1.2f); p.size = size * rng.range(0.7f, 1.2f); p.size1 = p.size * 0.2f; p.c = c; p.add = add; p.sprite = (u8)sprite; p.grav = grav; p.rot = rng.f() * TAU; p.rotV = rng.range(-4, 4);
    parts.push_back(p);
  }
}
void Game::floatText(v3 pos, const std::string& s, Col c, float scale) {
  if (texts.size() > 140) texts.erase(texts.begin());
  FloatText t; t.pos = pos + v3(rng.range(-0.3f, 0.3f), 0, rng.range(-0.3f, 0.3f)); t.s = s; t.c = c; t.scale = scale; texts.push_back(t);
}
void Game::banner(const std::string& t, const std::string& sub, Col c, float life) { Banner b; b.text = t; b.sub = sub; b.c = c; b.life = life; banners.push_back(b); }
void Game::alert(const std::string& t, const std::string& sub, Col c, float life) {
  int n = 0; for (auto& b : banners) n += b.compact;
  if (n >= 2) for (auto it = banners.begin(); it != banners.end(); ++it) if (it->compact) { banners.erase(it); break; }
  Banner b; b.text = t; b.sub = sub; b.c = c; b.life = life; b.compact = true; banners.push_back(b);
}

v3 Game::spawnRingPos(float rmin, float rmax) {
  float H = world.playableHalf();
  for (int k = 0; k < 12; k++) {
    float a = rng.f() * TAU, d = rng.range(rmin, rmax);
    v3 p = P.pos + v3(cosf(a) * d, 0, sinf(a) * d);
    if (fabsf(p.x) < H && fabsf(p.z) < H) { p.y = world.T.height(p.x, p.z); return p; }
  }
  v3 p = P.pos * 0.9f; p.y = world.T.height(p.x, p.z); return p;
}

Enemy* Game::spawnEnemy(int type, v3 pos, bool elite) {
  if (enemies.size() >= enemies.capacity()) return nullptr;
  if ((int)enemies.size() > cfg.maxEnemies + 60 && gEnemies[type].rank == RK_NORMAL) return nullptr;
  const EnemyDef& d = gEnemies[type];
  Enemy e; e.type = type; e.id = nextId++; e.pos = pos; e.rank = d.rank; e.flying = d.flying; e.radius = d.radius; e.scale = d.scale;
  float hm = hpMult(), dm = dmgMult();
  e.maxHp = d.hp * hm; e.dmg = d.dmg * dm; e.speed = d.spd * std::min(1.45f, 1.f + 0.12f * std::min(u(), 1.5f) + 0.1f * P.st[S_DIFFICULTY] / 100.f);
  if (d.rank == RK_MINIBOSS) { e.maxHp = d.hp * hm * 3.0f; e.dmg = d.dmg * dm; }
  if (d.rank == RK_BOSS) {
    float uu = cfg.mode == MODE_BOSSRUSH ? 0.1f + 0.11f * stage : uCap(1.2f);
    float tierM = cfg.tier == 0 ? 1.f : cfg.tier == 1 ? 2.4f : 5.f;
    e.maxHp = d.hp * (0.8f + 2.7f * uu) * tierM * (1 + 0.9f * (cfg.mode == MODE_BOSSRUSH ? stage % 3 : stage)) * (1 + 0.5f * P.st[S_DIFFICULTY] / 100.f);
    if (mut(MU_BOSSDUO)) e.maxHp *= 0.75f;
    e.dmg = d.dmg * dm * 0.8f;
  }
  if (elite) { e.elite = true; e.maxHp *= 6; e.dmg *= 1.4f; e.scale *= 1.35f; e.radius *= 1.35f; }
  e.hp = e.maxHp; e.animPhase = rng.f() * 3; e.spawnT = 0; e.hover = rng.range(1.2f, 2.0f);
  e.yaw = atan2f(P.pos.x - pos.x, P.pos.z - pos.z);
  if (e.flying) e.pos.y = world.T.height(pos.x, pos.z) + e.hover;
  enemies.push_back(e);
  return &enemies.back();
}

void Game::dropXp(v3 pos, float v) {
  int n = v > 40 ? 3 : 1;
  for (int i = 0; i < n; i++) dropPickup(PI_XP, 0, pos, v / n);
}
void Game::dropPickup(int kind, int sub, v3 pos, float value) {
  if (kind == PI_XP && pickups.size() > 650) {
    // merge into the nearest existing xp gem to keep the count bounded
    float best = 1e9; int bi = -1;
    for (size_t i = 0; i < pickups.size(); i += 3) if (pickups[i].kind == PI_XP && !pickups[i].dead) { float d = dist2XZ(pickups[i].pos, pos); if (d < best) { best = d; bi = (int)i; } }
    if (bi >= 0) { pickups[bi].value += value; return; }
  }
  if (pickups.size() >= pickups.capacity()) return;
  Pickup p; p.kind = kind; p.sub = sub; p.pos = pos + v3(0, 0.6f, 0); p.value = value;
  p.vel = v3(rng.range(-2.5f, 2.5f), rng.range(4, 7), rng.range(-2.5f, 2.5f));
  pickups.push_back(p);
}

void Game::addXp(float v) {
  P.xp += v * P.st[S_XPGAIN];
  while (P.xp >= P.xpNext) {
    P.xp -= P.xpNext; P.level++;
    float L = (float)P.level;
    P.xpNext = 5 + 3.5f * L + 0.6f * powf(L, 2.05f);
    pendingLevels++;
    statsDirty = true;
  }
}

void Game::heal(float amt, bool fromRegen) {
  if (amt <= 0 || P.hp <= 0 || mut(MU_SHELL)) return;
  float cap = P.st[S_MAXHP] * (1 + P.st[S_OVERHEAL] / 100.f);
  float before = P.hp;
  P.hp = std::min(cap, P.hp + amt);
  float healed = P.hp - before;
  if (P.items[IT_HOLYBOOK] && healed > 0.5f && !fromRegen) areaDamage(P.pos, 4.f, healed * 4.f * P.items[IT_HOLYBOOK], SRC_ITEM + IT_HOLYBOOK, 0, 1);
  if (!fromRegen && healed >= 3) floatText(P.pos + v3(0, 2.2f, 0), fmt("+%d", (int)healed), Col(90, 255, 120), 0.9f);
}

void Game::addWeapon(int def) {
  for (auto& w : P.weapons) if (w.def == def) return;
  WeaponInst w; w.def = def; w.level = 1; w.cd = 0.3f;
  P.weapons.push_back(w);
}

void Game::addItem(int id, int n) {
  const ItemDef& d = gItems[id];
  for (int k = 0; k < n; k++) {
    if (d.maxStack > 0 && P.items[id] >= d.maxStack) break;
    P.items[id]++;
  }
  statsDirty = true;
}

int Game::rollRarity(float bonus) {
  float L = P.st[S_LUCK] + bonus;
  float w[R_COUNT] = {60, 25, 10, 4, 1};
  for (int r = 1; r < R_COUNT; r++) w[r] *= 1 + std::max(-0.8f, L / 50.f) * r;
  float tot = 0; for (float x : w) tot += x;
  float x = rng.f() * tot;
  for (int r = 0; r < R_COUNT; r++) { if (x < w[r]) return r; x -= w[r]; }
  return R_COMMON;
}

int Game::rollItem(int rarity) {
  // item rarities are Common/Rare/Epic/Legendary; map Uncommon -> Rare
  if (rarity == R_UNCOMMON) rarity = R_RARE;
  std::vector<int> pool;
  for (int i = 0; i < IT_COUNT; i++) {
    const ItemDef& d = gItems[i];
    if (!d.inPool || d.rarity != rarity) continue;
    if (d.maxStack > 0 && P.items[i] >= d.maxStack) continue;
    pool.push_back(i);
  }
  if (pool.empty()) return IT_OATS;
  return pool[rng.irange((int)pool.size())];
}

int Game::chestCost() const {
  return (int)roundf(30.f * powf(1.22f, (float)chestsBought) * (1 + 0.5f * stage) * (cfg.tier == 0 ? 1.f : cfg.tier == 1 ? 1.3f : 1.7f) * (mut(MU_BARGAIN) ? 0.5f : 1.f) * (cfg.mode == MODE_SHRINEHUNT ? 0.6f : 1.f));
}

// ====================================================================== offers
static int itemRarityFromRoll(int r) { return r == R_UNCOMMON ? R_RARE : r; }

void Game::genLevelOffers() {
  offers.clear();
  struct Cand { int kind, id, slot; float w; };
  std::vector<Cand> c;
  int wmax = weaponMaxLevel(), tmax = tomeMaxLevel();
  for (size_t i = 0; i < P.weapons.size(); i++) if (P.weapons[i].level < wmax) c.push_back({1, P.weapons[i].def, (int)i, 2.6f});
  if ((int)P.weapons.size() < maxWeapons())
    for (int w = 0; w < W_COUNT; w++) {
      bool own = false; for (auto& x : P.weapons) if (x.def == w) own = true;
      if (own || !weaponAllowed(w) || std::find(P.banishedW.begin(), P.banishedW.end(), w) != P.banishedW.end()) continue;
      c.push_back({0, w, -1, 0.55f});
    }
  for (int i = 0; i < P.ntomes; i++) if (P.tomes[i].level < tmax) c.push_back({3, P.tomes[i].def, i, 2.0f});
  if (P.ntomes < MAX_TOMES)
    for (int t = 0; t < T_COUNT; t++) {
      bool own = false; for (int i = 0; i < P.ntomes; i++) if (P.tomes[i].def == t) own = true;
      if (own || std::find(P.banishedT.begin(), P.banishedT.end(), t) != P.banishedT.end()) continue;
      c.push_back({2, t, -1, 0.5f});
    }
  for (int k = 0; k < (mut(MU_BUFFET) ? 4 : 3) && !c.empty(); k++) {
    float tot = 0; for (auto& x : c) tot += x.w;
    float r = rng.f() * tot; size_t pick = 0;
    for (size_t i = 0; i < c.size(); i++) { if (r < c[i].w) { pick = i; break; } r -= c[i].w; }
    Cand cd = c[pick]; c.erase(c.begin() + pick);
    Offer o; o.kind = cd.kind; o.id = cd.id; o.slot = cd.slot; o.rarity = rollRarity();
    if (o.kind == 1) {
      std::vector<UpGain> pool = gWeapons[o.id].pool;
      int n = std::min((int)pool.size(), 2 + (P.items[IT_ANVIL] > 0 ? 1 : 0));
      for (int g = 0; g < n; g++) {
        int pi = rng.irange((int)pool.size()); UpGain ug = pool[pi]; pool.erase(pool.begin() + pi);
        if (ug.stat == U_PROJ || ug.stat == U_BOUNCE) ug.v = o.rarity >= R_EPIC ? 2.f : 1.f;
        else ug.v *= kRarityMult[o.rarity];
        o.gains.push_back(ug);
      }
    } else if (o.kind == 2 || o.kind == 3) {
      o.value = gTomes[o.id].per * kRarityMult[o.rarity];
    }
    offers.push_back(o);
  }
}

void Game::genShrineOffers(bool golden) {
  offers.clear();
  std::vector<int> pool;
  for (int s = 0; s < S_COUNT; s++) if (kStats[s].shrine > 0) pool.push_back(s);
  for (int k = 0; k < 3; k++) {
    int pi = rng.irange((int)pool.size()); int st = pool[pi]; pool.erase(pool.begin() + pi);
    Offer o; o.kind = 4; o.id = st; o.rarity = golden ? R_LEGENDARY : rollRarity();
    o.value = kStats[st].shrine * kRarityMult[o.rarity] * (1 + 0.075f * P.items[IT_WRENCH]) * (1 + 0.25f * stage);
    offers.push_back(o);
  }
}

void Game::applyOffer(const Offer& o) {
  switch (o.kind) {
    case 0: addWeapon(o.id); break;
    case 1: for (auto& w : P.weapons) if (w.def == o.id) { w.level++; for (auto& g : o.gains) w.g[g.stat] += g.v; } break;
    case 2: if (P.ntomes < MAX_TOMES) { TomeInst t; t.def = o.id; t.level = 1; t.value = o.value; P.tomes[P.ntomes++] = t; if (o.id == T_CHAOS) { int s = rng.irange(S_COUNT); while (kStats[s].shrine <= 0) s = rng.irange(S_COUNT); P.chaos[s] += kStats[s].shrine * 2.5f * kRarityMult[o.rarity]; } } break;
    case 3: for (int i = 0; i < P.ntomes; i++) if (P.tomes[i].def == o.id) { P.tomes[i].level++; P.tomes[i].value += o.value; if (o.id == T_CHAOS) { int s = rng.irange(S_COUNT); while (kStats[s].shrine <= 0) s = rng.irange(S_COUNT); P.chaos[s] += kStats[s].shrine * 2.5f * kRarityMult[o.rarity]; } } break;
    case 4: P.shrineBonus[o.id] += o.value; break;
    case 5: addItem(o.id); break;
  }
  float oldMax = P.st[S_MAXHP];
  recomputeStats();
  if (P.st[S_MAXHP] > oldMax) P.hp += P.st[S_MAXHP] - oldMax;
}

std::string offerTitle(const Offer& o, const Game& g) {
  switch (o.kind) {
    case 0: return gWeapons[o.id].name;
    case 1: { int lv = 1; for (auto& w : g.P.weapons) if (w.def == o.id) lv = w.level; return fmt(tr("%s  Lv %d"), gWeapons[o.id].name, lv + 1); }
    case 2: return gTomes[o.id].name;
    case 3: { int lv = 1; for (int i = 0; i < g.P.ntomes; i++) if (g.P.tomes[i].def == o.id) lv = g.P.tomes[i].level; return fmt(tr("%s  Lv %d"), gTomes[o.id].name, lv + 1); }
    case 4: return kStats[o.id].name;
    case 5: return gItems[o.id].name;
  }
  return "";
}
std::string offerDesc(const Offer& o, const Game& g) {
  (void)g;
  switch (o.kind) {
    case 0: return std::string(tr("NEW WEAPON! ")) + tr(gWeapons[o.id].desc);
    case 1: {
      std::string s;
      for (auto& gn : o.gains) {
        if (!s.empty()) s += "\n";
        if (gn.stat == U_DMG) s += fmt(tr("+%.1f Damage"), gn.v);
        else if (gn.stat == U_PROJ) s += fmt(tr("+%d Projectile"), (int)gn.v);
        else if (gn.stat == U_BOUNCE) s += fmt(tr("+%d Ricochet"), (int)gn.v);
        else if (gn.stat == U_CRIT) s += fmt(tr("+%.0f%% Crit"), gn.v);
        else if (gn.stat == U_ATKSPD) s += fmt(tr("+%.0f%% Attack Spd."), gn.v);
        else s += fmt("+%.0f%% %s", gn.v * 100, tr(kUpStatName[gn.stat]));
      }
      return s;
    }
    case 2: case 3: {
      const TomeDef& t = gTomes[o.id];
      if (t.stat < 0) return std::string(o.kind == 2 ? tr("NEW TOME! ") : "") + tr(t.desc);
      return std::string(o.kind == 2 ? tr("NEW TOME! ") : "") + statValueStr(t.stat, o.value) + " " + tr(kStats[t.stat].name);
    }
    case 4: return statValueStr(o.id, o.value) + " " + tr(kStats[o.id].name);
    case 5: return gItems[o.id].desc;
  }
  return "";
}
int offerIcon(const Offer& o) {
  switch (o.kind) { case 0: case 1: return ICON_WEAPON0 + o.id; case 2: case 3: return ICON_TOME0 + o.id; case 5: return ICON_ITEM0 + o.id; default: return ICON_UI0 + UI_ARROWUP; }
}

// ====================================================================== interactables
int Game::nearestInter(float maxD) const {
  int best = -1; float bd = maxD * maxD;
  for (size_t i = 0; i < inters.size(); i++) {
    const Inter& it = inters[i];
    if (it.used) continue;
    if (it.type == I_POT || it.type == I_POT_BIG || it.type == I_POT_SILVER || it.type == I_PUMPKIN || it.type == I_TUMBLEWEED || it.type == I_CRYPT_KEY || it.type == I_CHARGE_SHRINE || it.type == I_ALTAR) continue;
    if (it.type == I_BOSS_PORTAL && bossSummoned && !portalOpen) continue;
    float d = dist2XZ(it.pos, P.pos);
    if (d < bd && fabsf(it.pos.y - P.pos.y) < 4) { bd = d; best = (int)i; }
  }
  return best;
}

int Game::interCost(int idx) const { return inters[idx].type == I_CHEST ? chestCost() : -1; }

std::string Game::interLabel(int idx) const {
  const Inter& it = inters[idx];
  switch (it.type) {
    case I_CHEST: return tr("Chest");
    case I_CHEST_GOLD: return tr("Golden chest");
    case I_GREED_SHRINE: return tr("+5% Difficulty");
    case I_CURSED_SHRINE: return tr("+1 Boss");
    case I_CHALLENGE_SHRINE: return tr("Elite challenge");
    case I_MOAI: return tr("Choose 1 item");
    case I_SHADY: return tr("Trade");
    case I_MICROWAVE: return fmt(tr("Microwave x%d"), it.uses);
    case I_SUCC_SHRINE: return tr("Pull XP");
    case I_BOSS_PORTAL:
      if (portalOpen) return stage + 1 >= numStages ? (cfg.tier == 2 && !finalArena ? tr("Face the Final Boss") : tr("Finish the run!")) : tr("Go through the portal");
      if (cfg.map == MAP_GRAVEYARD && cryptKeys < 4) return fmt(tr("Crypt Keys: %d/4"), cryptKeys);
      return bossCount > 1 ? fmt(tr("Summon %d bosses"), bossCount) : tr("Summon the boss");
  }
  return gInters[it.type].name;
}

void Game::openChest(Inter& it, bool free) {
  it.used = true; it.open = true; it.openT = 0;
  int bonus = it.type == I_CHEST_GOLD ? 25 : 0;
  int r = itemRarityFromRoll(rollRarity((float)bonus));
  chestItem = rollItem(r); chestRarity = gItems[chestItem].rarity;
  chestAnim = 0; P.chestsOpened++;
  if (P.items[IT_CCGREEN]) P.shrineBonus[S_LUCK] += 2.f * P.items[IT_CCGREEN];
  if (P.items[IT_CCRED]) P.shrineBonus[S_DAMAGE] += 0.025f * P.items[IT_CCRED];
  if (!free && it.type == I_CHEST) chestsBought++;
  state = GS_CHEST;
  audioPlay(SFX_CHEST);
  particles(it.pos + v3(0, 1, 0), 24, hexc(kRarityColor[chestRarity]), 6, 0.35f, 0.9f, true, SP_STAR, -4);
  if (P.items[IT_GHOST]) for (int k = 0; k < P.items[IT_GHOST]; k++) { Ally a; a.kind = 0; a.pos = it.pos + v3(0, 1.5f, 0); a.life = 12; allies.push_back(a); }
}

void Game::interact(int idx) {
  if (idx < 0) return;
  Inter& it = inters[idx];
  curInter = idx;
  auto ghosts = [&]() { if (P.items[IT_GHOST]) for (int k = 0; k < P.items[IT_GHOST]; k++) { Ally a; a.kind = 0; a.pos = it.pos + v3(0, 1.5f, 0); a.life = 12; allies.push_back(a); } };
  switch (it.type) {
    case I_CHEST: {
      int cost = chestCost();
      float n = (float)P.items[IT_KEY]; float pfree = 0.1f * n / (1 + 0.1f * n);
      bool free = rng.f() < pfree;
      if (!free && P.gold < cost) { floatText(it.pos + v3(0, 1.5f, 0), tr("Not enough gold"), Col(255, 120, 90)); audioPlay(SFX_DENY); return; }
      if (free) floatText(it.pos + v3(0, 1.8f, 0), tr("FREE!"), Col(255, 230, 90), 1.3f); else P.gold -= cost;
      openChest(it, free);
    } break;
    case I_CHEST_GOLD: openChest(it, true); break;
    case I_GREED_SHRINE: it.used = true; P.greedDiff += 5; P.gold += 20 * (1 + stage) * P.st[S_GOLDGAIN]; statsDirty = true; floatText(it.pos + v3(0, 2, 0), tr("+5% Difficulty"), Col(255, 210, 80), 1.2f); audioPlay(SFX_SHRINE); ghosts(); break;
    case I_CURSED_SHRINE: it.used = true; bossCount++; cursedUsed++; floatText(it.pos + v3(0, 3, 0), tr("+1 BOSS"), Col(200, 80, 255), 1.4f); audioPlay(SFX_SHRINE); ghosts(); break;
    case I_CHALLENGE_SHRINE: {
      it.used = true; it.challengeLeft = 5 + stage * 2;
      const auto& tab = gSpawnTables[cfg.map];
      for (int k = 0; k < it.challengeLeft; k++) {
        int t = tab[std::min((int)tab.size() - 1, 3 + rng.irange(5))].enemy;
        float a = k * TAU / it.challengeLeft;
        v3 p = it.pos + v3(cosf(a) * 9, 0, sinf(a) * 9); p.y = world.T.height(p.x, p.z);
        Enemy* e = spawnEnemy(t, p, true); if (e) { e->challenge = true; e->aiState = idx; }
      }
      alert(tr("CHALLENGE!"), tr("Defeat the elites to win a chest"), Col(194, 107, 255), 2.2f); audioPlay(SFX_SHRINE); ghosts();
    } break;
    case I_MOAI: {
      it.used = true; offers.clear();
      for (int k = 0; k < 3; k++) { Offer o; o.kind = 5; o.rarity = itemRarityFromRoll(rollRarity(10)); o.id = rollItem(o.rarity); o.rarity = gItems[o.id].rarity; offers.push_back(o); }
      state = GS_MOAI; audioPlay(SFX_SHRINE); ghosts();
    } break;
    case I_SHADY: state = GS_SHOP; ghosts(); break;
    case I_MICROWAVE: state = GS_MICROWAVE; break;
    case I_SUCC_SHRINE: it.used = true; for (auto& p : pickups) if (p.kind == PI_XP || p.kind == PI_GOLD) p.magnet = true; floatText(it.pos + v3(0, 3, 0), "SUCC!", Col(190, 120, 255), 1.4f); audioPlay(SFX_SHRINE); ghosts(); break;
    case I_BOSS_PORTAL:
      if (portalOpen) { state = GS_TRANSITION; transitionT = 0; audioPlay(SFX_PORTAL); return; }
      if (cfg.map == MAP_GRAVEYARD && cryptKeys < 4) { floatText(it.pos + v3(0, 3, 0), fmt(tr("%d keys missing"), 4 - cryptKeys), Col(127, 255, 159)); return; }
      if (!bossSummoned) summonBoss(it);
      break;
  }
}

void Game::summonBoss(Inter& portal) {
  bossSummoned = true;
  int bt = gBosses[cfg.map][std::min(2, cfg.tier)];
  for (int k = 0; k < bossCount; k++) {
    float a = k * TAU / std::max(1, bossCount);
    v3 p = portal.pos + v3(cosf(a) * 6 + 0.01f, 0, sinf(a) * 6 + 4); p.y = world.T.height(p.x, p.z);
    Enemy* e = spawnEnemy(bt, p); if (e) { e->bossIdx = k; bossesAlive++; }
  }
  banner(gEnemies[bt].name, bossCount > 1 ? fmt(tr("x%d - Cursed Shrine"), bossCount) : tr("STAGE BOSS"), Col(255, 80, 80), 3);
  audioPlay(SFX_BOSS);
  shake = 0.6f;
}

void Game::bossDied(Enemy& e) {
  bossesAlive = std::max(0, bossesAlive - 1);
  Inter c; c.type = I_CHEST_GOLD; c.pos = e.pos; c.pos.y = world.T.height(e.pos.x, e.pos.z); inters.push_back(c);
  int silver = (int)((20 + 15 * stage + 10 * cfg.tier) * P.st[S_SILVERGAIN] * (1 + 0.5f * cursedUsed));
  P.silver += silver; totalSilverEarned += silver;
  floatText(e.pos + v3(0, 3, 0), fmt(tr("+%d silver"), silver), Col(220, 225, 235), 1.3f);
  bossesBeaten++;
  if (finalArena) { state = GS_VICTORY; deathT = 0; audioPlay(SFX_VICTORY); return; }
  if (cfg.mode == MODE_ENDLESS) { alert(tr("BOSS DEFEATED!"), tr("A golden chest appeared"), Col(120, 255, 140), 2.5f); audioPlay(SFX_LEVELUP); return; }
  if (bossesAlive == 0 && cfg.mode == MODE_BOSSRUSH) { pendingLevels += 3; heal(P.st[S_MAXHP] * 0.4f); }
  if (bossesAlive == 0) {
    bossDefeated = true; portalOpen = true;
    for (auto& p : pickups) if (p.kind == PI_XP) p.magnet = true;
    banner(tr("BOSS DEFEATED!"), stage + 1 >= numStages ? (cfg.tier == 2 ? tr("Enter the portal for the final boss") : tr("Enter the portal to win")) : tr("The portal to the next stage has opened"), Col(120, 255, 140), 3.5f);
    audioPlay(SFX_LEVELUP);
  }
}

// ====================================================================== update
void Game::update(float dt) {
  if (!running) return;
  realTime += dt;
  for (auto& b : banners) b.t += dt;
  banners.erase(std::remove_if(banners.begin(), banners.end(), [](const Banner& b) { return b.t > b.life; }), banners.end());
  if (state == GS_TRANSITION) {
    transitionT += dt;
    if (transitionT > 0.9f) { state = GS_PLAY; nextStage(); }
    return;
  }
  if (state == GS_DEAD || state == GS_VICTORY) { deathT += dt; updateFx(dt * 0.3f); return; }
  if (state == GS_CHEST) { chestAnim += dt; if (autoPilot && chestAnim > 0.8f) { addItem(chestItem); recomputeStats(); state = GS_PLAY; } return; }
  if (state != GS_PLAY) {
    if (autoPilot) {
      if (state == GS_LEVELUP || state == GS_SHRINE || state == GS_MOAI) {
        // heuristic pick: prefer weapon upgrades and damage/cooldown/quantity
        int best = 0; float bs = -1;
        for (size_t i = 0; i < offers.size(); i++) {
          const Offer& o = offers[i]; float s = 1 + o.rarity;
          if (o.kind == 1) s += 2.2f; if (o.kind == 0 && P.weapons.size() < 4) s += 2.5f;
          if ((o.kind == 2 || o.kind == 3) && (o.id == T_DAMAGE || o.id == T_COOLDOWN || o.id == T_QUANTITY || o.id == T_SIZE || o.id == T_HP || o.id == T_REGEN)) s += 3.2f;
          if (o.kind == 2 && P.ntomes < 4) s += 0.5f;
          if (o.kind == 4 && (o.id == S_DAMAGE || o.id == S_ATKSPD || o.id == S_MAXHP || o.id == S_ARMOR)) s += 1;
          if (s > bs) { bs = s; best = (int)i; }
        }
        if (!offers.empty()) applyOffer(offers[best]);
        if (state == GS_LEVELUP && pendingLevels > 0) pendingLevels--;
        state = GS_PLAY;
      } else state = GS_PLAY;
    }
    return;
  }
  if (statsDirty) { float oldMax = P.st[S_MAXHP]; recomputeStats(); if (P.st[S_MAXHP] > oldMax) P.hp += P.st[S_MAXHP] - oldMax; }
  if (hitStop > 0) { hitStop -= dt; return; }
  float warp = mut(MU_TIMEWARP) ? 1.f + 0.32f * sinf(realTime * 0.7f) + 0.12f * sinf(realTime * 2.3f) : 1.f;
  float sdt = dt * timeScale * warp;
  float edt = sdt * (P.speedBoiT > 0 ? 0.45f : 1.f) * (P.zaT > 0 || freezeEnemiesT > 0 ? 0.f : 1.f);
  if (autoPilot) updateAutopilot(dt);
  stageTime += sdt; runTime += sdt;
  updatePlayer(sdt);
  updateCamera(dt);
  buildEnemyGrid();
  updateSpawning(sdt);
  updateModes(sdt);
  updateEnemies(edt);
  updateWeapons(sdt);
  updateProjectiles(sdt);
  updateAreas(sdt);
  updatePickups(sdt);
  updateInters(sdt);
  updateItems(sdt);
  updateFx(sdt);
  // cleanup
  enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.alive && e.deathT > 0.3f; }), enemies.end());
  projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Projectile& p) { return p.dead; }), projs.end());
  areas.erase(std::remove_if(areas.begin(), areas.end(), [](const Area& a) { return a.dead; }), areas.end());
  pickups.erase(std::remove_if(pickups.begin(), pickups.end(), [](const Pickup& p) { return p.dead; }), pickups.end());
  if (pendingLevels > 0 && state == GS_PLAY) { genLevelOffers(); state = GS_LEVELUP; audioPlay(SFX_LEVELUP); }
  if (P.hp <= 0 && state == GS_PLAY) { state = GS_DEAD; deathT = 0; audioPlay(SFX_DEATH); }
}

void Game::updateCamera(float dt) {
  camYaw += in.camDX; camPitch = clampf(camPitch + in.camDY, -0.15f, 1.1f);
  if (in.camManual) camManualT = 1.8f; else camManualT -= dt;
  float spd = len(v2(P.vel.x, P.vel.z));
  if (camManualT <= 0 && spd > 1.5f && len(in.move) > 0.2f) {
    float target = atan2f(P.vel.x, P.vel.z);
    float diff = wrapAngle(target - camYaw);
    if (fabsf(diff) < 2.6f) camYaw += diff * std::min(1.f, dt * 1.1f) * sat(spd / 8.f);
  }
  v3 tgt = P.pos + v3(0, reverse() ? 2.1f : 1.45f, 0);   // the Overlord is a head taller: frame it from further back
  float cp = cosf(camPitch), sp = sinf(camPitch);
  v3 eye = tgt + v3(-sinf(camYaw) * cp, sp, -cosf(camYaw) * cp) * camDist * (reverse() ? 1.35f : 1.f);
  float gy = world.T.height(eye.x, eye.z) + 0.6f;
  if (eye.y < gy) eye.y = gy;
  float k = std::min(1.f, dt * 14.f);
  camTgt = lerp3(camTgt, tgt, k); camEye = lerp3(camEye, eye, std::min(1.f, dt * 18.f));
  // keep the camera out of rocks/terrain: march from the target to the eye
  {
    v3 dv = camEye - camTgt; float D = len(dv);
    if (D > 0.5f) {
      v3 dn = dv / D; float maxD = D;
      for (float t = 0.8f; t < D; t += 0.35f) {
        v3 q = camTgt + dn * t;
        if (q.y < world.T.height(q.x, q.z) + 0.25f || world.pointBlocked(q)) { maxD = std::max(1.2f, t - 0.35f); break; }
      }
      if (maxD < D) camEye = camTgt + dn * maxD;
    }
  }
  if (shake > 0) { shake -= dt; float s = std::max(0.f, shake) * 0.35f * shakeSetting; camEye += v3(rng.range(-s, s), rng.range(-s, s), rng.range(-s, s)); }
}

void Game::updatePlayer(float dt) {
  Player& p = P;
  float cs = cosf(camYaw), sn = sinf(camYaw);
  v3 fwd(sn, 0, cs), right(-cs, 0, sn); // right = cross(fwd, up)
  v2 mv = in.move; float ml = len(mv); if (ml > 1) { mv = mv / ml; ml = 1; }
  v3 wish = (right * mv.x + fwd * mv.y);
  float speed = 7.8f * p.st[S_MOVESPD] * (p.cowardT > 0 ? 1.35f : 1.f) * (p.phantomT > 0 ? 1.25f : 1.f) * (1 + p.speedDemon);
  if (p.ch == C_CALCIUM) { if (ml > 0.3f) p.speedDemon = std::min(0.6f, p.speedDemon + dt * 0.03f); }
  v3 hv(p.vel.x, 0, p.vel.z);
  if (p.onGround) {
    if (p.sliding) {
      v3 n = world.T.normal(p.pos.x, p.pos.z);
      v3 downhill = v3(n.x, 0, n.z) * 26.f;
      hv += downhill * dt;
      hv += wish * speed * 1.5f * dt;
      float hl = len(hv); float fr = std::max(0.f, hl - 2.f * dt);
      hv = hl > 0.01f ? hv / hl * fr : v3();
      float cap = speed * 2.1f; hl = len(hv); if (hl > cap) hv = hv / hl * cap;
      if (!in.slide || hl < 2.0f) p.sliding = false;
    } else {
      v3 target = wish * speed;
      float acc = ml > 0.05f ? 70.f : 40.f;
      v3 dv = target - hv; float dl = len(dv);
      if (dl > acc * dt) dv = dv / dl * acc * dt;
      // keep bhop momentum briefly after landing
      if (p.landT < 0.12f && len(hv) > speed && ml > 0.05f) { float hl = len(hv); hv = norm(hv * 0.9f + target * 0.1f / std::max(speed, 0.1f) * hl) * hl; }
      else hv += dv;
      if (in.slide && len(hv) > 3.0f) { p.sliding = true; hv = hv * 1.18f; audioPlay(SFX_SLIDE); }
    }
  } else {
    v3 target = wish * std::max(speed, len(hv));
    v3 dv = target - hv; float dl = len(dv); float acc = 22.f;
    if (dl > acc * dt) dv = dv / dl * acc * dt;
    if (ml > 0.05f) hv += dv;
  }
  p.vel.x = hv.x; p.vel.z = hv.z;
  // jumping
  int maxJumps = 1 + (int)p.st[S_JUMPS];
  bool wantJump = in.jump || (cfg.autoBhop && in.jumpHeld && p.onGround);
  if (p.onGround) p.coyote = 0.1f; else p.coyote -= dt;
  if (wantJump) {
    if (p.onGround || p.coyote > 0) {
      p.vel.y = 10.2f * sqrtf(p.st[S_JUMPH]);
      p.onGround = false; p.jumps = 1; p.coyote = 0;
      if (p.landT < 0.15f) { float hl = len(v2(p.vel.x, p.vel.z)); float cap = speed * 1.7f; if (hl > 0.1f && hl < cap) { p.vel.x *= 1.06f; p.vel.z *= 1.06f; } }
      if (p.sliding) { p.sliding = false; }
      audioPlay(SFX_JUMP);
    } else if (p.jumps < maxJumps) {
      p.vel.y = 9.0f * sqrtf(p.st[S_JUMPH]); p.jumps++;
      particles(p.pos, 8, Col(255, 255, 255, 160), 3, 0.3f, 0.4f, false, SP_SMOKE);
      audioPlay(SFX_JUMP);
    }
  }
  in.jump = false;
  p.vel.y -= 26.f * dt;
  if (p.vel.y < -40) p.vel.y = -40;
  v3 np = p.pos + p.vel * dt;
  v3 wallN;
  bool hitWall = world.collide(np, 0.42f, np.y, &wallN);
  if (hitWall && p.ch == C_MONKE && ml > 0.2f && dot(wish, wallN) < -0.2f) { p.vel.y = std::max(p.vel.y, 6.5f); }
  float g = world.groundAt(np.x, np.z, np.y);
  bool wasGround = p.onGround;
  if (np.y <= g + 0.02f && p.vel.y <= 0.5f) {
    if (!wasGround) {
      float fall = p.airMaxY - g;
      if (fall > 10 && !p.sliding && !cfg.godMode && p.ch != C_DIGBY) damagePlayer((fall - 10) * 3.f * (1 - p.st[S_FALLRED] / 100.f) * (p.ch == C_FOX ? 0.5f : 1.f), nullptr, false);
      p.landT = 0;
      if (fall > 2) particles(v3(np.x, g, np.z), 6, Col(220, 220, 220, 140), 2.5f, 0.35f, 0.35f, false, SP_SMOKE);
    }
    np.y = g; p.vel.y = 0; p.onGround = true; p.jumps = 0; p.airMaxY = g;
  } else {
    // snap down small steps while walking downhill
    if (wasGround && p.vel.y <= 0 && np.y - g < 0.6f) { np.y = g; p.vel.y = 0; p.onGround = true; }
    else { p.onGround = false; p.airMaxY = std::max(p.airMaxY, np.y); }
  }
  p.landT += dt;
  float moved = len(v3(np.x - p.pos.x, 0, np.z - p.pos.z));
  p.distMoved += moved;
  p.pos = np;
  if (ml > 0.1f || len(hv) > 1.5f) { float ty = atan2f(hv.x, hv.z); if (len(hv) > 0.5f) p.yaw = angleLerp(p.yaw, ty, std::min(1.f, dt * 14.f)); p.idleT = 0; }
  else p.idleT += dt;
  // regen & shield
  p.regenAcc += p.st[S_REGEN] / 60.f * dt;
  if (p.regenAcc >= 1) { heal(floorf(p.regenAcc), true); p.regenAcc -= floorf(p.regenAcc); }
  if (stageTime - p.lastHitT > 3.f && p.shield < p.st[S_SHIELD]) p.shield = std::min(p.st[S_SHIELD], p.shield + p.st[S_SHIELD] * 0.25f * dt);
  if (p.iframes > 0) p.iframes -= dt;
  if (p.shield > p.st[S_SHIELD]) p.shield = p.st[S_SHIELD];
}

// ====================================================================== spawning & events
void Game::updateSpawning(float dt) {
  float uu = cfg.mode == MODE_BOSSRUSH ? uCap(1.f) : u();
  if (finalArena) {
    spawnAcc += dt * (1.5f + stageTime * 0.01f);
    while (spawnAcc > 1 && (int)enemies.size() < cfg.maxEnemies / 2) { spawnAcc -= 1; spawnEnemy(E_SWARMGHOST, spawnRingPos(20, 30)); }
    if (stageTime > stageDur && !finalSwarm) { finalSwarm = true; banner(tr("THE FINAL SWARM HAS BEGUN"), tr("Defeat the Ghost King!"), Col(255, 90, 90)); }
    return;
  }
  bool infinite = cfg.stageMinutes <= 0;
  float d = P.st[S_DIFFICULTY] / 100.f;
  int alive = 0; for (auto& e : enemies) if (e.alive && e.rank == RK_NORMAL) alive++;
  float target = (14 + 234 * powf(std::min(uu, cfg.mode == MODE_ENDLESS ? 2.5f : 1.3f), 1.1f)) * (1 + 0.3f * (cfg.mode == MODE_BOSSRUSH ? stage % 3 : stage) + 0.25f * cfg.tier) * (1 + 1.1f * d);
  if (bossesAlive > 0) target *= 0.55f;
  if (cfg.mode == MODE_BOSSRUSH) target *= 0.45f;
  if (cfg.mode == MODE_SHRINEHUNT) target *= 0.6f;
  if (reverse()) target *= 0.4f;
  target = std::min(target, (float)cfg.maxEnemies);
  eliteT -= dt;
  spawnAcc += dt * (2.f + target * 0.22f);
  const auto& tab = gSpawnTables[cfg.map];
  float uEff = std::min(1.25f, uu + 0.35f * stage + 0.2f * cfg.tier);
  int guard = 0;
  while (spawnAcc >= 1 && alive < (int)target && guard++ < 12) {
    spawnAcc -= 1;
    float tot = 0; float w[16];
    for (size_t i = 0; i < tab.size(); i++) { w[i] = tab[i].uStart <= uEff ? tab[i].weight * (uEff - tab[i].uStart < 0.4f ? 1.3f : 0.75f) : 0; tot += w[i]; }
    float r = rng.f() * tot; int pick = 0;
    for (size_t i = 0; i < tab.size(); i++) { if (r < w[i]) { pick = (int)i; break; } r -= w[i]; }
    int type = tab[pick].enemy;
    if (rng.chance(0.003f)) type = rng.chance(0.5f) ? E_SKELETON_GOLD : E_SKELETON_XP;
    bool elite = false;
    if (reverse()) type = heroEnemy();
    if (eliteT <= 0) { elite = true; eliteT = (30.f - 18.f * std::min(uu, 1.f)) / std::max(0.2f, P.st[S_ELITESPAWN]) / (mut(MU_ELITES) ? 3.f : 1.f) / (mut(MU_MONO) ? 1.6f : 1.f); }
    if (mut(MU_MONO) && monoType >= 0 && gEnemies[type].rank == RK_NORMAL && type != E_SKELETON_GOLD && type != E_SKELETON_XP) type = monoType;
    spawnEnemy(type, spawnRingPos(24, 38), elite);
    alive++;
  }
  if (spawnAcc > 6) spawnAcc = 6;
  // events as fractions of the stage timer (10-min original: swarms at 7:00 and 3:00, minibosses at 7:00 and 2:00)
  float ev = uu;
  if (cfg.mode == MODE_ENDLESS) { int cyc = (int)uu; if (cyc != endlessCycle) { endlessCycle = cyc; eventsDone = 0; } ev = uu - cyc; }
  auto event = [&](int bit, float at, std::function<void()> f) { if (!(eventsDone & bit) && ev >= at) { eventsDone |= bit; f(); } };
  auto swarm = [&]() {
    int n = 45 + 15 * stage + 10 * cfg.tier;
    for (int k = 0; k < n; k++) {
      float a = k * TAU / n; v3 p = P.pos + v3(cosf(a) * 30, 0, sinf(a) * 30);
      float H = world.playableHalf(); p.x = clampf(p.x, -H, H); p.z = clampf(p.z, -H, H); p.y = world.T.height(p.x, p.z);
      int t = tab[std::min((int)tab.size() - 1, rng.irange(std::max(1, (int)(uEff * tab.size()))))].enemy;
      Enemy* e = spawnEnemy(t, p); if (e) e->speed *= 1.25f;
    }
    alert(tr("SWARM!"), tr("A horde approaches"), Col(255, 170, 60)); audioPlay(SFX_SWARM);
  };
  auto miniboss = [&](int k) {
    int t = gMinibosses[cfg.map][k];
    spawnEnemy(t, spawnRingPos(26, 32));
    alert(tr("MINIBOSS!"), gEnemies[t].name, Col(255, 120, 120)); audioPlay(SFX_BOSS);
  };
  if (!infinite || true) {
    event(1, 0.30f, swarm); event(2, 0.30f, [&] { miniboss(0); }); event(4, 0.70f, swarm); event(8, 0.80f, [&] { miniboss(1); });
  }
  if (!infinite && !reverse() && cfg.swarmMode == 0 && uu >= 1.f && !finalSwarm) { finalSwarm = true; banner(tr("THE FINAL SWARM HAS BEGUN"), bossDefeated ? tr("Run to the portal!") : tr("Kill the boss and flee through the portal!"), Col(255, 80, 80), 4); audioPlay(SFX_SWARM); }
  if (cfg.swarmMode == 1 && !reverse() && bossDefeated && !finalSwarm && !infinite && uu >= 1.f) { finalSwarm = true; banner(tr("THE FINAL SWARM HAS BEGUN"), tr("Enter the portal!"), Col(255, 80, 80), 4); }
  if (finalSwarm) {
    finalSwarmT += dt;
    float rate = 3.f + finalSwarmT * 0.12f;
    static float acc = 0; acc += dt * rate;
    while (acc > 1) {
      acc -= 1;
      Enemy* e = spawnEnemy(finalSwarmT > 180 ? E_GHOST_RED : E_SWARMGHOST, spawnRingPos(20, 32));
      if (e) { float k = powf(1.15f, finalSwarmT / 10.f); e->maxHp *= k; e->hp = e->maxHp; e->dmg *= sqrtf(k); e->speed *= finalSwarmT > 120 ? 1.35f : 1.f; }
    }
  }
}

// ====================================================================== pickups & interactables
void Game::updatePickups(float dt) {
  float pr = pickupRadius();
  for (auto& p : pickups) {
    if (p.dead) continue;
    p.t += dt;
    float d = len(P.pos + v3(0, 0.8f, 0) - p.pos);
    if (!p.magnet && ((d < pr && p.t > 0.25f) || (p.kind == PI_XP && p.t > 50.f))) p.magnet = true;
    if (p.magnet) {
      v3 dir = norm(P.pos + v3(0, 0.8f, 0) - p.pos);
      float sp = 14.f + p.t * 6.f;
      p.vel = lerp3(p.vel, dir * sp, std::min(1.f, dt * 8.f));
      if (P.items[IT_SHATTEREDKNOWLEDGE] && p.kind == PI_XP) {
        forEnemiesNear(p.pos, 0.9f, [&](Enemy& e) { if (e.hitCd[7] <= 0) { e.hitCd[7] = 0.4f; hitEnemy(e, 6.f * P.items[IT_SHATTEREDKNOWLEDGE] * (1 + stage), SRC_ITEM + IT_SHATTEREDKNOWLEDGE, 0.3f, 0, p.pos, 0.2f); } });
      }
    } else {
      p.vel.y -= 20 * dt; p.vel.x *= 1 - dt * 3; p.vel.z *= 1 - dt * 3;
    }
    p.pos += p.vel * dt;
    float g = world.T.height(p.pos.x, p.pos.z) + 0.35f;
    if (!p.magnet && p.pos.y < g) { p.pos.y = g; p.vel.y = 0; }
    if (d < 1.0f && p.t > 0.15f) {
      p.dead = true;
      switch (p.kind) {
        case PI_XP: addXp(p.value); audioPlay(SFX_XP); break;
        case PI_GOLD: { double gv = p.value * P.st[S_GOLDGAIN] * (P.goldT > 0 ? 2.0 : 1.0); P.gold += gv; audioPlay(SFX_COIN); } break;
        case PI_SILVER: { int sv = std::max(1, (int)roundf(p.value * P.st[S_SILVERGAIN])); P.silver += sv; totalSilverEarned += sv; audioPlay(SFX_COIN); } break;
        case PI_BORGAR: heal(P.st[S_MAXHP] * 0.2f); audioPlay(SFX_HEAL); break;
        case PI_POWERUP: {
          float mult = P.st[S_POWERUP];
          const char* nm = tr(gPowerups[p.sub].name);
          floatText(P.pos + v3(0, 2.3f, 0), nm, hexc(gPowerups[p.sub].color), 1.4f);
          switch (p.sub) {
            case PU_SHIELD: P.shieldT = 6 * mult; break;
            case PU_CLOCK: freezeEnemiesT = 5 * mult; break;
            case PU_GOLD: P.goldT = 10 * mult; break;
            case PU_RAGE: P.rageT = 8 * mult; break;
            case PU_MAGNET: for (auto& q : pickups) if (q.kind == PI_XP || q.kind == PI_GOLD) q.magnet = true; break;
            case PU_NUKE: areaDamage(P.pos, 18, 60 * hpMult() * mult, SRC_ITEM + IT_COUNT, 0, 4, -1, false); particles(P.pos + v3(0, 1, 0), 60, Col(255, 180, 80), 18, 0.8f, 0.9f); shake = 0.8f; audioPlay(SFX_EXPLODE); break;
          }
          audioPlay(SFX_POWERUP);
        } break;
      }
    }
    if (p.t > 180 && !p.magnet) p.dead = true;
  }
}

void Game::updateInters(float dt) {
  for (size_t i = 0; i < inters.size(); i++) {
    Inter& it = inters[i];
    it.animT += dt;
    if (it.open) it.openT += dt;
    float d = distXZ(it.pos, P.pos);
    switch (it.type) {
      case I_POT: case I_POT_BIG: case I_POT_SILVER: case I_PUMPKIN: case I_TUMBLEWEED:
        if (!it.used && d < 1.3f + (it.type == I_POT_BIG ? 0.4f : 0) && fabsf(P.pos.y - it.pos.y) < 2) {
          it.used = true;
          Col c = it.type == I_PUMPKIN ? Col(255, 150, 40) : it.type == I_TUMBLEWEED ? Col(200, 160, 100) : it.type == I_POT_SILVER ? Col(220, 225, 235) : Col(190, 120, 75);
          particles(it.pos + v3(0, 0.5f, 0), 14, c, 5, 0.3f, 0.6f, false, SP_WHITE, -12);
          audioPlay(SFX_BREAK);
          float mul = 1 + 0.1f * P.items[IT_PUMPKIN] * 0;
          if (mut(MU_POTS) && it.type != I_TUMBLEWEED && rng.chance(0.45f)) surprisePot(it.pos);
          if (it.type == I_POT_SILVER) { for (int k = 0; k < 3; k++) dropPickup(PI_SILVER, 0, it.pos, 1 + stage); }
          else {
            int n = it.type == I_POT_BIG ? 4 : 2;
            for (int k = 0; k < n; k++) dropPickup(PI_GOLD, 0, it.pos, (2 + 3 * u() + 3 * stage) * mul);
            if (rng.chance(0.5f)) dropXp(it.pos, 3 * (1 + stage));
            if (rng.chance(0.10f * P.st[S_POWERUPCH] * (it.type == I_POT_BIG ? 2 : 1))) dropPickup(PI_POWERUP, rng.irange(PU_COUNT), it.pos, 1);
            if (rng.chance(0.05f)) dropPickup(PI_BORGAR, 0, it.pos, 1);
            if (rng.chance(0.06f)) dropPickup(PI_SILVER, 0, it.pos, 1);
          }
        }
        break;
      case I_CRYPT_KEY:
        if (!it.used && d < 1.8f) { it.used = true; cryptKeys++; alert(fmt(tr("Crypt Key %d/4"), cryptKeys), cryptKeys >= 4 ? tr("The boss portal is open!") : "", Col(127, 255, 159), 2.2f); audioPlay(SFX_POWERUP); }
        break;
      case I_CHARGE_SHRINE:
        if (!it.used) {
          float rate = 1.f / 5.5f * (1 + 0.04f * P.items[IT_WRENCH]);
          if (d < 4.2f) { it.progress += dt * rate; if ((int)(it.animT * 8) % 2 == 0) particles(it.pos + v3(rng.range(-3, 3), 0.2f, rng.range(-3, 3)), 1, Col(90, 220, 255), 2, 0.25f, 0.8f, true, SP_SOFT, -3); }
          else it.progress = std::max(0.f, it.progress - dt * rate * 0.5f);
          if (it.progress >= 1) {
            it.used = true; curInter = (int)i;
            genShrineOffers(it.rarity == R_LEGENDARY);
            state = GS_SHRINE; audioPlay(SFX_SHRINE);
            if (P.items[IT_BEACON]) it.healT = 20;
            if (P.items[IT_GHOST]) for (int k = 0; k < P.items[IT_GHOST]; k++) { Ally a; a.kind = 0; a.pos = it.pos + v3(0, 2, 0); a.life = 12; allies.push_back(a); }
          }
        } else if (it.healT > 0) {
          it.healT -= dt;
          if (d < 5) heal(P.st[S_MAXHP] * 0.03f * dt * P.items[IT_BEACON], true);
        }
        break;
      case I_ALTAR:
        if (!it.used) {
          if (d < 4.f) it.progress += dt / 4.f;
          if (it.progress >= 1) { it.used = true; particles(it.pos + v3(0, 1, 0), 30, Col(255, 80, 80), 8, 0.5f, 0.8f); audioPlay(SFX_SHRINE); }
        }
        break;
      default: break;
    }
  }
  inters.erase(std::remove_if(inters.begin(), inters.end(), [](const Inter& it) { return it.used && (it.type == I_CRYPT_KEY || ((it.type == I_POT || it.type == I_POT_BIG || it.type == I_POT_SILVER || it.type == I_PUMPKIN || it.type == I_TUMBLEWEED))) ; }), inters.end());
}

// ====================================================================== items over time
void Game::updateItems(float dt) {
  Player& p = P;
  auto dec = [&](float& t) { if (t > 0) t -= dt; };
  dec(p.rageT); dec(p.shieldT); dec(p.goldT); dec(p.phantomT); dec(p.cowardT); dec(p.slurpCd); dec(p.thunderCd); dec(p.mirrorCd); dec(p.speedBoiCd); dec(p.speedBoiT); dec(p.zaT); dec(freezeEnemiesT);
  if (p.items[IT_GOLDENSNEAKERS]) { p.sneakAcc += len(v2(p.vel.x, p.vel.z)) * dt * 0.1f * p.items[IT_GOLDENSNEAKERS]; if (p.sneakAcc >= 1) { p.gold += floorf(p.sneakAcc) * p.st[S_GOLDGAIN]; p.sneakAcc -= floorf(p.sneakAcc); } }
  if (p.items[IT_CAMPFIRE] && p.idleT > 1.0f) heal(p.st[S_MAXHP] * 0.025f * p.items[IT_CAMPFIRE] * dt, true);
  if (p.items[IT_BOBDEAD]) {
    p.bobDist += len(v2(p.vel.x, p.vel.z)) * dt;
    if (p.bobDist > 14.f / p.items[IT_BOBDEAD]) { p.bobDist = 0; Ally a; a.kind = 1; a.pos = p.pos; a.life = 8; allies.push_back(a); }
  }
  if (p.items[IT_BOBSLIGHT]) {
    p.bobsLightT += dt;
    float cd = 5.f / (1 + 0.25f * (p.items[IT_BOBSLIGHT] - 1));
    if (p.bobsLightT > cd) {
      p.bobsLightT = 0; float r = 4.f + 0.6f * p.items[IT_BOBSLIGHT];
      Area& a = addArea(); a.kind = AK_LIGHT; a.pos = p.pos; a.radius = r; a.life = a.maxLife = 0.4f; a.color = 0xfff59d;
      areaDamage(p.pos, r, 25.f * hpMult() * 0.35f, SRC_ITEM + IT_BOBSLIGHT, 0.2f, 1.5f);
    }
  }
  if (p.items[IT_SUCKYMAGNET]) { p.suckyT += dt; if (p.suckyT > 30.f / p.items[IT_SUCKYMAGNET]) { p.suckyT = 0; for (auto& q : pickups) if (q.kind == PI_XP) q.magnet = true; } }
  if (p.items[IT_ENERGYCORE]) {
    p.energyT += dt;
    if (p.energyT > 1.6f) {
      p.energyT = 0;
      for (int k = 0; k < p.items[IT_ENERGYCORE]; k++) {
        int t = randomEnemy(p.pos, 16); if (t < 0) break;
        Projectile& pr = addProj(); pr.kind = PK_HOMING; pr.model = M_P_ORB; pr.pos = p.pos + v3(0, 1.6f, 0); pr.target = enemies[t].id; pr.speed = 16; pr.vel = norm(enemies[t].pos - pr.pos) * 16.f; pr.homing = 8; pr.life = pr.maxLife = 2.5f; pr.dmg = 14.f * (1 + stage) ; pr.src = SRC_ITEM + IT_ENERGYCORE; pr.radius = 0.5f; pr.color = 0x40f0ff; pr.procCoef = 0.5f;
      }
    }
  }
  if (p.items[IT_CURSEDDOLL]) {
    p.dollT += dt;
    if (p.dollT > 3.f / p.items[IT_CURSEDDOLL]) {
      p.dollT = 0; int t = randomEnemy(p.pos, 18);
      if (t >= 0) { Enemy& e = enemies[t]; e.curseDps = e.maxHp * 0.30f * (e.rank == RK_BOSS ? 0.02f : e.rank == RK_MINIBOSS ? 0.15f : 1.f); e.curseT = 3.2f; particles(e.pos + v3(0, 1, 0), 8, Col(170, 60, 255), 3, 0.3f, 0.6f); }
    }
  }
  if (p.items[IT_GASMASK]) { int n = 0; forEnemiesNear(p.pos, 10, [&](Enemy& e) { if (e.poisonT > 0) n++; }); p.shrineBonus[S_ARMOR] = p.shrineBonus[S_ARMOR]; float want = std::min(n, 20) * 1.5f * p.items[IT_GASMASK]; static float last = 0; if (fabsf(want - last) > 0.5f) { p.shrineBonus[S_ARMOR] += want - last; p.shrineBonus[S_LIFESTEAL] += (want - last) * 0.5f; last = want; statsDirty = true; } }
  // allies
  for (auto& a : allies) {
    a.t += dt; a.life -= dt; a.atkT -= dt;
    int ti = nearestEnemy(a.pos, 14);
    if (ti >= 0) {
      Enemy& e = enemies[ti]; v3 dir = e.pos + v3(0, 0.8f, 0) - a.pos; float d = len(dir);
      a.vel = lerp3(a.vel, dir / std::max(0.01f, d) * (a.kind == 0 ? 9.f : 7.f), std::min(1.f, dt * 4));
      if (d < 1.2f && a.atkT <= 0) {
        if (a.kind == 0) { hitEnemy(e, 10.f * (1 + stage) * (1 + u()), SRC_ITEM + IT_GHOST, 0.5f, 0, a.pos, 0.5f); a.atkT = 0.5f; }
        else if (a.kind == 2) { hitEnemy(e, 14.f * P.st[S_DAMAGE] * (1 + 0.06f * P.level), SRC_COUNT - 1, 0.5f, 0, a.pos, 1.f); a.atkT = 0.6f; }
        else { areaDamage(a.pos, 3.f, 30.f * (1 + stage) * (1 + u()) * P.items[IT_BOBDEAD], SRC_ITEM + IT_BOBDEAD, 0.3f, 2.f); particles(a.pos, 20, Col(140, 200, 120), 6, 0.4f, 0.6f); a.life = 0; audioPlay(SFX_EXPLODE); }
      }
    } else { v3 dir = P.pos + v3(0, 1.5f, 0) - a.pos; a.vel = lerp3(a.vel, dir * 1.5f, std::min(1.f, dt * 3)); }
    a.pos += a.vel * dt;
    if (a.kind >= 1) a.pos.y = world.T.height(a.pos.x, a.pos.z);
  }
  allies.erase(std::remove_if(allies.begin(), allies.end(), [](const Ally& a) { return a.life <= 0; }), allies.end());
}

void Game::updateFx(float dt) {
  for (auto& p : parts) {
    p.life -= dt; p.vel.y += p.grav * dt; p.pos += p.vel * dt; p.vel *= 1 - dt * 1.5f; p.rot += p.rotV * dt;
  }
  parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
  for (auto& t : texts) { t.t += dt; t.pos.y += dt * 1.2f; }
  texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatText& t) { return t.t > t.life; }), texts.end());
}

// ====================================================================== autopilot (tests, screenshots, menu background)
void Game::updateAutopilot(float dt) {
  apT -= dt;
  in.camManual = false; in.camDX = in.camDY = 0;
  if (apT <= 0) {
    apT = 0.15f;
    // goal selection
    v3 goal; bool haveGoal = false;
    int best = -1; float bd = 1e9;
    for (size_t i = 0; i < inters.size(); i++) {
      const Inter& it = inters[i]; if (it.used) continue;
      bool want = it.type == I_CHARGE_SHRINE || it.type == I_CHEST_GOLD || (it.type == I_CHEST && P.gold >= chestCost()) || it.type == I_MOAI || it.type == I_POT || it.type == I_POT_BIG || it.type == I_CRYPT_KEY || it.type == I_PUMPKIN || it.type == I_ALTAR;
      if (it.type == I_BOSS_PORTAL && (u() > 0.72f || portalOpen) && (cfg.map != MAP_GRAVEYARD || cryptKeys >= 4 || portalOpen) && (!bossSummoned || portalOpen)) want = true;
      if (!want) continue;
      float d = dist2XZ(it.pos, P.pos) * (it.type == I_BOSS_PORTAL ? 0.004f : it.type == I_CHARGE_SHRINE ? 0.6f : it.type == I_CRYPT_KEY ? 0.2f : 1.f);
      if (d < bd) { bd = d; best = (int)i; }
    }
    // xp cluster nearby
    v3 xpc; float xw = 0;
    for (auto& p : pickups) if (!p.dead && !p.magnet && dist2XZ(p.pos, P.pos) < 18 * 18) { xpc += p.pos * p.value; xw += p.value; }
    if (best >= 0) { goal = inters[best].pos; haveGoal = true; apInter = best; }
    bool portalGoal = best >= 0 && inters[best].type == I_BOSS_PORTAL;
    if (bossSummoned && !portalOpen) { for (auto& e : enemies) if (e.alive && e.rank == RK_BOSS) { goal = e.pos + norm(P.pos - e.pos) * 9.f; haveGoal = true; break; } }
    else if (xw > 8 && !portalGoal && (!haveGoal || bd > 25 * 25)) { goal = xpc / xw; haveGoal = true; }
    bool shrineHold = best >= 0 && inters[best].type == I_CHARGE_SHRINE && dist2XZ(inters[best].pos, P.pos) < 3.5f * 3.5f;
    bool altarHold = best >= 0 && inters[best].type == I_ALTAR && dist2XZ(inters[best].pos, P.pos) < 3.f * 3.f;
    float bestScore = -1e9; v3 bestDir;
    float H = world.playableHalf() - 6;
    for (int k = 0; k < 16; k++) {
      float a = k * TAU / 16; v3 d(cosf(a), 0, sinf(a));
      float score = 0;
      if (haveGoal) { v3 g = goal - P.pos; g.y = 0; float gl = len(g); if (gl > 0.5f) score += dot(d, g / gl) * (shrineHold || altarHold ? 0.3f : 1.3f); }
      else score += dot(d, v3(cosf(realTime * 0.07f), 0, sinf(realTime * 0.07f))) * 0.6f;
      float danger = 0;
      forEnemiesNear(P.pos, 13, [&](Enemy& e) {
        v3 to = e.pos - P.pos; to.y = 0; float l = std::max(0.6f, len(to)); float al = dot(d, to / l);
        float w = (e.rank ? 4.f : 1.f) * (e.elite ? 2.f : 1.f);
        if (al > 0.2f) danger += w * al / (l * 0.35f);
      });
      for (auto& pr : projs) if (pr.enemy && !pr.dead) { v3 to = pr.pos - P.pos; to.y = 0; float l = std::max(0.6f, len(to)); if (l < 10) { float al = dot(d, to / l); if (al > 0) danger += 2.f * al / l; } }
      for (auto& ar : areas) if (ar.kind == AK_TELEGRAPH && !ar.dead) { v3 to = ar.pos - P.pos; to.y = 0; float l = len(to); if (l < ar.radius + 3) danger += 4.f * std::max(0.f, dot(d, to / std::max(0.5f, l))) + (l < ar.radius ? 3.f : 0); }
      v3 np = P.pos + d * 6.f;
      if (fabsf(np.x) > H || fabsf(np.z) > H) score -= 3.f;
      score -= danger * 1.6f;
      if (score > bestScore) { bestScore = score; bestDir = d; }
    }
    float cs = cosf(camYaw), sn = sinf(camYaw);
    v3 fwd(sn, 0, cs), right(-cs, 0, sn);
    apDir = v2(dot(bestDir, right), dot(bestDir, fwd));
    if (shrineHold || altarHold) { int nearE = 0; forEnemiesNear(P.pos, 4, [&](Enemy&) { nearE++; }); if (nearE < 3) apDir = apDir * 0.25f; }
    int close = 0; forEnemiesNear(P.pos, 2.8f, [&](Enemy&) { close++; });
    if (close >= 2 && rng.chance(0.5f)) in.jump = true;
  }
  in.move = apDir;
  int ni = nearestInter(2.6f);
  if (ni >= 0 && rng.chance(0.25f)) { const Inter& it = inters[ni]; bool portalOk = it.type != I_BOSS_PORTAL || portalOpen || u() > 0.72f; if (portalOk && it.type != I_SHADY && it.type != I_MICROWAVE && it.type != I_GREED_SHRINE && it.type != I_CURSED_SHRINE && it.type != I_CHALLENGE_SHRINE) interact(ni); }
  if (state == GS_SHOP || state == GS_MICROWAVE) state = GS_PLAY;
}
