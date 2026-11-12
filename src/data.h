// Game catalogs (weapons, tomes, items, enemies, characters, maps)
#pragma once
#include "base.h"
#include "art.h"

enum Rarity { R_COMMON, R_UNCOMMON, R_RARE, R_EPIC, R_LEGENDARY, R_COUNT };
extern const char* kRarityName[R_COUNT];
extern const u32 kRarityColor[R_COUNT];
extern const float kRarityMult[R_COUNT];

enum StatId {
  S_MAXHP, S_REGEN, S_OVERHEAL, S_SHIELD, S_ARMOR, S_EVASION, S_LIFESTEAL, S_THORNS, S_DAMAGE, S_CRIT, S_CRITDMG,
  S_ATKSPD, S_PROJ, S_BOUNCE, S_SIZE, S_PROJSPD, S_DURATION, S_ELITEDMG, S_KNOCKBACK, S_MOVESPD, S_JUMPS, S_JUMPH,
  S_LUCK, S_DIFFICULTY, S_PICKUP, S_XPGAIN, S_GOLDGAIN, S_SILVERGAIN, S_ELITESPAWN, S_POWERUP, S_POWERUPCH, S_FALLRED,
  S_COUNT
};
enum StatFmt { SF_FLAT, SF_PCT, SF_MULT, SF_PERMIN };
struct StatDef { const char* name; StatFmt fmt; float base; float shrine; };
extern const StatDef kStats[S_COUNT];
std::string statValueStr(int stat, float v, bool sign = true);

// ---------------- weapons ----------------
enum WeaponId {
  W_FIRESTAFF, W_SWORD, W_REVOLVER, W_BONE, W_BOW, W_AURA, W_AXE, W_AEGIS, W_BANANARANG, W_BLACKHOLE,
  W_BLOODMAGIC, W_CHUNKERS, W_CORRUPTED, W_DEXECUTIONER, W_DICE, W_DRAGONBREATH, W_FLAMEWALKER, W_FROSTWALKER,
  W_HEROSWORD, W_KATANA, W_LIGHTNING, W_MINES, W_POISONFLASK, W_SCYTHE, W_SHOTGUN, W_ROCKET, W_SNIPER,
  W_SPACENOODLE, W_TORNADO, W_WIRELESS, W_COUNT
};
enum WeaponBeh { WB_PROJ, WB_MELEE, WB_ORBIT, WB_AURA, WB_TRAIL, WB_MINES, WB_FLASK, WB_BLACKHOLE, WB_TORNADO, WB_BOOMERANG, WB_LIGHTNING, WB_BEAM, WB_CONE, WB_SPIN, WB_AEGIS, WB_SPIRAL };
enum UpStat { U_DMG, U_PROJ, U_SIZE, U_PROJSPD, U_CRIT, U_KNOCK, U_DUR, U_ATKSPD, U_BOUNCE, U_COUNT };
extern const char* kUpStatName[U_COUNT];
struct UpGain { u8 stat; float v; };
struct WeaponDef {
  const char* name; const char* desc; u8 beh;
  float dmg; int proj; float cd; float range; float speed; float size; float dur; int pierce; int bounce; float explode; float knock;
  std::vector<UpGain> pool;
  IconRecipe icon; u32 color; int projModel; bool manualAim;
};
extern std::vector<WeaponDef> gWeapons;

// ---------------- tomes ----------------
enum TomeId {
  T_DAMAGE, T_COOLDOWN, T_QUANTITY, T_PRECISION, T_SIZE, T_HP, T_ARMOR, T_SHIELD, T_REGEN, T_EVASION, T_BLOODY,
  T_THORNS, T_DURATION, T_ATTRACTION, T_AGILITY, T_KNOCKBACK, T_PROJSPEED, T_XP, T_GOLDEN, T_SILVER, T_LUCK,
  T_CURSED, T_CHAOS, T_COUNT
};
struct TomeDef { const char* name; int stat; float per; const char* desc; IconRecipe icon; };
extern std::vector<TomeDef> gTomes;

