// Procedural low-poly models with baked flip-book animation (stepped / low-FPS look)
#pragma once
#include "base.h"
#include "art.h"

enum ModelId {
  M_FOX, M_NINJA, M_CALCIUM, M_MONKE, M_OGRE, M_ROCCO, M_BRAMBLE,
  M_SLIME, M_GOBLIN, M_BEE, M_GOBLIN_STRONG, M_BOOMER, M_GOLEM, M_GOBLIN_TANK, M_GHOUL, M_SKELETON, M_SKELETON_ARMORED,
  M_TUMBLEBONE, M_MUMMY, M_WISP, M_CACTUS_SHOOTER, M_SCORPION, M_SKELETON_DUSTY, M_SKELETON_ARMORED_DUSTY, M_SCORPION_BIG, M_MUMMY_TANK, M_MUMMY_ANCIENT,
  M_GHOST, M_HEAD, M_ZOMBIE, M_GHOST_GREATER, M_GHOST_PURPLE, M_GHOST_RED, M_SKELETON_GOLD, M_SKELETON_XP,
  M_MB_STONEGOLEM, M_MB_CHUNKHAM, M_MB_SANDGOLEM, M_MB_SCORPIONUSSY, M_MB_GHOSTHAM, M_MB_SKELETONKING,
  M_B_LILBARK, M_B_CHADBARK, M_B_BARKVADER, M_B_LILANUBIS, M_B_ANUBRUH, M_B_JUGEANUBIS, M_B_BIGBOB, M_B_GHOSTKING,
  M_SWARMGHOST,
  // props
  M_TREE_BLOB, M_TREE_BLOB2, M_PINE, M_PINE2, M_ROCK_BIG, M_ROCK_MED, M_ROCK_SMALL, M_LOG, M_STUMP, M_MUSHROOM, M_BUSH,
  M_RUIN_WALL, M_RUIN_PILLAR, M_GRASS, M_FLOWERS,
  M_CACTUS_PROP, M_CACTUS_PROP2, M_DEAD_TREE, M_SANDSTONE, M_SANDSTONE_ARCH, M_OBELISK, M_BONES, M_PALM, M_DESERT_GRASS,
  M_TOMB, M_TOMB_CROSS, M_DEAD_TREE2, M_FENCE, M_CRYPT, M_LANTERN, M_GRAVE_GRASS,
  // interactables
  M_CHEST, M_CHEST_GOLD, M_SHRINE_CHARGE, M_SHRINE_GREED, M_SHRINE_CURSED, M_SHRINE_CHALLENGE, M_MOAI, M_SHADY, M_MICROWAVE,
  M_SHRINE_SUCC, M_PORTAL_BOSS, M_PORTAL_EXIT, M_POT, M_POT_BIG, M_POT_SILVER, M_PUMPKIN, M_TUMBLEWEED, M_CRYPT_KEY, M_ALTAR, M_HEAL_PILLAR,
  // pickups
  M_XP_SMALL, M_XP_MED, M_XP_BIG, M_GOLD_COIN, M_SILVER_COIN, M_BORGAR, M_PU_SHIELD, M_PU_CLOCK, M_PU_GOLD, M_PU_RAGE, M_PU_MAGNET, M_PU_NUKE,
  // projectiles
  M_P_FIREBALL, M_P_ARROW, M_P_BULLET, M_P_BONE, M_P_AXE, M_P_SHIELD, M_P_BANANA, M_P_BLACKHOLE, M_P_BLOODORB, M_P_ROCK,
  M_P_BLADE, M_P_DIE, M_P_MINE, M_P_FLASK, M_P_ROCKET, M_P_DAGGER, M_P_TORNADO, M_P_ORB, M_P_SOUL, M_P_SPIKE, M_P_ENEMYBALL,
  M_P_SCYTHE, M_P_WAVE, M_P_BOB, M_P_SWORD, M_P_KATANA,
  // fx meshes (transparent pass)
  M_FX_SLASH, M_FX_SPHERE, M_FX_RING, M_FX_CONE, M_FX_BEAM,
  M_COUNT
};

enum ClipId { CL_IDLE, CL_MOVE, CL_ATTACK, CL_JUMP, CL_SLIDE, CL_SPECIAL, CL_COUNT };
constexpr int BAKE_FPS = 12;
struct FrameRef { int frame = 0, next = 0; float blend = 0; };

// GPU vertex formats
struct AnimVtx { float x, y, z; i8 nx, ny, nz, pad; };            // 16 B per vertex per frame
struct StatVtx { u8 r, g, b, emis; float u, v; u8 layer, f0, f1, f2; }; // 16 B per vertex

struct ModelClip { int first = 0, frames = 1; bool loop = true; };
struct ModelInfo {
  int vtx = 0, idx = 0;                 // counts
  size_t animOff = 0, statOff = 0, idxOff = 0; // byte offsets in shared buffers
  int frames = 1;
  ModelClip clip[CL_COUNT];
  float radius = 1, height = 1, minY = 0;
  bool transparent = false;
};

struct ModelLibrary {
  std::vector<AnimVtx> anim;
  std::vector<StatVtx> stat;
  std::vector<u16> idx;
  ModelInfo info[M_COUNT];
  void build();
};
extern ModelLibrary gModels;

// frame index for clip at time t (seconds) honoring the stepped animation rate
int modelFrame(int model, int clip, float t, int animFps, bool clampEnd = false);
// stepped for animFps <= 12; animFps >= 24 interpolates between baked frames (smooth option)
FrameRef modelFrameRef(int model, int clip, float t, int animFps, bool clampEnd = false);
float clipDuration(int model, int clip);
