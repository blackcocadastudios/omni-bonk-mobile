#include "data.h"
#include "models.h"

const char* kRarityName[R_COUNT] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};
const u32 kRarityColor[R_COUNT] = {0xd9dde3, 0x6ee26e, 0x4aa8ff, 0xc26bff, 0xffc83a};
const float kRarityMult[R_COUNT] = {1.0f, 1.2f, 1.4f, 1.6f, 2.0f};
const char* kUpStatName[U_COUNT] = {"Damage", "Projectiles", "Size", "Proj. Speed", "Crit", "Knockback", "Duration", "Attack Speed", "Ricochets"};

const StatDef kStats[S_COUNT] = {
  {"Max HP", SF_FLAT, 100, 10}, {"Regeneration", SF_PERMIN, 0, 15}, {"Overheal", SF_PCT, 0, 6}, {"Shield", SF_FLAT, 0, 10},
  {"Armor", SF_PCT, 0, 4}, {"Evasion", SF_PCT, 0, 4}, {"Lifesteal", SF_PCT, 0, 3}, {"Thorns", SF_FLAT, 0, 5},
  {"Damage", SF_MULT, 1, 0.04f}, {"Crit Chance", SF_PCT, 5, 3}, {"Crit Damage", SF_MULT, 2, 0.10f}, {"Attack Speed", SF_PCT, 0, 4},
  {"Projectiles", SF_FLAT, 0, 0}, {"Ricochets", SF_FLAT, 0, 0}, {"Size", SF_MULT, 1, 0.05f}, {"Proj. Speed", SF_MULT, 1, 0.06f},
  {"Duration", SF_MULT, 1, 0.06f}, {"Elite Damage", SF_MULT, 1, 0.06f}, {"Knockback", SF_MULT, 1, 0.08f}, {"Move Speed", SF_MULT, 1, 0.04f},
  {"Extra Jumps", SF_FLAT, 0, 0}, {"Jump Height", SF_MULT, 1, 0.05f}, {"Luck", SF_PCT, 0, 3}, {"Difficulty", SF_PCT, 0, 0},
  {"Pickup Range", SF_MULT, 1, 0.10f}, {"XP Gain", SF_MULT, 1, 0.04f}, {"Gold Gain", SF_MULT, 1, 0.06f}, {"Silver Gain", SF_MULT, 1, 0.06f},
  {"Elite Spawns", SF_MULT, 1, 0}, {"Powerup Mult.", SF_MULT, 1, 0.06f}, {"Powerup Chance", SF_MULT, 1, 0.06f}, {"Fall Dmg. Red.", SF_PCT, 0, 0},
};

std::string statValueStr(int stat, float v, bool sign) {
  const char* sg = (sign && v >= 0) ? "+" : "";
  switch (kStats[stat].fmt) {
    case SF_FLAT: return fabsf(v - roundf(v)) < 0.01f ? fmt("%s%d", sg, (int)roundf(v)) : fmt("%s%.1f", sg, v);
    case SF_PCT: return fabsf(v - roundf(v)) < 0.05f ? fmt("%s%d%%", sg, (int)roundf(v)) : fmt("%s%.1f%%", sg, v);
    case SF_MULT: return fmt("%s%d%%", sg, (int)roundf(v * 100));
    case SF_PERMIN: return fmt("%s%d/min", sg, (int)roundf(v));
  }
  return "";
}

std::vector<WeaponDef> gWeapons;
std::vector<TomeDef> gTomes;
std::vector<ItemDef> gItems;
std::vector<PowerupDef> gPowerups;
std::vector<MapDef> gMaps;
std::vector<EnemyDef> gEnemies;
std::vector<SpawnEntry> gSpawnTables[MAP_COUNT];
int gMinibosses[MAP_COUNT][2];
int gBosses[MAP_COUNT][3];
std::vector<CharDef> gChars;
std::vector<InterDef> gInters;

static IconRecipe IR(u8 s, u32 a = 0xffffff, u32 b = 0x888888, u8 sym = 0) { IconRecipe r; r.shape = s; r.c1 = a; r.c2 = b; r.sym = sym; return r; }