// ---------------- items ----------------
enum ItemId {
  IT_ANVIL, IT_BACKPACK, IT_BATTERY, IT_BEACON, IT_BEEFYRING, IT_BEER, IT_BIGBONK, IT_BLOODYCLEAVER, IT_BOBDEAD, IT_BOBSLIGHT,
  IT_BORGAR, IT_BOSSBUSTER, IT_BRASSKNUCKLES, IT_CACTUS, IT_CAMPFIRE, IT_CHONKPLATE, IT_CLOVER, IT_COWARDSCLOAK, IT_CCGREEN, IT_CCRED,
  IT_CURSEDDOLL, IT_CURSEDGRABBIES, IT_DEMONICBLADE, IT_DEMONICBLOOD, IT_DEMONICSOUL, IT_DRAGONFIRE, IT_EAGLECLAW, IT_ECHOSHARD, IT_ELECTRICPLUG, IT_ENERGYCORE,
  IT_FEATHERS, IT_FORBIDDENJUICE, IT_GAMERGOGGLES, IT_GASMASK, IT_GHOST, IT_GIANTFORK, IT_GOLDENGLOVE, IT_GOLDENRING, IT_GOLDENSHIELD, IT_GOLDENSNEAKERS,
  IT_GRANDMATONIC, IT_GYMSAUCE, IT_HOLYBOOK, IT_ICECUBE, IT_ICECRYSTAL, IT_IDLEJUICE, IT_JOESDAGGER, IT_KEVIN, IT_KEY, IT_LEECHINGCRYSTAL,
  IT_LIGHTNINGORB, IT_MEDKIT, IT_MIRROR, IT_MOLDYCHEESE, IT_MOLDYGLOVES, IT_OATS, IT_OLDMASK, IT_OPLAMP, IT_PHANTOMSHROUD, IT_POT,
  IT_POWERGLOVES, IT_PUMPKIN, IT_QUINSMASK, IT_SCARF, IT_SHATTEREDKNOWLEDGE, IT_SKULEG, IT_SLIPPERYRING, IT_SLURPGLOVES, IT_SLUTTYCANNON, IT_SNEK,
  IT_SOULHARVESTER, IT_SPEEDBOI, IT_SPICYMEATBALL, IT_SPIKYSHIELD, IT_SUCKYMAGNET, IT_TACTICALGLASSES, IT_THUNDERMITTS, IT_TIMEBRACELET, IT_TOXICBARREL, IT_TURBOSKATES,
  IT_TURBOSOCKS, IT_UNSTABLETRANSFUSION, IT_WIZARDSHAT, IT_WRENCH, IT_ZAWARUDO, IT_GOLDENKEY, IT_COUNT
};
struct StatMod { u8 stat; float v; };
struct ItemDef { const char* name; u8 rarity; const char* desc; IconRecipe icon; int maxStack; std::vector<StatMod> stats; bool inPool; };
extern std::vector<ItemDef> gItems;

// ---------------- powerups ----------------
enum PowerupId { PU_SHIELD, PU_CLOCK, PU_GOLD, PU_RAGE, PU_MAGNET, PU_NUKE, PU_COUNT };
struct PowerupDef { const char* name; const char* desc; IconRecipe icon; float dur; u32 color; };
extern std::vector<PowerupDef> gPowerups;

// ---------------- maps ----------------
enum MapId { MAP_FOREST, MAP_DESERT, MAP_GRAVEYARD, MAP_COUNT };
struct Palette {
  u32 skyTop, skyHorizon, fog, sun, ambientSky, ambientGround, ground1, ground2, ground3, foliage1, foliage2, trunk, rock, accent;
  float fogDensity; float sunElev; bool stars; float sunAngle;
};
struct StageDef { const char* name; Palette pal; };
struct MapDef { const char* name; const char* desc; StageDef tiers[3]; };
extern std::vector<MapDef> gMaps;

