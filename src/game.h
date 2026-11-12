// Game simulation
#pragma once
#include "base.h"
#include "data.h"
#include "world.h"
#include "models.h"

struct Renderer;

constexpr int MAX_WEAPONS = 4, MAX_TOMES = 4;
enum { SRC_ITEM = 40 }; // damage source ids: weapon slot 0..7, SRC_ITEM+itemId for items, 39 thorns/other
constexpr int SRC_COUNT = SRC_ITEM + IT_COUNT + 1;

struct WeaponInst {
  int def = -1, level = 0;
  float g[U_COUNT] = {0};       // accumulated upgrade gains
  float cd = 0, t = 0, burstT = 0; int burst = 0; float angle = 0; int target = -1; float charge = 0; bool active = false;
  double dmgDealt = 0; int kills = 0; float trailT = 0; int elitesHit = 0; float noodleT = 0;
};
struct TomeInst { int def = -1, level = 0; float value = 0; };

struct Enemy {
  v3 pos, vel, knock; float yaw = 0;
  float hp = 1, maxHp = 1, dmg = 1, speed = 1, radius = 0.5f, scale = 1;
  int type = 0, id = 0; u8 rank = 0; bool elite = false, challenge = false, alive = true, flying = false, invuln = false;
  float animT = 0, animPhase = 0, clipT = 0; u8 clip = CL_MOVE;
  float atkCd = 0, hitFlash = 0, spawnT = 0, deathT = 0;
  float freezeT = 0, slowT = 0, stunT = 0, poisonDps = 0, poisonT = 0, burnDps = 0, burnT = 0, curseDps = 0, curseT = 0, airT = 0; int bleed = 0; float bleedT = 0;
  float hitCd[8] = {0};
  float aiT = 0, aiT2 = 0; int aiState = 0; v3 aiVec; float hover = 1.4f;
  int bossIdx = -1; float phase = 0;
};
enum ProjKind { PK_STRAIGHT, PK_HOMING, PK_BOUNCE, PK_ORBIT, PK_SPIRAL, PK_BOOMERANG, PK_LOB, PK_ENEMY, PK_ENEMY_LOB, PK_WAVE, PK_SHIELD, PK_SOUL, PK_ROCK_ORBIT, PK_BOB };
struct Projectile {
  v3 pos, vel, origin; float life = 1, maxLife = 1, dmg = 1, radius = 0.4f, size = 1; int model = 0; u8 kind = 0; bool enemy = false; int src = 0;
  int pierce = 0, bounces = 0; float explode = 0, knock = 0, homing = 0, speed = 10, range = 20, crit = 0, procCoef = 1; int target = -1;
  int hits[10]; int nhits = 0; float spin = 0, t = 0, aux = 0, aux2 = 0; v3 aux3; u32 color = 0xffffff; bool dead = false; float slow = 0;
};
enum AreaKind { AK_FIRE, AK_POISON, AK_BLACKHOLE, AK_TORNADO, AK_MINE, AK_EXPLOSION, AK_SHOCKWAVE, AK_TELEGRAPH, AK_HEALZONE, AK_FROST, AK_DRAGONFIRE, AK_BOSSFIRE, AK_ALTAR_FX, AK_LIGHT };
struct Area {
  int kind = 0; v3 pos, vel; float radius = 1, life = 1, maxLife = 1, dmg = 0, tick = 0.5f, tickT = 0; int src = 0; bool enemy = false; u32 color = 0xffffff; float pull = 0, knock = 0, aux = 0; int target = -1; bool dead = false; float delay = 0;
};
enum PickKind { PI_XP, PI_GOLD, PI_SILVER, PI_BORGAR, PI_POWERUP };
struct Pickup { int kind = 0, sub = 0; v3 pos, vel; float value = 1, t = 0; bool magnet = false, dead = false; };
struct Inter {
  int type = 0; v3 pos; float yaw = 0; int rarity = 0; bool used = false, open = false; float progress = 0, animT = 0, openT = 0; int cost = 0, uses = 0; std::vector<int> stock; std::vector<bool> sold; float healT = 0; int challengeLeft = 0;
};
struct Particle { v3 pos, vel; float life = 1, maxLife = 1, size = 0.3f, size1 = 0; Col c; u8 sprite = 0; bool add = true; float grav = 0, rot = 0, rotV = 0; };
struct FloatText { v3 pos; float t = 0, life = 0.8f; std::string s; Col c; float scale = 1; };
struct Ally { int kind = 0; v3 pos, vel; float life = 10, t = 0, atkT = 0; int target = -1; };
struct Banner { std::string text, sub; float t = 0, life = 3; Col c; };