static void initWeapons() {
  auto W = [](const char* n, const char* d, u8 beh, float dmg, int proj, float cd, float range, float speed, float size, float dur, int pierce,
              int bounce, float expl, float knock, std::vector<UpGain> pool, IconRecipe ic, u32 col, int pm, bool manual = false) {
    WeaponDef w{n, d, beh, dmg, proj, cd, range, speed, size, dur, pierce, bounce, expl, knock, pool, ic, col, pm, manual};
    gWeapons.push_back(w);
  };
  W("Firestaff", "Fireballs that explode in an area.", WB_PROJ, 10, 1, 1.15f, 22, 16, 1, 1.6f, 0, 0, 2.2f, 1.0f, {{U_SIZE, .16f}, {U_PROJSPD, .20f}, {U_DMG, 2.5f}, {U_PROJ, 1}}, IR(IS_STAFF, 0xff7a1a, 0xffd23f), 0xff7a1a, M_P_FIREBALL);
  W("Sword", "Arc swings that cut through several enemies.", WB_MELEE, 11, 1, 1.0f, 3.4f, 0, 1, 0.18f, 99, 0, 0, 1.5f, {{U_SIZE, .20f}, {U_DMG, 2}, {U_PROJ, 1}, {U_KNOCK, .25f}}, IR(IS_SWORD, 0xd8e0ea, 0x7a4a2a), 0xeaf2ff, M_FX_SLASH);
  W("Revolver", "Fires a burst of bullets.", WB_PROJ, 5, 6, 1.6f, 26, 42, 1, 0.8f, 0, 0, 0, 0.6f, {{U_PROJSPD, .20f}, {U_DMG, 2.5f}, {U_PROJ, 1}, {U_CRIT, 10}}, IR(IS_REVOLVER), 0xffe9a0, M_P_BULLET);
  W("Bone", "Bones that ricochet between enemies.", WB_PROJ, 11, 1, 1.3f, 20, 17, 1, 1.4f, 0, 3, 0, 0.8f, {{U_DMG, 2.5f}, {U_PROJ, 1}, {U_CRIT, 12}, {U_BOUNCE, 1}}, IR(IS_BONE), 0xf4ecd8, M_P_BONE);
  W("Bow", "Long-range piercing arrows.", WB_PROJ, 9, 1, 1.0f, 30, 36, 1, 1.2f, 3, 0, 0, 0.6f, {{U_PROJSPD, .20f}, {U_DMG, 1.75f}, {U_PROJ, 1}, {U_CRIT, 8}}, IR(IS_BOW, 0xff6b6b), 0xe8d0a0, M_P_ARROW);
  W("Aura", "Continuous damage around you.", WB_AURA, 6, 0, 0.5f, 3.6f, 0, 1, 0, 99, 0, 0, 0.3f, {{U_SIZE, .14f}, {U_DMG, 1.4f}, {U_ATKSPD, 8}}, IR(IS_AURA, 0xb48cff), 0xb48cff, M_FX_RING);
  W("Axe", "Spinning axes that spiral outward.", WB_SPIRAL, 6, 2, 2.4f, 9, 7, 1, 2.2f, 99, 0, 0, 1.2f, {{U_SIZE, .10f}, {U_DUR, .10f}, {U_DMG, 1.5f}, {U_PROJ, 1}}, IR(IS_AXE, 0xc9d1db), 0xc9d1db, M_P_AXE);
  W("Aegis", "Orbiting shields block attacks and release shockwaves.", WB_AEGIS, 10, 2, 3.0f, 2.6f, 2.6f, 1, 0, 99, 0, 3.5f, 2.5f, {{U_SIZE, .15f}, {U_DMG, 2}, {U_PROJ, 1}, {U_KNOCK, .25f}}, IR(IS_SHIELD, 0x4f8fe0, 0xf5c542), 0x8fc4ff, M_P_SHIELD);
  W("Bananarang", "A banana boomerang that goes and returns.", WB_BOOMERANG, 9, 1, 1.4f, 11, 14, 1, 0, 99, 0, 0, 0.8f, {{U_SIZE, .14f}, {U_PROJSPD, .10f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_BANANA, 0xffe14d), 0xffe14d, M_P_BANANA);
  W("Black Hole", "Creates a vortex that sucks in and hurts enemies.", WB_BLACKHOLE, 4.5f, 1, 5.0f, 14, 10, 1, 3.0f, 99, 0, 4.0f, 0, {{U_SIZE, .13f}, {U_DMG, 1.25f}, {U_PROJ, 1}, {U_DUR, .10f}}, IR(IS_HOLE, 0xa34fff, 0x1a1028), 0x8a4fff, M_P_BLACKHOLE);
  W("Blood Magic", "Homing blood orbs; kills can give +Max HP.", WB_PROJ, 9, 1, 1.2f, 18, 15, 1, 2.0f, 0, 0, 0, 0.5f, {{U_SIZE, .15f}, {U_DMG, 1.5f}, {U_PROJ, 1}}, IR(IS_BLOODDROP, 0xd8203a), 0xff3050, M_P_BLOODORB);
  W("Chunkers", "Rocks orbit around you.", WB_ORBIT, 12, 3, 4.0f, 3.2f, 3.2f, 1, 3.0f, 99, 0, 0, 2.0f, {{U_SIZE, .20f}, {U_DMG, 3}, {U_PROJ, 1}, {U_KNOCK, .25f}}, IR(IS_ROCKS, 0x9a8f84), 0xa09080, M_P_ROCK);
  W("Corrupted Sword", "Cuts both sides; more damage at low HP.", WB_MELEE, 12, 1, 1.15f, 3.4f, 0, 1, 0.18f, 99, 0, 0, 1.2f, {{U_SIZE, .15f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_CORRUPT, 0x5b2a86, 0x2b1840), 0xc04fff, M_FX_SLASH);
  W("Dexecutioner", "Piercing blade with a chance to execute.", WB_PROJ, 9, 1, 1.3f, 16, 22, 1.3f, 0.9f, 99, 0, 0, 1.0f, {{U_SIZE, .20f}, {U_DMG, 2}, {U_PROJ, 1}, {U_CRIT, 5}}, IR(IS_DEXE, 0xb0b8c4), 0xff5050, M_P_BLADE);
  W("Dice", "Damage from 1 to 6; rolling a 6 gives a permanent crit.", WB_PROJ, 2.5f, 1, 1.1f, 20, 18, 1, 1.4f, 0, 1, 0, 0.6f, {{U_SIZE, .15f}, {U_DMG, 2}, {U_PROJ, 1}, {U_CRIT, 10}}, IR(IS_DICE), 0xffffff, M_P_DIE);
  W("Dragon's Breath", "A cone of fire in front of you.", WB_CONE, 16, 0, 2.2f, 7.5f, 0, 1, 0.7f, 99, 0, 0, 0.6f, {{U_SIZE, .15f}, {U_DMG, 4}, {U_ATKSPD, 8}}, IR(IS_FLAMECONE), 0xff6a1a, M_FX_CONE);
  W("Flamewalker", "Leaves a trail of fire wherever you go.", WB_TRAIL, 5, 1, 0.28f, 1.3f, 0, 1, 2.6f, 99, 0, 0, 0, {{U_SIZE, .15f}, {U_DUR, .12f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_FIREFOOT, 0xff8c1a, 0x6d4c41), 0xff7a1a, -1);
  W("Frostwalker", "An icy aura that freezes nearby enemies.", WB_AURA, 9, 0, 1.0f, 3.2f, 0, 1, 0, 99, 0, 0, 0, {{U_SIZE, .10f}, {U_DMG, 2}, {U_ATKSPD, 8}}, IR(IS_ICEFOOT, 0x9be7ff, 0x4a6b8a), 0x9be7ff, M_FX_RING);
  W("Hero Sword", "A strike that fires a cutting wave.", WB_MELEE, 11, 1, 1.2f, 3.2f, 26, 1, 0.9f, 99, 0, 0, 1.0f, {{U_SIZE, .15f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_HEROSWORD, 0x4fc3f7, 0x3b2a1a), 0xffe680, M_P_WAVE);
  W("Katana", "Quick slashes at the nearest enemy.", WB_MELEE, 11, 1, 0.85f, 7.0f, 0, 1, 0.15f, 99, 0, 0, 0.8f, {{U_SIZE, .20f}, {U_DMG, 2.2f}, {U_PROJ, 1}, {U_CRIT, 8}}, IR(IS_KATANA, 0, 0x2b2533), 0xfff0b0, M_FX_SLASH);
  W("Lightning Staff", "Lightning strikes enemies and jumps between them.", WB_LIGHTNING, 7, 1, 1.4f, 16, 0, 1, 0, 0, 2, 1.5f, 0.2f, {{U_SIZE, .20f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_BOLT, 0xffe066), 0xbfe8ff, -1);
  W("Mines", "Plants mines that explode on contact.", WB_MINES, 10, 1, 1.8f, 2.6f, 0, 1, 10, 0, 0, 2.6f, 1.5f, {{U_SIZE, .15f}, {U_DMG, 3}, {U_PROJ, 1}}, IR(IS_MINE), 0xff4040, M_P_MINE);
  W("Poison Flask", "Flasks create puddles of poison.", WB_FLASK, 1, 1, 2.4f, 12, 12, 1, 4.0f, 99, 0, 2.6f, 0, {{U_SIZE, .15f}, {U_DMG, .5f}, {U_PROJ, 1}}, IR(IS_FLASK, 0x7bd23f), 0x8fff4f, M_P_FLASK);
  W("Scythe", "Scythe spin; elites charge a powerful strike.", WB_SPIN, 11, 1, 1.8f, 3.8f, 0, 1, 0.35f, 99, 0, 0, 1.2f, {{U_SIZE, .12f}, {U_DMG, 1.5f}, {U_PROJ, 1}, {U_KNOCK, .15f}}, IR(IS_SCYTHE, 0xc9d1db), 0xd0d8ff, M_P_SCYTHE);
  W("Shotgun", "A spread of pellets in the camera direction.", WB_PROJ, 9, 1, 1.7f, 13, 34, 1, 0.45f, 1, 0, 0, 1.6f, {{U_SIZE, .15f}, {U_DMG, 3}, {U_PROJ, 1}, {U_CRIT, 10}}, IR(IS_SHOTGUN), 0xffd27a, M_P_BULLET, true);
  W("Slutty Rocket", "Homing missiles that explode.", WB_PROJ, 11, 1, 1.6f, 26, 15, 1, 3.0f, 0, 0, 2.4f, 1.2f, {{U_PROJSPD, .20f}, {U_DMG, 2}, {U_PROJ, 1}, {U_CRIT, 8}}, IR(IS_ROCKET, 0xff5fa2, 0x7a4aa0), 0xff5fa2, M_P_ROCKET);
  W("Sniper Rifle", "Piercing shot with very high damage (camera aim).", WB_PROJ, 22, 1, 2.0f, 60, 90, 1, 0.8f, 99, 0, 0, 1.5f, {{U_SIZE, .13f}, {U_DMG, 4}, {U_PROJ, 1}}, IR(IS_SNIPER), 0xffffff, M_P_BULLET, true);
  W("Space Noodle", "A cosmic lasso ties you to an enemy and hurts everything in its path.", WB_BEAM, 10, 0, 0.25f, 12, 0, 1, 0, 99, 0, 0, 0, {{U_SIZE, .20f}, {U_DMG, 2}, {U_ATKSPD, 8}}, IR(IS_NOODLE, 0xff7ad8, 0x7ad8ff), 0xff7ad8, M_FX_BEAM);
  W("Tornado", "Tornadoes wander around pushing enemies.", WB_TORNADO, 8, 1, 2.4f, 14, 5, 1, 4.0f, 99, 0, 0, 3.0f, {{U_SIZE, .14f}, {U_PROJSPD, .20f}, {U_DMG, 2}, {U_PROJ, 1}, {U_KNOCK, .20f}}, IR(IS_TORNADO, 0xdfe6ee, 0x9aa3b2), 0xdfe6ee, M_P_TORNADO);
  W("Wireless Dagger", "Smart daggers that chase targets.", WB_PROJ, 9, 1, 1.1f, 20, 22, 1, 2.2f, 0, 0, 0, 0.5f, {{U_PROJSPD, .15f}, {U_DMG, 2}, {U_PROJ, 1}}, IR(IS_DAGGER, 0x4fc3f7, 0x9be7ff), 0x9be7ff, M_P_DAGGER);
}

static void initTomes() {
  auto T = [](const char* n, int st, float per, const char* d, u32 col, u8 sym) { gTomes.push_back({n, st, per, d, IR(IS_BOOK, col, 0xffffff, sym)}); };
  T("Damage Tome", S_DAMAGE, .08f, "+8% damage per level.", 0xc0392b, 1);
  T("Cooldown Tome", S_ATKSPD, 7.5f, "+7.5% attack speed.", 0x2980b9, 2);
  T("Quantity Tome", S_PROJ, 1, "+1 projectile for all weapons.", 0x16a085, 3);
  T("Precision Tome", S_CRIT, 7, "+7% crit chance (above 100% becomes overcrit).", 0x8e44ad, 4);
  T("Size Tome", S_SIZE, .10f, "+10% attack size.", 0xd35400, 5);
  T("HP Tome", S_MAXHP, 25, "+25 max HP.", 0xe74c3c, 6);
  T("Armor Tome", S_ARMOR, 12, "+12% armor (hyperbolic reduction).", 0x7f8c8d, 7);
  T("Shield Tome", S_SHIELD, 25, "+25 regenerating shield.", 0x3498db, 8);
  T("Regen Tome", S_REGEN, 40, "+40 HP per minute.", 0x27ae60, 9);
  T("Evasion Tome", S_EVASION, 10, "+10% evasion.", 0x95a5a6, 10);
  T("Bloody Tome", S_LIFESTEAL, 10, "+10% lifesteal.", 0x8b0000, 11);
  T("Thorns Tome", S_THORNS, 15, "+15 thorns damage.", 0x556b2f, 12);
  T("Duration Tome", S_DURATION, .15f, "+15% effect duration.", 0xb8860b, 13);
  T("Attraction Tome", S_PICKUP, .5f, "+50% pickup range.", 0x9b59b6, 14);
  T("Agility Tome", S_MOVESPD, .15f, "+15% movement speed.", 0x1abc9c, 15);
  T("Knockback Tome", S_KNOCKBACK, .20f, "+20% knockback.", 0x6d4c41, 16);
  T("Projectile Speed Tome", S_PROJSPD, .15f, "+15% projectile speed.", 0x2c3e50, 17);
  T("XP Tome", S_XPGAIN, .07f, "+7% XP gain.", 0x3f51b5, 18);
  T("Golden Tome", S_GOLDGAIN, .12f, "+12% gold gain.", 0xc9a227, 19);
  T("Silver Tome", S_SILVERGAIN, .12f, "+12% silver gain.", 0x8a94a6, 20);
  T("Luck Tome", S_LUCK, 7, "+7% luck.", 0x2e7d32, 21);
  T("Cursed Tome", S_DIFFICULTY, 3.5f, "+3.5% difficulty: more enemies, more XP.", 0x4a148c, 22);
  T("Chaos Tome", -1, 0, "Each level gives a random stat.", 0x212121, 23);
}

static void initItems() {
  auto I = [](const char* n, u8 r, const char* d, IconRecipe ic, int mx = 0, std::vector<StatMod> st = {}, bool pool = true) { gItems.push_back({n, r, d, ic, mx, st, pool}); };
  const u8 C_ = R_COMMON, R_ = R_RARE, E_ = R_EPIC, L_ = R_LEGENDARY;
  I("Anvil", L_, "Weapon upgrades roll +1 stat.", IR(IS_ANVIL, 0x4a4f5a), 1);
  I("Backpack", R_, "+1 projectile for all weapons.", IR(IS_BAG, 0x8d6e63, 0xc9a227), 0, {{S_PROJ, 1}});
  I("Battery", C_, "+8% attack speed.", IR(IS_BOTTLE, 0x43a047, 0x212121), 0, {{S_ATKSPD, 8}});
  I("Beacon", R_, "Charge Shrines become healing zones; +2 shrines in later stages.", IR(IS_BEACON, 0xeeeeee));
  I("Beefy Ring", E_, "+20% damage per 100 Max HP.", IR(IS_RING, 0xd84315, 0xff8a65));
  I("Beer", R_, "+20% damage, -5% Max HP.", IR(IS_MUG), 0, {{S_DAMAGE, .20f}});
  I("Big Bonk", L_, "2% chance of a BONK: 20x damage.", IR(IS_HAMMER, 0x8d6e63));
  I("Bloody Cleaver", L_, "Lifesteal applies Bloodmark; +50% Bloodmark chance.", IR(IS_CLEAVER));
  I("Bob (Dead)", E_, "Every 14 m walked, summons Bob (Dead), who explodes on enemies.", IR(IS_BOB));
  I("Bob's Light", E_, "Every 5s a sphere of light explodes near you.", IR(IS_LIGHTORB, 0xfff59d));
  I("Borgar", C_, "2% chance to drop a Borgar on kill (heals).", IR(IS_BURGER));
  I("Boss Buster", C_, "+15% damage against Elites and Bosses.", IR(IS_CROWN), 0, {{S_ELITEDMG, .15f}});
  I("Brass Knuckles", R_, "+20% damage to nearby enemies.", IR(IS_KNUCKLES, 0xc9a227));
  I("Cactus", C_, "When hit, fires thorns (scales with Thorns).", IR(IS_CACTUS, 0x43a047), 0, {{S_THORNS, 5}});
  I("Campfire", R_, "Standing still heals you.", IR(IS_CAMPFIRE));
  I("Chonkplate", L_, "+20% lifesteal and overheal up to 75% of Max HP.", IR(IS_SHIELD, 0x8d6e63, 0xf5c542), 0, {{S_LIFESTEAL, 20}, {S_OVERHEAL, 75}});
  I("Clover", C_, "+7.5% luck.", IR(IS_CLOVER, 0x43a047), 0, {{S_LUCK, 7.5f}});
  I("Coward's Cloak", R_, "+5% speed; speed burst when hit.", IR(IS_CLOAK, 0x6a1b9a), 0, {{S_MOVESPD, .05f}});
  I("Credit Card (Green)", R_, "+2% luck per chest opened.", IR(IS_CARD, 0x43a047));
  I("Credit Card (Red)", R_, "+2.5% damage per chest opened.", IR(IS_CARD, 0xd32f2f));
  I("Cursed Doll", C_, "Curses enemies, draining 30% of their Max HP per second.", IR(IS_DOLL));
  I("Cursed Grabbies", E_, "+10% difficulty, -20% Max HP; 5% chance of cursed magic.", IR(IS_HAND, 0x7b1fa2), 0, {{S_DIFFICULTY, 10}});
  I("Demonic Blade", R_, "Crits have a 25% chance to heal you.", IR(IS_BLADE, 0xc62828));
  I("Demonic Blood", R_, "+0.5 Max HP per kill (up to 200 stacks).", IR(IS_BLOODDROP, 0x880e4f));
  I("Demonic Soul", E_, "+0.1% damage per kill (up to +100%).", IR(IS_SOUL, 0xd81b60));
  I("Dragonfire", L_, "15% chance to summon dragonfire on hit.", IR(IS_FLAMECONE));
  I("Eagle Claw", L_, "+66% damage to flyers; 8% chance to launch them into the air.", IR(IS_EAGLE, 0x5d4037));
  I("Echo Shard", R_, "12% chance of an extra XP shard on kill.", IR(IS_SHARD, 0x7e57c2));
  I("Electric Plug", R_, "When hit, releases a shock to nearby enemies.", IR(IS_PLUG, 0x37474f));
  I("Energy Core", L_, "Fires energy orbs at enemies.", IR(IS_CORE, 0x00e5ff, 0x00838f));
  I("Feathers", R_, "Higher jumps and +1 extra jump.", IR(IS_FEATHER, 0xeceff1), 0, {{S_JUMPS, 1}, {S_JUMPH, .2f}});
  I("Forbidden Juice", C_, "+10% crit chance.", IR(IS_BOTTLE, 0x9c27b0, 0xffffff), 0, {{S_CRIT, 10}});
  I("Gamer Goggles", E_, "Damage increases as your HP drops.", IR(IS_GOGGLES, 0x212121, 0x00e676));
  I("Gas Mask", E_, "Armor and lifesteal per poisoned enemy.", IR(IS_MASK, 0x546e7a, 0x263238));
  I("Ghost", C_, "Using an interactable summons allied ghosts.", IR(IS_GHOST, 0xeceff1));
  I("Giant Fork", L_, "Crits can become Megacrits.", IR(IS_FORK, 0xb0bec5));
  I("Golden Glove", C_, "+15% gold from kills.", IR(IS_GLOVE, 0xffd23f, 0xc9a227), 0, {{S_GOLDGAIN, .15f}});
  I("Golden Ring", L_, "It shines nicely. It doesn't seem to do anything...", IR(IS_RING, 0xffd23f, 0xffffff));
  I("Golden Shield", R_, "Gain gold when hit.", IR(IS_SHIELD, 0xffd23f, 0xc9a227));
  I("Golden Sneakers", R_, "Generates 0.1 gold per meter traveled.", IR(IS_SHOE, 0xffd23f));
  I("Grandma's Secret Tonic", E_, "Crits have a 50% chance to explode (50% of the damage).", IR(IS_TONIC, 0x8d6e63));
  I("Gym Sauce", C_, "+10% damage.", IR(IS_BOTTLE, 0xd32f2f, 0xffeb3b), 0, {{S_DAMAGE, .10f}});
  I("Holy Book", L_, "+100 Max HP, +50 regen; heals radiate damage.", IR(IS_BOOK, 0xfff8e1, 0xffd23f, 3), 0, {{S_MAXHP, 100}, {S_REGEN, 50}});
  I("Ice Cube", L_, "20% chance of ice damage, which can freeze.", IR(IS_CUBE, 0x81d4fa));
  I("Ice Crystal", C_, "7.5% chance to freeze; executes frozen enemies at low HP.", IR(IS_GEM, 0x4fc3f7));
  I("Idle Juice", R_, "The longer you stand still, the more damage (up to +100%).", IR(IS_BOTTLE, 0x26c6da, 0xffffff));
  I("Joe's Dagger", L_, "1% chance to execute; each execution gives +1% damage.", IR(IS_DAGGER, 0xb71c1c, 0xffcdd2), 10);
  I("Kevin", R_, "Kevin hates you: 25% chance to punch you on hit (does not kill).", IR(IS_RAGE));
  I("Key", C_, "+10% chance for chests to open for free (hyperbolic).", IR(IS_KEY, 0xffd23f));
  I("Leeching Crystal", R_, "+50% Max HP, -50% regeneration.", IR(IS_GEM, 0xc62828));
  I("Lightning Orb", L_, "25% chance of lightning on hit; can stun.", IR(IS_LIGHTORB, 0x5e35b1));
  I("Medkit", C_, "+45 regeneration.", IR(IS_MEDKIT), 0, {{S_REGEN, 45}});
  I("Mirror", E_, "Reflects damage taken and grants brief invulnerability.", IR(IS_MIRROR, 0x81d4fa));
  I("Moldy Cheese", C_, "40% chance to poison on hit.", IR(IS_CHEESE, 0x7cb342));
  I("Moldy Gloves", R_, "Creates poison clouds on hit.", IR(IS_GLOVE, 0x7cb342, 0x33691e));
  I("Oats", C_, "+25 Max HP.", IR(IS_OATS, 0xd84315), 0, {{S_MAXHP, 25}});
  I("Old Mask", C_, "Ancient mask from the graveyard: +5% evasion and +5% armor.", IR(IS_MASK, 0xd7ccc8, 0x5d4037), 0, {{S_EVASION, 5}, {S_ARMOR, 5}});
  I("Overpowered Lamp", L_, "On-hit effects roll 1 extra time.", IR(IS_LAMP, 0xffd23f), 2);
  I("Phantom Shroud", R_, "After dodging: next hit deals 2x and you speed up.", IR(IS_SHROUD, 0x7e57c2));
  I("Pot (stainless steel)", L_, "+10 max level for all weapons.", IR(IS_POT, 0xb0bec5));
  I("Power Gloves", L_, "8% chance of a giant explosion on hit.", IR(IS_GLOVE, 0xff6f00, 0xbf360c));
  I("Pumpkin", R_, "More pots in later stages, with bigger rewards.", IR(IS_PUMPKIN, 0xff8f00));
  I("Quin's Mask", E_, "Thorns damage has a 50% chance to explode.", IR(IS_MASK, 0xffffff, 0xd32f2f));
  I("Scarf", E_, "+33% damage while airborne.", IR(IS_SCARF, 0xd32f2f, 0xffffff));
  I("Shattered Knowledge", E_, "XP shards cut enemies on their way.", IR(IS_SHARD, 0x29b6f6));
  I("Skuleg", C_, "+7% difficulty. More enemies, more fun.", IR(IS_LEG), 0, {{S_DIFFICULTY, 7}});
  I("Slippery Ring", C_, "+15% evasion.", IR(IS_RING, 0x26a69a, 0x80cbc4), 0, {{S_EVASION, 15}});
  I("Slurp Gloves", E_, "Blood magic on hit: area damage and healing (9s cooldown).", IR(IS_GLOVE, 0xad1457, 0x880e4f));
  I("Slutty Cannon", E_, "20% chance to launch missiles on kill.", IR(IS_CANNON, 0x37474f));
  I("Snek", L_, "Applies poison; 5% chance to burst all the poison.", IR(IS_SNAKE, 0x43a047));
  I("Soul Harvester", L_, "Kills summon 2 homing souls.", IR(IS_SOUL, 0x4dd0e1));
  I("Speed Boi", L_, "Below 50% HP: time slows and you deal 2x damage.", IR(IS_SHOE, 0x00b0ff));
  I("Spicy Meatball", L_, "25% chance of an explosion (65% of the damage in an area).", IR(IS_CORE, 0x8d3a1a, 0xff5722));
  I("Spiky Shield", E_, "+10% armor; +2 thorns per 1% armor.", IR(IS_SPIKYSHIELD, 0x6d4c41), 0, {{S_ARMOR, 10}});
  I("Sucky Magnet", L_, "Periodically pulls all XP on the map.", IR(IS_MAGNET, 0xd32f2f));
  I("Tactical Glasses", C_, "+20% damage to enemies above 90% HP.", IR(IS_GLASSES, 0x263238));
  I("Thunder Mitts", R_, "Hits summon lightning (10s cooldown).", IR(IS_GLOVE, 0xffd600, 0x1565c0));
  I("Time Bracelet", C_, "+8% XP gain.", IR(IS_BRACELET, 0xc9a227), 0, {{S_XPGAIN, .08f}});
  I("Toxic Barrel", E_, "Drops toxic waste when hit.", IR(IS_BARREL, 0x7cb342));
  I("Turbo Skates", E_, "Speed becomes attack speed (up to +40%).", IR(IS_SKATE, 0xff4081, 0xffeb3b));
  I("Turbo Socks", C_, "+15% movement speed.", IR(IS_SHOE, 0xeceff1), 0, {{S_MOVESPD, .15f}});
  I("Unstable Transfusion", C_, "27% chance to apply Bloodmark on hit.", IR(IS_FLASK, 0xc62828));
  I("Wizard's Hat", L_, "+10 max level for all tomes.", IR(IS_HAT, 0x3949ab, 0xffd23f));
  I("Wrench", C_, "Charge Shrines charge 4% faster and give +7.5% bonus.", IR(IS_WRENCH, 0x90a4ae));
  I("Za Warudo", E_, "On fatal damage, time stops. Breaks after use.", IR(IS_CLOCK, 0xffd23f));
  I("Golden Key", L_, "Mysterious quest item. Nobody knows what it opens.", IR(IS_GOLDKEY), 1, {}, false);
}

static void initPowerups() {
  gPowerups = {
    {"Shield", "Invulnerable for a few seconds.", IR(IS_SHIELDPU), 6, 0x4fc3f7},
    {"Clock", "Freezes all enemies.", IR(IS_CLOCK, 0x90caf9), 5, 0x90caf9},
    {"Gold", "Extra gold from kills.", IR(IS_COIN, 0xffd23f, 0xc9a227), 10, 0xffd23f},
    {"Rage", "+50% damage and attack speed.", IR(IS_RAGE), 8, 0xff5252},
    {"Magnet", "Pulls all XP on the map.", IR(IS_MAGNET, 0xd32f2f), 0, 0xff8a80},
    {"Bomb", "Massive explosion around you.", IR(IS_NUKE), 0, 0xffab40},
  };
}

static Palette PAL(u32 skyTop, u32 skyHor, u32 fog, u32 sun, u32 aSky, u32 aGnd, u32 g1, u32 g2, u32 g3, u32 f1, u32 f2, u32 trunk, u32 rock, u32 acc, float dens, float elev, bool stars, float sunAng) {
  Palette p{skyTop, skyHor, fog, sun, aSky, aGnd, g1, g2, g3, f1, f2, trunk, rock, acc, dens, elev, stars, sunAng};
  return p;
}
static void initMaps() {
  MapDef forest{"Forest", "Forest with goblins, slimes and ents.", {
    {"Green Forest", PAL(0x4f9fd8, 0xbfe6e8, 0xa6d3d8, 0xfff0d0, 0xa8d0e8, 0x3c4a30, 0x4f8f34, 0x6fa844, 0x8a7448, 0x2a5f34, 0x3f7f38, 0x6b4a32, 0x8a8f95, 0xffe066, 0.010f, 0.80f, false, 0.6f)},
    {"Deep Forest", PAL(0x2e2266, 0x8a6ad6, 0x7460b0, 0xe0d0ff, 0x8f80dc, 0x262640, 0x36467a, 0x46588c, 0x3a3a5c, 0x26363a, 0x36463f, 0x6a3a2a, 0x6c6c7e, 0xc8a0ff, 0.016f, 0.42f, true, 2.2f)},
    {"Cursed Forest", PAL(0x1c0a26, 0x8a2a4c, 0x5e2440, 0xff8a8a, 0x9a5a7a, 0x2a1828, 0x4a3448, 0x5a3848, 0x3a2430, 0x3a2434, 0x4a2a40, 0x4a2a2a, 0x6a5a62, 0xff4040, 0.019f, 0.34f, true, 3.6f)}}};
  MapDef desert{"Desert", "Desert with mummies, scorpions and sandstorms.", {
    {"Crusty Desert", PAL(0x3f9ae0, 0xf4dca6, 0xead0a0, 0xfff0c0, 0xd6dce4, 0x8a6a3a, 0xd6ac6a, 0xe6c486, 0xb8884e, 0x5a8a3a, 0x7aa04a, 0x7a5a3a, 0xc89a60, 0xffe066, 0.009f, 0.85f, false, 1.0f)},
    {"Twilight Desert", PAL(0x34266a, 0xf08a6a, 0xc47e7c, 0xffb080, 0xb08ab0, 0x5a3a3a, 0xb88a7a, 0xc89a88, 0x8a6a6a, 0x4a6a4a, 0x5a7a4a, 0x5a3a2a, 0xa07a70, 0xff9a60, 0.014f, 0.22f, true, 2.6f)},
    {"Blood Desert", PAL(0x260808, 0xa02a1a, 0x7a2018, 0xff5030, 0x8a3a30, 0x3a1010, 0x8a3a2a, 0x9a4a30, 0x6a2a20, 0x3a3a2a, 0x4a4a2a, 0x3a2018, 0x7a3a30, 0xff3030, 0.019f, 0.20f, true, 4.0f)}}};
  MapDef grave{"Graveyard", "Graveyard with ghosts, zombies and Big Bob.", {
    {"Graveyard", PAL(0x0e1a30, 0x3e6478, 0x385868, 0xc0e8ff, 0x7898b8, 0x243030, 0x3a503a, 0x4a5e40, 0x3a3a2e, 0x24382c, 0x34483a, 0x3a2e24, 0x6a7078, 0x7fff9f, 0.021f, 0.6f, true, 1.4f)},
    {"Haunted Graveyard", PAL(0x0a1e18, 0x3a7a64, 0x346654, 0xd0ffe8, 0x6a9a8a, 0x1a2a24, 0x34503e, 0x405e40, 0x30382a, 0x203a2a, 0x2e4a34, 0x34261e, 0x60706c, 0x60ffb0, 0.023f, 0.55f, true, 2.4f)},
    {"Cursed Graveyard", PAL(0x160a26, 0x6a3c8a, 0x52306c, 0xe8c0ff, 0x8a6aaa, 0x20162a, 0x3a3054, 0x4a3a60, 0x302438, 0x2a1e38, 0x3a2c4a, 0x30202a, 0x6a6278, 0xd060ff, 0.023f, 0.5f, true, 3.2f)}}};
  gMaps = {forest, desert, grave};
}

static void initEnemies() {
  const u8 F = 1, D = 2, G = 4, A = 7;
  auto E = [](const char* n, u8 maps, float hp, float dmg, float spd, u8 beh, bool fly, int model, float r, float sc, float xp, u8 rank = RK_NORMAL, u32 tint = 0) {
    gEnemies.push_back({n, maps, hp, dmg, spd * 0.72f, beh, fly, model, r, sc, xp, rank, tint});
  };
  E("Slime", F, 6, 8, 6.5f, EB_CHASE, false, M_SLIME, .5f, 1, 1);
  E("Goblin", F, 7, 15, 5, EB_CHASE, false, M_GOBLIN, .45f, 1, 1);
  E("Bee", F, 10, 15, 6.5f, EB_FLY, true, M_BEE, .4f, 1, 1.5f);
  E("Strong Goblin", F, 15, 25, 5.5f, EB_CHASE, false, M_GOBLIN_STRONG, .55f, 1, 2.5f);
  E("Boomer", F, 18, 30, 6, EB_EXPLODE, false, M_BOOMER, .6f, 1, 3);
  E("Golem", F, 20, 20, 5, EB_CHASE, false, M_GOLEM, .7f, 1, 3);
  E("Goblin Tank", F, 40, 30, 5, EB_CHASE, false, M_GOBLIN_TANK, .85f, 1, 6);
  E("Ghoul", F | G, 9, 12, 9, EB_CHASE, false, M_GHOUL, .45f, 1, 1.5f);
  E("Skeleton", F | G, 20, 20, 5.5f, EB_CHASE, false, M_SKELETON, .45f, 1, 3);
  E("Armored Skeleton", F | G, 30, 25, 4, EB_CHASE, false, M_SKELETON_ARMORED, .55f, 1, 4.5f);
  E("Tumblebone", D, 7, 9, 11, EB_ROLL, false, M_TUMBLEBONE, .5f, 1, 1);
  E("Mummy", D, 8, 13, 5.5f, EB_CHASE, false, M_MUMMY, .45f, 1, 1);
  E("Wisp", D, 8, 13, 7.5f, EB_FLY, true, M_WISP, .4f, 1, 1.2f);
  E("Cactus Shooter", D, 15, 30, 5, EB_SHOOT, false, M_CACTUS_SHOOTER, .55f, 1, 2.5f);
  E("Scorpion", D, 16, 20, 7, EB_POISON, false, M_SCORPION, .6f, 1, 2.5f);
  E("Dusty Skeleton", D, 16, 22, 6.5f, EB_CHASE, false, M_SKELETON_DUSTY, .45f, 1, 2.5f);
  E("Dusty Armored Skeleton", D, 25, 16, 4.5f, EB_CHASE, false, M_SKELETON_ARMORED_DUSTY, .55f, 1, 4);
  E("Big Scorpion", D, 30, 30, 6.5f, EB_POISON, false, M_SCORPION_BIG, .85f, 1, 5);
  E("Mummy Tank", D, 45, 30, 5, EB_CHASE, false, M_MUMMY_TANK, .85f, 1, 7);
  E("Ancient Mummy", D, 70, 40, 5, EB_CHASE, false, M_MUMMY_ANCIENT, .7f, 1, 10);
  E("Ghost", G, 10, 10, 6, EB_FLY, true, M_GHOST, .45f, 1, 1.5f);
  E("Head", G, 12, 8, 7, EB_FLY, true, M_HEAD, .45f, 1, 1.5f);
  E("Zombie", G, 12, 20, 6.5f, EB_CHASE, false, M_ZOMBIE, .5f, 1, 2);
  E("Greater Ghost", G, 20, 13, 7, EB_FLY, true, M_GHOST_GREATER, .6f, 1, 3);
  E("Purple Ghost", G, 40, 20, 8, EB_FLY, true, M_GHOST_PURPLE, .6f, 1, 6);
  E("Red Ghost", G, 100, 30, 10, EB_FLY, true, M_GHOST_RED, .65f, 1, 12);
  E("Gold Skeleton", A, 50, 25, 6, EB_CHASE, false, M_SKELETON_GOLD, .5f, 1.1f, 5);
  E("XP Skeleton", A, 50, 25, 6, EB_CHASE, false, M_SKELETON_XP, .5f, 1.1f, 40);
  E("Stone Golem", F, 250, 40, 6, EB_MB_SLAM, false, M_MB_STONEGOLEM, 1.6f, 1, 60, RK_MINIBOSS);
  E("Chunkham the Terrible", F, 250, 40, 6, EB_MB_CHARGE, false, M_MB_CHUNKHAM, 1.5f, 1, 60, RK_MINIBOSS);
  E("Sand Golem", D, 250, 40, 6, EB_MB_THROW, false, M_MB_SANDGOLEM, 1.6f, 1, 60, RK_MINIBOSS);
  E("Scorpionussy", D, 250, 35, 7, EB_MB_SPIT, false, M_MB_SCORPIONUSSY, 1.7f, 1, 60, RK_MINIBOSS);
  E("Ghostham", G, 320, 45, 6.5f, EB_MB_RING, true, M_MB_GHOSTHAM, 1.5f, 1, 70, RK_MINIBOSS);
  E("Skeleton King", G, 320, 40, 8, EB_MB_SUMMON, false, M_MB_SKELETONKING, 1.3f, 1, 70, RK_MINIBOSS);
  E("Lil Bark", F, 9000, 40, 0, EB_BOSS, false, M_B_LILBARK, 2.4f, 1, 300, RK_BOSS);
  E("Chadbark", F, 8000, 45, 4.5f, EB_BOSS, false, M_B_CHADBARK, 2.4f, 1, 400, RK_BOSS);
  E("Bark Vader", F, 10000, 50, 4.5f, EB_BOSS, false, M_B_BARKVADER, 2.7f, 1, 500, RK_BOSS);
  E("Lil Anubis", D, 8000, 40, 4, EB_BOSS, false, M_B_LILANUBIS, 1.8f, 1, 300, RK_BOSS);
  E("Anubruh", D, 8000, 45, 4.5f, EB_BOSS, false, M_B_ANUBRUH, 2.0f, 1, 400, RK_BOSS);
  E("Juge Anubis", D, 8000, 50, 5, EB_BOSS, false, M_B_JUGEANUBIS, 2.2f, 1, 500, RK_BOSS);
  E("Big Bob", G, 9000, 45, 4.5f, EB_BOSS, false, M_B_BIGBOB, 2.4f, 1, 400, RK_BOSS);
  E("Ghost King", A, 60000, 60, 5, EB_BOSS, true, M_B_GHOSTKING, 3.0f, 1, 2000, RK_BOSS);
  E("Final Swarm Ghost", A, 30, 25, 9, EB_FLY, true, M_SWARMGHOST, .5f, 1, 2);

  gSpawnTables[MAP_FOREST] = {{E_SLIME, 0, 1.0f}, {E_GOBLIN, 0, 1.0f}, {E_BEE, .06f, .7f}, {E_GHOUL, .14f, .6f}, {E_GOBLIN_STRONG, .22f, .8f}, {E_SKELETON, .30f, .7f}, {E_BOOMER, .36f, .45f}, {E_GOLEM, .44f, .5f}, {E_SKELETON_ARMORED, .55f, .5f}, {E_GOBLIN_TANK, .64f, .4f}};
  gSpawnTables[MAP_DESERT] = {{E_TUMBLEBONE, 0, .9f}, {E_MUMMY, 0, 1.0f}, {E_WISP, .06f, .7f}, {E_CACTUS, .15f, .35f}, {E_SCORPION, .22f, .7f}, {E_SKELETON_DUSTY, .30f, .7f}, {E_SKELETON_ARMORED_DUSTY, .44f, .5f}, {E_SCORPION_BIG, .55f, .45f}, {E_MUMMY_TANK, .62f, .4f}, {E_MUMMY_ANCIENT, .74f, .3f}};
  gSpawnTables[MAP_GRAVEYARD] = {{E_ZOMBIE, 0, 1.0f}, {E_GHOST, 0, .9f}, {E_HEAD, .05f, .7f}, {E_GHOUL, .14f, .6f}, {E_SKELETON, .20f, .7f}, {E_GHOST_GREATER, .33f, .6f}, {E_SKELETON_ARMORED, .45f, .5f}, {E_GHOST_PURPLE, .58f, .4f}, {E_GHOST_RED, .82f, .25f}};
  int mb[MAP_COUNT][2] = {{E_MB_STONEGOLEM, E_MB_CHUNKHAM}, {E_MB_SANDGOLEM, E_MB_SCORPIONUSSY}, {E_MB_GHOSTHAM, E_MB_SKELETONKING}};
  int bs[MAP_COUNT][3] = {{E_B_LILBARK, E_B_CHADBARK, E_B_BARKVADER}, {E_B_LILANUBIS, E_B_ANUBRUH, E_B_JUGEANUBIS}, {E_B_BIGBOB, E_B_BIGBOB, E_B_BIGBOB}};
  memcpy(gMinibosses, mb, sizeof(mb)); memcpy(gBosses, bs, sizeof(bs));
}

static void initChars() {
  gChars = {
    {"Fox", W_FIRESTAFF, "RNG Blessing", "+1% luck per level.", {{S_EVASION, 5}, {S_MOVESPD, .04f}, {S_JUMPS, 1}, {S_PICKUP, .4f}, {S_LUCK, 15}}, M_FOX, IR(IS_FOX), "A lucky fox with a fire staff. Great for starting out."},
    {"Ninja", W_KATANA, "Shadowstep", "+1% evasion per level; when you dodge, the next hit is a crit.", {{S_CRIT, 10}, {S_MOVESPD, .08f}, {S_EVASION, 10}, {S_MAXHP, -20}}, M_NINJA, IR(IS_NINJA), "Fast and deadly. Lives on dodges and crits."},
    {"Calcium", W_BONE, "Speed Demon", "Builds speed while running (halves when hit); damage scales with speed. +0.75% damage per level.", {{S_MOVESPD, .20f}, {S_MAXHP, -30}}, M_CALCIUM, IR(IS_SKELETON), "A fast skeleton: never stop running."},
    {"Monke", W_BANANARANG, "Monke Grip", "Climbs walls and rocks; +2 Max HP per level.", {{S_JUMPH, .25f}, {S_MAXHP, 20}}, M_MONKE, IR(IS_MONKE), "An agile monkey: climbs everything and throws bananas."},
    {"Ogre", W_AXE, "Warrior", "+1.5% damage per level.", {{S_MAXHP, 60}, {S_MOVESPD, -.04f}, {S_ARMOR, 10}}, M_OGRE, IR(IS_OGRE), "Slow, but every level makes it stronger."},
  };
}

static void initInters() {
  gInters = {
    {"Chest", "Open with gold to get an item.", IR(IS_CHEST)},
    {"Golden Chest", "Free! A guaranteed item.", IR(IS_CHEST)},
    {"Charge Shrine", "Stay in the area until it charges and pick a bonus.", IR(IS_SHRINE, 0x4fc3f7)},
    {"Greed Shrine", "+5% difficulty and a bit of gold.", IR(IS_COIN, 0xffd23f, 0xc9a227)},
    {"Cursed Shrine", "+1 boss in this stage, with more silver.", IR(IS_SKULL)},
    {"Challenge Shrine", "Summons elites; the last one drops a free chest.", IR(IS_SWORD, 0xc26bff, 0x4a2a6a)},
    {"Moai", "Choose 1 of 3 items.", IR(IS_MASK, 0x9e9e9e, 0x424242)},
    {"Shady Guy", "Sells items for gold; hat color is the rarity.", IR(IS_HAT, 0x37474f, 0xffd23f)},
    {"Microwave", "Duplicates an item of its rarity (consumes another).", IR(IS_CARD, 0xb0bec5)},
    {"Shrine of Succ", "Pulls all XP on the ground to you.", IR(IS_MAGNET, 0x7e57c2)},
    {"Boss Portal", "Summons the stage boss.", IR(IS_PORTAL, 0xff5252, 0xffcdd2)},
    {"Portal", "Go to the next stage.", IR(IS_PORTAL, 0x40c4ff, 0xe1f5fe)},
    {"Pot", "Break it to find gold, XP and powerups.", IR(IS_POT, 0xc07a4a)},
    {"Big Pot", "More rewards.", IR(IS_POT, 0xa0603a)},
    {"Silver Pot", "Contains silver.", IR(IS_POT, 0xd8dde6)},
    {"Pumpkin", "Break it to find rewards.", IR(IS_PUMPKIN, 0xff8f00)},
    {"Tumbleweed", "Break it to find rewards.", IR(IS_ROCKS, 0xc8a060)},
    {"Crypt Key", "Collect 4 to open the boss portal.", IR(IS_KEY, 0x7fff9f)},
    {"Altar", "Capture to break the boss's shield.", IR(IS_SHRINE, 0xff5252)},
  };
}

std::vector<IconRecipe> buildIconList() {
  std::vector<IconRecipe> v(512);
  for (int i = 0; i < W_COUNT; i++) v[ICON_WEAPON0 + i] = gWeapons[i].icon;
  for (int i = 0; i < T_COUNT; i++) v[ICON_TOME0 + i] = gTomes[i].icon;
  for (int i = 0; i < IT_COUNT; i++) v[ICON_ITEM0 + i] = gItems[i].icon;
  for (int i = 0; i < PU_COUNT; i++) v[ICON_PU0 + i] = gPowerups[i].icon;
  for (int i = 0; i < I_COUNT; i++) v[ICON_INTER0 + i] = gInters[i].icon;
  IconRecipe ui[UI_COUNT] = {
    IR(IS_CLOCK, 0xeeeeee), IR(IS_SILVER), IR(IS_COIN, 0xffd23f, 0xc9a227), IR(IS_SKULL), IR(IS_HEART, 0xe53935), IR(IS_SHIELDPU), IR(IS_STAR, 0xffd23f),
    IR(IS_PAUSE, 0xffffff), IR(IS_MAP), IR(IS_JUMP, 0xffffff), IR(IS_SLIDE, 0xffffff), IR(IS_INTERACT, 0xffffff), IR(IS_GEAR, 0xdddddd), IR(IS_CROWN),
    IR(IS_REROLL, 0x80d8ff), IR(IS_SKIP, 0xffffff), IR(IS_BANISH), IR(IS_TROPHY), IR(IS_LOCK), IR(IS_XP, 0x40c4ff), IR(IS_HOURGLASS, 0xffd23f),
    IR(IS_PORTAL, 0xff5252, 0xffcdd2), IR(IS_SWORD, 0xffffff, 0xdddddd), IR(IS_ARROWUP, 0x69f0ae),
  };
  for (int i = 0; i < UI_CHAR0; i++) v[ICON_UI0 + i] = ui[i];
  for (int i = 0; i < C_COUNT; i++) v[ICON_UI0 + UI_CHAR0 + i] = gChars[i].icon;
  return v;
}

void dataInit() {
  if (!gWeapons.empty()) return;
  initWeapons(); initTomes(); initItems(); initPowerups(); initMaps(); initEnemies(); initChars(); initInters();
}
