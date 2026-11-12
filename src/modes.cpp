#include "modes.h"
#include "data.h"
#include <ctime>

const ModeDef kModes[MODE_COUNT] = {
  {"Classic", "The full run", "Pick a hero, a map, a tier and the time per stage. Find the boss, open the portal, survive the Final Swarm.", ICON_UI0 + UI_SWORD, 0x69f0ae},
  {"Daily Run", "Same run for everyone today", "One run a day with a fixed hero, map, seed and two mutators. Your best score of the day is kept.", ICON_UI0 + UI_CLOCK, 0xffd23f},
  {"Boss Rush", "Nine bosses back to back", "Each round drops you next to a boss, from the first forest guardian to the Ghost King. Level-ups and a golden chest after every kill.", ICON_UI0 + UI_CROWN, 0xff5252},
  {"Endless Siege", "No portal, no way out", "The horde never stops growing and a boss shows up every 5 minutes. How long can you hold?", ICON_UI0 + UI_SKULL, 0xb48cff},
  {"Hero Gauntlet", "New hero every stage", "Three stages across the three maps. At each portal you become a different random hero, keeping your items, tomes and weapons.", ICON_UI0 + UI_REROLL, 0x4fc3f7},
  {"Shrine Hunt", "Short stages, packed maps", "Maps packed with chests and shrines, cheaper chests and a starting purse, but only 6 minutes per stage.", ICON_INTER0 + I_CHARGE_SHRINE, 0x7bd23f},
  {"Reverse Mode", "You are the boss now", "Play as the Overlord, a towering villain. The ten heroes storm your lair in waves and hero parties while your goblin minions fight at your side. Hold for 10 minutes, then crush the Legendary Party.", ICON_WEAPON0 + W_CORRUPTED, 0xc070ff},
};

const MutatorDef kMutators[MU_COUNT] = {
  {"Glass Cannon", "Double damage, half max HP.", ICON_ITEM0 + IT_MIRROR, 25},
  {"Brute Force", "+100% damage, -50% attack speed.", ICON_ITEM0 + IT_POWERGLOVES, 0},
  {"Trigger Happy", "+100% attack speed, -50% damage.", ICON_ITEM0 + IT_GYMSAUCE, 0},
  {"Close Quarters", "Only close-range weapons are offered; a ranged starting weapon becomes a Sword.", ICON_WEAPON0 + W_SWORD, 15},
  {"Specialist", "Only one weapon slot, but +75% damage.", ICON_WEAPON0 + W_KATANA, 15},
  {"Madness Edge", "Critical hits deal triple damage instead of double.", ICON_ITEM0 + IT_DEMONICBLADE, 0},
  {"Sure Shot", "+100% crit chance, but crits only deal +50% damage.", ICON_ITEM0 + IT_TACTICALGLASSES, 0},
  {"Giant Form", "You are huge: +50 max HP, +10% armor, -15% move speed.", ICON_ITEM0 + IT_CHONKPLATE, 0},
  {"Pocket Size", "You are tiny: +15% move speed and evasion, -25% max HP.", ICON_ITEM0 + IT_SKULEG, 10},
  {"Hard Shell", "Every hit deals exactly 5 damage, but nothing heals you.", ICON_ITEM0 + IT_SPIKYSHIELD, 10},
  {"Kill Streak", "Fast kill chains grant speed, then damage, then regeneration.", ICON_UI0 + UI_SKULL, 0},
  {"Bargain Day", "Chests and shops cost half.", ICON_UI0 + UI_GOLD, 0},
  {"Buffet", "Level-ups offer 4 choices instead of 3.", ICON_ITEM0 + IT_BORGAR, 0},
  {"Boss Duo", "Stage bosses come in pairs, each with 75% HP.", ICON_UI0 + UI_CROWN, 30},
  {"Cell Division", "Wounded enemies split into two smaller halves.", ICON_ITEM0 + IT_MOLDYCHEESE, 25},
  {"Blink Foes", "Enemies sometimes teleport when they are hit.", ICON_UI0 + UI_PORTAL, 20},
  {"Time Warp", "Game speed keeps drifting between slow and fast.", ICON_UI0 + UI_HOURGLASS, 15},
  {"Monoculture", "Each stage spawns a single enemy type, with more elites.", ICON_ITEM0 + IT_CURSEDDOLL, 10},
  {"Weapon Shuffle", "At every new stage your weapons turn into random others, keeping their level.", ICON_UI0 + UI_REROLL, 10},
  {"Surprise Pots", "Breaking pots triggers a random effect: gold, XP, bombs, freezing, buffs or a ghost ally.", ICON_ITEM0 + IT_POT, 0},
  {"Moon Gravity", "Much higher jumps, +1 extra jump, no fall damage.", ICON_UI0 + UI_JUMP, 0},
  {"Volatile Foes", "Enemies may burst into slow bullets when they die.", ICON_ITEM0 + IT_TOXICBARREL, 30},
  {"Elite Parade", "Elites show up three times as often.", ICON_ITEM0 + IT_GOLDENSHIELD, 25},
  {"Stat Roulette", "Every minute one random stat gets a big bonus and another a penalty.", ICON_ITEM0 + IT_CLOVER, 10},
};

int mutatorSilverPct(u32 mask) { int p = 0; for (int i = 0; i < MU_COUNT; i++) if (mask & (1u << i)) p += kMutators[i].silverPct; return p; }
int mutatorCount(u32 mask) { int n = 0; for (int i = 0; i < MU_COUNT; i++) n += (mask >> i) & 1; return n; }

int dailyKeyToday() {
  time_t t = time(nullptr); struct tm lt;
#ifdef _WIN32
  localtime_s(&lt, &t);
#else
  localtime_r(&t, &lt);
#endif
  return (lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
}

DailySetup dailySetup(int key) {
  DailySetup d; d.key = key;
  u32 h = hash32((u32)key * 2654435761u + 0x5eed);
  d.seed = h | 1;
  d.character = (int)(hash32(h + 1) % C_COUNT);
  d.map = (int)(hash32(h + 2) % MAP_COUNT);
  int a = (int)(hash32(h + 3) % MU_COUNT), b = (int)(hash32(h + 4) % (MU_COUNT - 1));
  if (b >= a) b++;
  d.mutators = (1u << a) | (1u << b);
  return d;
}

int dailyScore(int kills, int level, bool victory, float runTime) {
  return kills + level * 50 + (victory ? 5000 + std::max(0, 1200 - (int)runTime) * 2 : (int)runTime);
}