struct Offer {
  int kind = 0; // 0 new weapon, 1 weapon upgrade, 2 new tome, 3 tome upgrade, 4 stat, 5 item
  int id = 0, rarity = 0, slot = -1; std::vector<UpGain> gains; float value = 0; int price = 0;
};

enum GState { GS_PLAY, GS_LEVELUP, GS_CHEST, GS_SHRINE, GS_SHOP, GS_MICROWAVE, GS_MOAI, GS_PAUSE, GS_DEAD, GS_VICTORY, GS_TRANSITION };

struct RunConfig {
  int map = MAP_FOREST, tier = 0, character = C_FOX;
  float stageMinutes = 15; bool scaleStages = true; int swarmMode = 0; // 0 auto when timer ends, 1 only after the boss is defeated
  u32 seed = 1; int rerolls = 2, skips = 2, banishes = 2; int maxEnemies = 300; bool autoBhop = false; bool instantChests = false;
  bool godMode = false;
};

struct Input { v2 move; bool jump = false, jumpHeld = false, slide = false, interact = false; float camDX = 0, camDY = 0; bool camManual = false; };

struct Player {
  int ch = 0; v3 pos, vel; float yaw = 0; bool onGround = true, sliding = false; int jumps = 0; float airMaxY = 0, landT = 0, coyote = 0;
  float hp = 100, shield = 0, iframes = 0, lastHitT = -99; int level = 1; float xp = 0, xpNext = 10; double gold = 0; int silver = 0, kills = 0;
  float st[S_COUNT]; float shrineBonus[S_COUNT] = {0}; float chaos[S_COUNT] = {0};
  std::vector<WeaponInst> weapons; TomeInst tomes[MAX_TOMES]; int ntomes = 0; int items[IT_COUNT] = {0};
  float speedDemon = 0, idleT = 0, distMoved = 0, bobDist = 0, bobsLightT = 0, suckyT = 0, energyT = 0, dollT = 0, slurpCd = 0, thunderCd = 0, mirrorCd = 0, speedBoiCd = 0, speedBoiT = 0;
  int chestsOpened = 0; float demonicBlood = 0; int demonicKills = 0, joesExec = 0; float rageT = 0, shieldT = 0, goldT = 0, phantomT = 0, cowardT = 0, zaT = 0; bool nextDouble = false, nextCrit = false;
  float diceCrit = 0, bloodMagicHp = 0, greedDiff = 0; int rerolls = 2, skips = 2, banishes = 2; std::vector<int> banishedW, banishedT;
  float regenAcc = 0, sneakAcc = 0;
};

struct Game {
  RunConfig cfg; World world; Player P; Input in;
  std::vector<Enemy> enemies; std::vector<Projectile> projs; std::vector<Area> areas; std::vector<Pickup> pickups; std::vector<Inter> inters;
  std::vector<Particle> parts; std::vector<FloatText> texts; std::vector<Ally> allies; std::vector<Banner> banners;
  GState state = GS_PLAY, prevState = GS_PLAY; bool running = false;
  int stage = 0, numStages = 1; float stageTime = 0, stageDur = 900, runTime = 0, realTime = 0;
  bool bossSummoned = false, bossDefeated = false, portalOpen = false, finalSwarm = false, finalArena = false; float finalSwarmT = 0; int bossCount = 1, bossesAlive = 0, cryptKeys = 0;
  int eventsDone = 0; float spawnAcc = 0; int chestsBought = 0, firstEliteDone = 0, cursedUsed = 0; int pumpkinBonus = 0, beaconBonus = 0;
  float timeScale = 1; float freezeEnemiesT = 0; float shake = 0; float hitStop = 0;
  std::vector<Offer> offers; int pendingLevels = 0; int curInter = -1; int chestItem = -1, chestRarity = 0; float chestAnim = 0;
  double dmgBySrc[SRC_COUNT] = {0}; int nextId = 1; Rng rng; float camYaw = 0, camPitch = 0.38f, camDist = 7.5f; float camManualT = 0; v3 camEye, camTgt;
  bool autoPilot = false; float apT = 0; v2 apDir; int apInter = -1; float transitionT = 0; float deathT = 0;
  std::vector<int> egHead, egNext; float egCell = 2.5f; int egN = 0; float egOrigin = 0;
  float statsDirtyT = 0; bool statsDirty = true; int animFps = 12; float shakeSetting = 1; bool dmgNumbers = true; int totalSilverEarned = 0;
  float swarmWarnT = 0; int lastKillMilestone = 0; float announceT = 0; float eliteT = 0; std::string lastHitBy; float lastHitDmg = 0;
  float fovDeg = 62; bool shadowsOn = true; int worldGen = 0; float playerAnimT = 0, lastRenderT = 0; float stormK = 0; int particleLevel = 2;