// ---------------- enemies ----------------
enum EnemyId {
  E_SLIME, E_GOBLIN, E_BEE, E_GOBLIN_STRONG, E_BOOMER, E_GOLEM, E_GOBLIN_TANK, E_GHOUL, E_SKELETON, E_SKELETON_ARMORED,
  E_TUMBLEBONE, E_MUMMY, E_WISP, E_CACTUS, E_SCORPION, E_SKELETON_DUSTY, E_SKELETON_ARMORED_DUSTY, E_SCORPION_BIG, E_MUMMY_TANK, E_MUMMY_ANCIENT,
  E_GHOST, E_HEAD, E_ZOMBIE, E_GHOST_GREATER, E_GHOST_PURPLE, E_GHOST_RED, E_SKELETON_GOLD, E_SKELETON_XP,
  E_MB_STONEGOLEM, E_MB_CHUNKHAM, E_MB_SANDGOLEM, E_MB_SCORPIONUSSY, E_MB_GHOSTHAM, E_MB_SKELETONKING,
  E_B_LILBARK, E_B_CHADBARK, E_B_BARKVADER, E_B_LILANUBIS, E_B_ANUBRUH, E_B_JUGEANUBIS, E_B_BIGBOB, E_B_GHOSTKING,
  E_SWARMGHOST, E_COUNT
};
enum EnemyBeh { EB_CHASE, EB_FLY, EB_EXPLODE, EB_SHOOT, EB_ROLL, EB_POISON, EB_MB_SLAM, EB_MB_CHARGE, EB_MB_THROW, EB_MB_SPIT, EB_MB_RING, EB_MB_SUMMON, EB_BOSS };
enum EnemyRank { RK_NORMAL, RK_MINIBOSS, RK_BOSS };
struct EnemyDef {
  const char* name; u8 maps; float hp, dmg, spd; u8 beh; bool flying; int model; float radius; float scale; float xp; u8 rank; u32 tint;
};
extern std::vector<EnemyDef> gEnemies;
struct SpawnEntry { int enemy; float uStart; float weight; };
extern std::vector<SpawnEntry> gSpawnTables[MAP_COUNT];
extern int gMinibosses[MAP_COUNT][2];
extern int gBosses[MAP_COUNT][3];

// ---------------- characters ----------------
enum CharId { C_FOX, C_NINJA, C_CALCIUM, C_MONKE, C_OGRE, C_COUNT };
struct CharDef { const char* name; int weapon; const char* passive; const char* passiveDesc; std::vector<StatMod> stats; int model; IconRecipe icon; const char* blurb; };
extern std::vector<CharDef> gChars;

// ---------------- interactables ----------------
enum InterId {
  I_CHEST, I_CHEST_GOLD, I_CHARGE_SHRINE, I_GREED_SHRINE, I_CURSED_SHRINE, I_CHALLENGE_SHRINE, I_MOAI, I_SHADY,
  I_MICROWAVE, I_SUCC_SHRINE, I_BOSS_PORTAL, I_EXIT_PORTAL, I_POT, I_POT_BIG, I_POT_SILVER, I_PUMPKIN, I_TUMBLEWEED,
  I_CRYPT_KEY, I_ALTAR, I_COUNT
};
struct InterDef { const char* name; const char* desc; IconRecipe icon; };
extern std::vector<InterDef> gInters;

// icon ids in atlas
enum { ICON_WEAPON0 = 0, ICON_TOME0 = 32, ICON_ITEM0 = 64, ICON_PU0 = 160, ICON_INTER0 = 176, ICON_UI0 = 200 };
enum UIIcon {
  UI_CLOCK, UI_SILVER, UI_GOLD, UI_SKULL, UI_HEART, UI_SHIELD, UI_STAR, UI_PAUSE, UI_MAP, UI_JUMP, UI_SLIDE, UI_INTERACT,
  UI_GEAR, UI_CROWN, UI_REROLL, UI_SKIP, UI_BANISH, UI_TROPHY, UI_LOCK, UI_XP, UI_HOURGLASS, UI_PORTAL, UI_SWORD, UI_ARROWUP,
  UI_CHAR0, UI_COUNT = UI_CHAR0 + C_COUNT
};
std::vector<IconRecipe> buildIconList();
void dataInit();
