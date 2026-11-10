// Procedural art: texture-array layers (pixel textures), UI atlas, font, icons
#pragma once
#include "base.h"

enum TexLayer {
  L_PLAIN, L_NOISE, L_GRASS, L_DIRT, L_SAND, L_STONE, L_BRICK, L_BARK, L_LEAVES, L_PINE,
  L_WOOD, L_METAL, L_GOLD, L_CLOTH, L_SKIN, L_BONE, L_SLIME, L_CHITIN, L_FUR, L_WRAPS,
  L_ROCK, L_CRYSTAL, L_TUFT, L_FLOWER, L_TOMB, L_PUMPKIN, L_CACTUS, L_PORTAL, L_RUNE, L_LAVA,
  L_FEATHER, L_GRAVEL, L_COUNT
};
constexpr int LAYER_SIZE = 64;
// Builds all layers RGBA8 (L_COUNT * 64*64*4)
void artBuildLayers(std::vector<u8>& out);

// ---------- canvas for 2D pixel art ----------
struct Canvas {
  int w, h; std::vector<Col> px;
  Canvas(int W, int H) : w(W), h(H), px(W * H, Col(0, 0, 0, 0)) {}
  void set(int x, int y, Col c) { if (x >= 0 && y >= 0 && x < w && y < h) px[y * w + x] = c; }
  Col get(int x, int y) const { return (x >= 0 && y >= 0 && x < w && y < h) ? px[y * w + x] : Col(0, 0, 0, 0); }
  void blend(int x, int y, Col c);
  void rect(float x0, float y0, float x1, float y1, Col c);
  void circle(float cx, float cy, float r, Col c);
  void ellipse(float cx, float cy, float rx, float ry, Col c);
  void ring(float cx, float cy, float r0, float r1, Col c);
  void arc(float cx, float cy, float r0, float r1, float a0, float a1, Col c); // angles in degrees, y-down screen space
  void erase(float x0, float y0, float x1, float y1);
  void line(float x0, float y0, float x1, float y1, float th, Col c);
  void tri(v2 a, v2 b, v2 c, Col col);
  void poly(const std::vector<v2>& p, Col c);
  void outline(Col c);          // 1px outline around opaque pixels
  void shadeEdges(float k);     // darken bottom/right edges of shapes
};

// icon recipes (shape + colors). Shapes are drawn procedurally.
enum IconShape : u8 {
  IS_NONE, IS_SWORD, IS_STAFF, IS_BOW, IS_REVOLVER, IS_BONE, IS_AURA, IS_AXE, IS_SHIELD, IS_BANANA, IS_HOLE,
  IS_BLOODDROP, IS_ROCKS, IS_DICE, IS_FLAMECONE, IS_FIREFOOT, IS_ICEFOOT, IS_HEROSWORD, IS_KATANA, IS_BOLT,
  IS_MINE, IS_FLASK, IS_SCYTHE, IS_SHOTGUN, IS_ROCKET, IS_SNIPER, IS_NOODLE, IS_TORNADO, IS_DAGGER, IS_CORRUPT,
  IS_DEXE, IS_BOOK, IS_RING, IS_BOTTLE, IS_MUG, IS_BURGER, IS_CLOVER, IS_CLOAK, IS_CARD, IS_DOLL, IS_HAND,
  IS_GEM, IS_PLUG, IS_CORE, IS_FEATHER, IS_GOGGLES, IS_MASK, IS_GHOST, IS_FORK, IS_GLOVE, IS_COIN, IS_SHOE,
  IS_CUBE, IS_KEY, IS_LAMP, IS_CHEESE, IS_MIRROR, IS_SCARF, IS_SKULL, IS_MAGNET, IS_GLASSES, IS_BRACELET,
  IS_BARREL, IS_SKATE, IS_CLOCK, IS_POT, IS_HAT, IS_SNAKE, IS_PUMPKIN, IS_CAMPFIRE, IS_CACTUS, IS_WRENCH,
  IS_BEACON, IS_ANVIL, IS_HAMMER, IS_SOUL, IS_LEG, IS_KNUCKLES, IS_CLEAVER, IS_BOB, IS_HEART, IS_STAR,
  IS_SHARD, IS_CHEST, IS_OATS, IS_MEDKIT, IS_CANNON, IS_SHROUD, IS_LIGHTORB, IS_BLADE, IS_EAGLE, IS_TONIC,
  IS_SPIKYSHIELD, IS_BAG, IS_GOLDKEY, IS_NUKE, IS_RAGE, IS_SHIELDPU, IS_SILVER, IS_PAUSE, IS_MAP, IS_JUMP,
  IS_SLIDE, IS_INTERACT, IS_GEAR, IS_PORTAL, IS_SHRINE, IS_CROWN, IS_ARROWUP, IS_REROLL, IS_SKIP, IS_BANISH,
  IS_TROPHY, IS_LOCK, IS_FOX, IS_NINJA, IS_SKELETON, IS_MONKE, IS_OGRE, IS_XP, IS_HOURGLASS,
  IS_COUNT
};
struct IconRecipe { u8 shape = IS_NONE; u32 c1 = 0xffffff, c2 = 0x888888; u8 sym = 0; };
void artDrawIcon(Canvas& c, const IconRecipe& r);

// ---------- UI atlas ----------
constexpr int ATLAS_SIZE = 1024;
// Font cells: 0..255 = Latin-1, 256..319 = Cyrillic U+0410..U+044F, 320/321 = Yo/yo (U+0401/U+0451)
constexpr int GLYPH_COUNT = 322;
inline int glyphIdx(int cp) {
  if (cp >= 0 && cp < 256) return cp;
  if (cp >= 0x410 && cp <= 0x44F) return 256 + (cp - 0x410);
  if (cp == 0x401) return 320;
  if (cp == 0x451) return 321;
  return '?';
}
inline int glyphCp(int idx) { return idx < 256 ? idx : idx < 320 ? 0x410 + (idx - 256) : idx == 320 ? 0x401 : 0x451; }
struct ARect { u16 x = 0, y = 0, w = 0, h = 0; };
enum Sprite {
  SP_WHITE, SP_SOFT, SP_RING, SP_SPARK, SP_STAR, SP_SMOKE, SP_SHADOW, SP_DECAL_RING, SP_DECAL_DISC,
  SP_JOY_BASE, SP_JOY_KNOB, SP_BTN_BIG, SP_BTN_SMALL, SP_PANEL, SP_GLOW, SP_ARROW, SP_SLASH, SP_FLAME,
  SP_BOLT, SP_DROP, SP_SWIRL, SP_CROSS, SP_COUNT
};
struct UIAtlas {
  std::vector<u8> px; // ATLAS_SIZE^2 RGBA
  ARect glyph[GLYPH_COUNT];
  int glyphW[GLYPH_COUNT];
  ARect icon[512];
  ARect sprite[SP_COUNT];
};
constexpr int GLYPH_CW = 8, GLYPH_CH = 12; // cell incl. outline
void artBuildAtlas(UIAtlas& a, const std::vector<IconRecipe>& icons);
// decode next UTF-8 codepoint (Latin-1 + Cyrillic kept, anything else becomes '?'), advances i
int utf8Next(const std::string& s, size_t& i);