  void start(const RunConfig& c);
  void loadStage(int s);
  void update(float dt);
  void render(Renderer& R);
  // systems
  void updatePlayer(float dt);
  void updateCamera(float dt);
  void updateSpawning(float dt);
  void updateEnemies(float dt);
  void updateWeapons(float dt);
  void updateProjectiles(float dt);
  void updateAreas(float dt);
  void updatePickups(float dt);
  void updateInters(float dt);
  void updateItems(float dt);
  void updateFx(float dt);
  void updateAutopilot(float dt);
  void recomputeStats();
  float u() const { return stageDur > 0 ? stageTime / stageDur : 0; }
  float hpMult() const; float dmgMult() const;
  // entities
  Enemy* spawnEnemy(int type, v3 pos, bool elite = false);
  v3 spawnRingPos(float rmin, float rmax);
  void killEnemy(Enemy& e, int src);
  void hitEnemy(Enemy& e, float dmg, int src, float procCoef, float critAdd, v3 from, float knock, bool canCrit = true, bool noProc = false);
  void areaDamage(v3 c, float r, float dmg, int src, float procCoef, float knock, int exceptId = -1, bool canCrit = true);
  void damagePlayer(float dmg, Enemy* src, bool canDodge = true, bool kevin = false);
  void heal(float amt, bool fromRegen = false);
  void addXp(float v);
  void dropXp(v3 pos, float v);
  void dropPickup(int kind, int sub, v3 pos, float value);
  Projectile& addProj(); Area& addArea();
  void particles(v3 pos, int n, Col c, float speed, float size, float life, bool add = true, int sprite = 1, float grav = 0);
  void floatText(v3 pos, const std::string& s, Col c, float scale = 1);
  void banner(const std::string& t, const std::string& sub, Col c, float life = 3);
  void addItem(int id, int n = 1);
  void addWeapon(int def);
  void applyOffer(const Offer& o);
  void genLevelOffers();
  void genShrineOffers(bool golden);
  int rollRarity(float bonus = 0);
  int rollItem(int rarity);
  int chestCost() const;
  void openChest(Inter& it, bool free);
  void interact(int idx);
  void summonBoss(Inter& portal);
  void bossDied(Enemy& e);
  void nextStage();
  void startFinalArena();
  void lightning(v3 at, float dmg, int src, float r, float stun);
  int nearestEnemy(v3 p, float maxR, int exclude = -1, bool needLos = false) const;
  int randomEnemy(v3 p, float maxR);
  void buildEnemyGrid();
  template <class F> void forEnemiesNear(v3 p, float r, F f);
  void bossAI(Enemy& e, float dt);
  void minibossAI(Enemy& e, float dt);
  void fireWeapon(WeaponInst& w, int slot, float dt);
  int nearestInter(float maxD) const;
  float weaponDmg(const WeaponInst& w) const;
  int weaponMaxLevel() const { return 40 + std::max(0, P.items[IT_POT] > 0 ? 10 + (P.items[IT_POT] - 1) * 2 : 0); }
  int tomeMaxLevel() const { return 99 + (P.items[IT_WIZARDSHAT] > 0 ? 10 + (P.items[IT_WIZARDSHAT] - 1) * 2 : 0); }
  float pickupRadius() const { return 3.6f * P.st[S_PICKUP]; }
  bool playerAirborne() const { return !P.onGround; }
  std::string interLabel(int idx) const;
  int interCost(int idx) const;
  float stageDuration(int s) const;
};

template <class F> void Game::forEnemiesNear(v3 p, float r, F f) {
  if (egHead.empty()) return;
  float rr = r + 3.f;
  int x0 = clampi((int)((p.x - rr - egOrigin) / egCell), 0, egN - 1), x1 = clampi((int)((p.x + rr - egOrigin) / egCell), 0, egN - 1);
  int z0 = clampi((int)((p.z - rr - egOrigin) / egCell), 0, egN - 1), z1 = clampi((int)((p.z + rr - egOrigin) / egCell), 0, egN - 1);
  for (int z = z0; z <= z1; z++)
    for (int x = x0; x <= x1; x++)
      for (int i = egHead[z * egN + x]; i >= 0; i = egNext[i]) {
        if (i >= (int)enemies.size()) break;
        Enemy& e = enemies[i];
        if (!e.alive) continue;
        float q = r + e.radius;
        if (dist2XZ(e.pos, p) <= q * q && fabsf(e.pos.y - p.y) < 3.5f + e.radius * 2) f(e);
      }
}

std::string offerTitle(const Offer& o, const Game& g);
std::string offerDesc(const Offer& o, const Game& g);
int offerIcon(const Offer& o);
