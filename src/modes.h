// Game modes and run mutators (catalog + daily-run setup). The gameplay hooks live in game.cpp / combat.cpp.
#pragma once
#include "base.h"

enum GameMode { MODE_NORMAL, MODE_DAILY, MODE_BOSSRUSH, MODE_ENDLESS, MODE_GAUNTLET, MODE_SHRINEHUNT, MODE_REVERSE, MODE_COUNT };
struct ModeDef { const char* name; const char* tagline; const char* desc; int icon; u32 color; };
extern const ModeDef kModes[MODE_COUNT];

// many are adaptations of classic roguelite mutators (Soul Knight-style); names and tuning are our own
enum Mutator {
  MU_GLASS, MU_BRUTE, MU_TRIGGER, MU_MELEE, MU_SPECIALIST, MU_MADNESS, MU_SURESHOT, MU_GIANT,
  MU_POCKET, MU_SHELL, MU_STREAK, MU_BARGAIN, MU_BUFFET, MU_BOSSDUO, MU_SPLIT, MU_BLINK,
  MU_TIMEWARP, MU_MONO, MU_SHUFFLE, MU_POTS, MU_MOON, MU_VOLATILE, MU_ELITES, MU_ROULETTE, MU_COUNT
};
struct MutatorDef { const char* name; const char* desc; int icon; int silverPct; };
extern const MutatorDef kMutators[MU_COUNT];

constexpr int BOSSRUSH_ROUNDS = 9;   // three bosses per map (tiers 1-3), then the Ghost King
int mutatorSilverPct(u32 mask);
int mutatorCount(u32 mask);

// one shared run per calendar day: same hero, map, seed and mutators for everyone on that date
struct DailySetup { int key = 0, character = 0, map = 0; u32 seed = 1, mutators = 0; };
int dailyKeyToday();                 // local date as YYYYMMDD
DailySetup dailySetup(int key);
int dailyScore(int kills, int level, bool victory, float runTime);
