// Headless desktop harness (Mesa EGL surfaceless + GLES3): screenshots, art sheets, balance sims.
#include "app.h"
#include "i18n.h"
#include "render.h"
#include "audio.h"
#include <png.h>
#include <sys/stat.h>
#include <chrono>

typedef void* EGLDisplay; typedef void* EGLConfig; typedef void* EGLContext; typedef int EGLint; typedef unsigned int EGLBoolean; typedef unsigned int EGLenum;
extern "C" {
void* eglGetProcAddress(const char*);
EGLBoolean eglInitialize(EGLDisplay, EGLint*, EGLint*);
EGLBoolean eglBindAPI(EGLenum);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay, void*, void*, EGLContext);
}
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_NONE 0x3038
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

void plat_vibrate(int) {}
void plat_quit() {}

static int W = 2400, H = 1080;
static std::string outDir = "shots";
static GLuint fbo, colorRb, depthRb;

static bool writePng(const std::string& path, const u8* rgba, int w, int h, bool flip) {
  FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = png_create_info_struct(png);
  if (setjmp(png_jmpbuf(png))) { fclose(f); return false; }
  png_init_io(png, f);
  png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);
  for (int y = 0; y < h; y++) png_write_row(png, (png_bytep)(rgba + (size_t)(flip ? h - 1 - y : y) * w * 4));
  png_write_end(png, nullptr); png_destroy_write_struct(&png, &info); fclose(f);
  return true;
}
static void shot(const std::string& name) {
  std::vector<u8> px; int w, h; gR.readPixels(px, w, h);
  for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
  writePng(outDir + "/" + name + ".png", px.data(), w, h, true);
  LOGI("wrote %s/%s.png", outDir.c_str(), name.c_str());
}
static void frame(float dt = 1 / 30.f, int n = 1) { for (int i = 0; i < n; i++) { app_frame(dt); } glFinish(); }

static void simGame(Game& g, float seconds, float dt = 1 / 30.f) {
  int n = (int)(seconds / dt);
  for (int i = 0; i < n; i++) {
    g.update(dt);
    if (g.state == GS_TRANSITION) { g.update(1.0f); }
    if (g.state == GS_DEAD || g.state == GS_VICTORY) break;
  }
}

static void startGame(int map, int tier, int ch, bool god, u32 seed, float stageMin = 15) {
  App& a = gApp;
  a.selMap = map; a.selTier = tier; a.selChar = ch; a.set.stageMin = (int)stageMin;
  a.startRun();
  a.game.cfg.seed = seed; a.game.cfg.godMode = god; a.game.autoPilot = true;
  a.game.start(a.game.cfg); a.applySettingsToGame(a.game); a.game.autoPilot = true;
}

// ---------------------------------------------------------------- showcase sheets
struct ShowItem { int model, clip; float t, scale; std::string label; Col ta; };
static void showcase(const std::string& file, const std::string& name, const std::vector<ShowItem>& items, int map, int tier, float spacing, bool labels = true, float animT = 0.33f, float fovDeg = 38) {
  Game& g = gApp.game;
  RunConfig c; c.map = map; c.tier = tier; c.character = 0; c.seed = 77; c.godMode = true;
  g.start(c);
  g.enemies.clear(); g.inters.clear(); g.pickups.clear(); g.banners.clear(); g.texts.clear(); g.parts.clear();
  g.fovDeg = fovDeg; g.stormK = 0;
  World& w = g.world;
  w.pal.fogDensity *= 0.55f;
  float totalW = spacing * (items.size() - 1);
  float maxH = 0.5f;
  for (auto& it : items) maxH = std::max(maxH, gModels.info[it.model].height * it.scale);
  float aspect = (float)W / H, th = tanf(fovDeg * 0.5f * DEG);
  float dW = (totalW * 0.5f + spacing * 0.8f) / (th * aspect);
  float dH = maxH * 1.9f / (2 * th);
  float d = std::max(dW, dH);
  // clear props between the camera and the lineup (keep grass)
  w.props.erase(std::remove_if(w.props.begin(), w.props.end(), [&](const PropInst& p) { return fabsf(p.pos.x) < totalW * 0.5f + spacing + 8 && p.pos.z > -d - 6 && p.pos.z < 8 && !p.grass; }), w.props.end());
  w.buildGrid(); g.worldGen++;
  float h0 = w.T.height(0, 0);
  g.P.pos = v3(0, -100, 0);
  v3 tgt(0, h0 + maxH * 0.40f, 0), eye(0, h0 + maxH * 0.62f + d * 0.16f, -d);
  g.camEye = eye; g.camTgt = tgt;
  for (int pass = 0; pass < 2; pass++) {
    gApp.ui.begin(W, H, 0.75f, gApp.plat, 0.016f);
    gR.beginFrame(W, H, 0.016f);
    g.render(gR); // terrain & props (player is hidden below ground)
    for (size_t i = 0; i < items.size(); i++) {
      const ShowItem& it = items[i];
      float x = totalW * 0.5f - i * spacing;
      v3 p(x, w.T.height(x, 0) - gModels.info[it.model].minY * it.scale + (gModels.info[it.model].minY < -0.01f ? 0.25f : 0.f), 0);
      int frameI = modelFrame(it.model, it.clip, it.t >= 0 ? it.t : animT, 12);
      float yaw = PI + 0.22f * (x / std::max(1.f, totalW));
      gR.model(it.model, frameI, p, q4::yaw(yaw), v3(it.scale, it.scale, it.scale), 0, it.ta);
      gR.decal(v3(p.x, w.T.height(x, 0), p.z), gModels.info[it.model].radius * it.scale * 1.1f + 0.3f, SP_SHADOW, Col(0, 0, 0, 120));
    }
    gR.renderScene(true);
    UI& u = gApp.ui;
    if (labels) {
      // alternate two label rows so long names never overlap
      float rowY = 0;
      for (size_t i = 0; i < items.size(); i++) {
        float x = totalW * 0.5f - i * spacing;
        v2 sp; if (!gR.worldToScreen(v3(x, w.T.height(x, 0), -0.6f), sp)) continue;
        rowY = std::max(rowY, sp.y / u.s);
      }
      for (size_t i = 0; i < items.size(); i++) {
        float x = totalW * 0.5f - i * spacing;
        v2 sp; if (!gR.worldToScreen(v3(x, w.T.height(x, 0), -0.6f), sp)) continue;
        float y = rowY + 6 + (items.size() > 4 ? (i % 2) * 17.f : 0.f);
        float tw = u.textW(items[i].label) + 8;
        u.rect(sp.x / u.s - 0.5f, sp.y / u.s, 1, y - sp.y / u.s, Col(255, 255, 255, 70));
        u.panel(sp.x / u.s - tw / 2, y, tw, 14, Col(10, 8, 16, 220), Col(255, 255, 255, 90));
        u.text(sp.x / u.s, y + 2, items[i].label, 1, Col(255, 255, 255), AL_CENTER);
      }
    }
    u.panel(u.W * 0.5f - u.textW(name) - 10, 4, u.textW(name) * 2 + 20, 30, Col(10, 8, 16, 170), Col(255, 214, 80, 120));
    u.text(u.W * 0.5f, 8, name, 2, Col(255, 214, 80), AL_CENTER);
    u.text(u.W - 6, u.H - 12, "Omni Bonk Mobile - render em tempo real (GLES 3.0)", 1, Col(255, 255, 255, 150), AL_RIGHT);
    gR.uiFlush();
    glFinish();
  }
  shot(file);
}

static void artSheets() {
  auto E = [](int e, float sc = 1, int clip = CL_MOVE, float t = 0.33f) { const EnemyDef& d = gEnemies[e]; return ShowItem{d.model, clip, t, sc * d.scale, d.name, Col(255, 255, 255, 0)}; };
  showcase("art_01_inimigos_forest", "Inimigos - Forest", {E(E_SLIME), E(E_GOBLIN), E(E_BEE), E(E_GOBLIN_STRONG), E(E_BOOMER), E(E_GOLEM), E(E_GOBLIN_TANK), E(E_GHOUL), E(E_SKELETON), E(E_SKELETON_ARMORED)}, MAP_FOREST, 0, 2.6f);
  showcase("art_02_inimigos_desert", "Inimigos - Desert", {E(E_TUMBLEBONE), E(E_MUMMY), E(E_WISP), E(E_CACTUS), E(E_SCORPION), E(E_SKELETON_DUSTY), E(E_SKELETON_ARMORED_DUSTY), E(E_SCORPION_BIG), E(E_MUMMY_TANK), E(E_MUMMY_ANCIENT)}, MAP_DESERT, 0, 2.8f);
  showcase("art_03_inimigos_graveyard", "Inimigos - Graveyard e especiais", {E(E_GHOST), E(E_HEAD), E(E_ZOMBIE), E(E_GHOST_GREATER), E(E_GHOST_PURPLE), E(E_GHOST_RED), E(E_SKELETON_GOLD), E(E_SKELETON_XP), E(E_SWARMGHOST)}, MAP_GRAVEYARD, 0, 2.8f);
  showcase("art_04_minichefes", "Minichefes", {E(E_MB_STONEGOLEM), E(E_MB_CHUNKHAM), E(E_MB_SANDGOLEM), E(E_MB_SCORPIONUSSY), E(E_MB_GHOSTHAM), E(E_MB_SKELETONKING)}, MAP_FOREST, 1, 5.0f);
  showcase("art_05_chefes", "Chefes", {E(E_B_LILBARK, 1, CL_IDLE), E(E_B_CHADBARK), E(E_B_BARKVADER), E(E_B_LILANUBIS), E(E_B_ANUBRUH), E(E_B_JUGEANUBIS), E(E_B_BIGBOB), E(E_B_GHOSTKING, 0.75f)}, MAP_FOREST, 2, 6.0f);
  std::vector<ShowItem> chars;
  for (int i = 0; i < C_COUNT; i++) chars.push_back({gChars[i].model, CL_IDLE, 0.25f, 1.6f, std::string(gChars[i].name) + " / " + gWeapons[gChars[i].weapon].name, Col(255, 255, 255, 0)});
  showcase("art_06_personagens", "Personagens jogaveis", chars, MAP_FOREST, 0, 3.4f);
  // stepped animation strip: one character run cycle, consecutive 12 fps frames
  std::vector<ShowItem> strip;
  for (int f = 0; f < 6; f++) strip.push_back({M_NINJA, CL_MOVE, f / 12.f, 1.6f, fmt("frame %d - %.0f ms", f, f * 1000 / 12.f), Col(255, 255, 255, 0)});
  showcase("art_07_animacao_stepped", "Animacao stepped 12 FPS - corrida do Ninja", strip, MAP_FOREST, 0, 2.6f);
  auto I = [](int m, const char* n, Col ta = Col(255, 255, 255, 0), int clip = CL_IDLE, float t = 0.3f) { return ShowItem{m, clip, t, 1.f, n, ta}; };
  showcase("art_08_baus_shrines_torres", "Baus, shrines e torres de upgrade", {I(M_CHEST, "Baú"), I(M_CHEST_GOLD, "Baú Dourado"), I(M_CHEST, "Baú aberto", Col(255, 255, 255, 0), CL_SPECIAL, 1.0f), I(M_SHRINE_CHARGE, "Charge Shrine"), I(M_SHRINE_GREED, "Greed Shrine"), I(M_SHRINE_CURSED, "Cursed Shrine"), I(M_SHRINE_CHALLENGE, "Challenge Shrine"), I(M_MOAI, "Moai"), I(M_SHADY, "Shady Guy", hexc(kRarityColor[R_LEGENDARY], 0)), I(M_MICROWAVE, "Microwave"), I(M_SHRINE_SUCC, "Shrine of Succ")}, MAP_FOREST, 0, 3.4f);
  showcase("art_09_portais_potes_chaves", "Portais, potes e chaves", {I(M_PORTAL_BOSS, "Portal do Chefe"), I(M_PORTAL_EXIT, "Portal de Saída"), I(M_POT, "Pote"), I(M_POT_BIG, "Pote Grande"), I(M_POT_SILVER, "Pote de Prata"), I(M_PUMPKIN, "Abóbora"), I(M_TUMBLEWEED, "Tumbleweed"), I(M_CRYPT_KEY, "Chave da Cripta"), I(M_ALTAR, "Altar")}, MAP_GRAVEYARD, 0, 3.6f);
  auto Pp = [](int m, const char* n, float s = 1) { return ShowItem{m, CL_IDLE, 0, s, n, Col(255, 255, 255, 0)}; };
  showcase("art_10_pickups_powerups", "Pickups e powerups", {Pp(M_XP_SMALL, "XP", 3.4f), Pp(M_XP_MED, "XP+", 3.2f), Pp(M_XP_BIG, "XP++", 3.0f), Pp(M_GOLD_COIN, "Ouro", 3.2f), Pp(M_SILVER_COIN, "Prata", 3.2f), Pp(M_BORGAR, "Borgar", 3.0f), Pp(M_PU_SHIELD, "Escudo", 2.4f), Pp(M_PU_CLOCK, "Relógio", 2.4f), Pp(M_PU_GOLD, "Ouro x2", 2.4f), Pp(M_PU_RAGE, "Fúria", 2.4f), Pp(M_PU_MAGNET, "Ímã", 2.4f), Pp(M_PU_NUKE, "Bomba", 2.4f)}, MAP_DESERT, 0, 1.9f);
  // UI atlas (icons + font) and texture layers
  writePng(outDir + "/art_11_atlas_icones_fonte.png", gR.atlas.px.data(), ATLAS_SIZE, ATLAS_SIZE, false);
  std::vector<u8> layers; artBuildLayers(layers);
  int cols = 8, rows = (L_COUNT + cols - 1) / cols, sc = 4, tw = LAYER_SIZE * sc;
  std::vector<u8> img((size_t)cols * tw * rows * tw * 4, 30);
  for (int l = 0; l < L_COUNT; l++)
    for (int y = 0; y < tw; y++) for (int x = 0; x < tw; x++) {
      const u8* s = &layers[((size_t)l * LAYER_SIZE * LAYER_SIZE + (y / sc) * LAYER_SIZE + x / sc) * 4];
      u8* d = &img[(((size_t)(l / cols) * tw + y) * cols * tw + (l % cols) * tw + x) * 4];
      d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
    }
  writePng(outDir + "/art_12_texturas_pixel.png", img.data(), cols * tw, rows * tw, false);
}

// launcher icon: close-up of the Fox on the Green Forest, square render (run with --size 432x432)
static void iconShot() {
  Game& g = gApp.game;
  RunConfig c; c.map = MAP_FOREST; c.tier = 0; c.character = 0; c.seed = 9; c.godMode = true;
  g.start(c);
  g.enemies.clear(); g.inters.clear(); g.pickups.clear(); g.banners.clear(); g.texts.clear(); g.parts.clear();
  g.fovDeg = 34; g.stormK = 0;
  World& w = g.world;
  w.props.erase(std::remove_if(w.props.begin(), w.props.end(), [&](const PropInst& p) { return fabsf(p.pos.x) < 6 && p.pos.z > -8 && p.pos.z < 5; }), w.props.end());
  w.buildGrid(); g.worldGen++;
  g.P.pos = v3(0, -100, 0);
  int m = gChars[C_FOX].model; const ModelInfo& mi = gModels.info[m];
  float sc = 1.0f, hh = mi.height * sc, h0 = w.T.height(0, 0);
  g.camTgt = v3(0, h0 + hh * 0.79f, 0);
  g.camEye = v3(hh * 0.36f, h0 + hh * 0.88f, -hh * 1.6f);
  gR.set.renderScale = 1.f; gR.set.vignette = 0.f;
  for (int pass = 0; pass < 2; pass++) {
    gApp.ui.begin(W, H, 1, gApp.plat, 0.016f);
    gR.beginFrame(W, H, 0.016f);
    g.render(gR);
    gR.model(m, modelFrame(m, CL_IDLE, 0.2f, 12), v3(0, h0, 0), q4::yaw(PI - 0.38f), v3(sc, sc, sc), 0, Col(255, 255, 255, 0));
    gR.renderScene(true);
    gR.uiFlush();
    glFinish();
  }
  shot("icon_src");
}

// ---------------------------------------------------------------- scenarios
static void uiShots() {
  App& a = gApp;
  a.selMap = MAP_FOREST; a.selChar = C_NINJA; a.scr = SCR_TITLE; for (int i = 0; i < 240; i++) a.updateMenuGame(1 / 30.f);
  frame(1 / 30.f, 3); shot("ui_01_titulo");
  a.scr = SCR_CHARS; a.selChar = C_NINJA; frame(1 / 30.f, 2); shot("ui_02_personagens");
  a.scr = SCR_MAP; a.selMap = MAP_FOREST; a.selTier = 1; frame(1 / 30.f, 2); shot("ui_03_mapa_tier_tempo");
  a.scr = SCR_SETTINGS; a.settingsTab = 0; frame(1 / 30.f, 2); shot("ui_04_config_jogo");
  a.settingsTab = 1; frame(1 / 30.f, 2); shot("ui_05_config_graficos");
  a.scr = SCR_CATALOG; a.catTab = 2; frame(1 / 30.f, 2); shot("ui_06_catalogo_itens");
  a.catTab = 0; frame(1 / 30.f, 2); shot("ui_07_catalogo_armas");
  a.scr = SCR_SHOP; frame(1 / 30.f, 2); shot("ui_08_loja");
}

// ---------------------------------------------------------------- gamepad scenario (handheld UI)
static PadState gPad;
static void padFrames(int n, float dt = 1 / 30.f) { for (int i = 0; i < n; i++) { app_gamepad(gPad); app_frame(dt); } glFinish(); }
static void press(unsigned btn, int hold = 2, int after = 4) { gPad.buttons |= btn; padFrames(hold); gPad.buttons &= ~btn; padFrames(after); }
static void padLog(const char* what) {
  App& a = gApp;
  LOGI("[pad] %-28s scr=%d state=%d focus=%d (%.0f,%.0f %.0fx%.0f) selChar=%d selMap=%d tab=%d", what, (int)a.scr, (int)a.game.state, a.ui.hasFocus, a.ui.focus.x, a.ui.focus.y, a.ui.focus.w, a.ui.focus.h, a.selChar, a.selMap, a.settingsTab);
}
static void padScenario() {
  App& a = gApp;
  a.testMode = false; a.set.vibration = false;
  a.selMap = MAP_FOREST; a.selChar = C_FOX; a.scr = SCR_TITLE; for (int i = 0; i < 240; i++) a.updateMenuGame(1 / 30.f);
  padFrames(4); padLog("title"); shot("pad_01_titulo");
  press(PB_DOWN); press(PB_DOWN); press(PB_DOWN); press(PB_LEFT); padLog("title: 3x down, left"); shot("pad_02_titulo_config");
  press(PB_A); padFrames(6); padLog("settings"); shot("pad_03_config");
  for (int i = 0; i < 3; i++) press(PB_DOWN);
  press(PB_RIGHT); padLog("settings: adjust"); shot("pad_04_config_ajuste");
  for (int i = 0; i < 9; i++) press(PB_DOWN);
  padLog("settings: scrolled"); shot("pad_05_config_rolagem");
  press(PB_R1); padFrames(4); padLog("settings: R1 tab"); shot("pad_06_config_aba2");
  press(PB_R1); padFrames(4); shot("pad_07_config_controles");
  press(PB_B); padFrames(6); padLog("back to title");
  press(PB_DOWN); press(PB_A); padFrames(6); padLog("catalog"); shot("pad_08_catalogo");
  press(PB_R1); press(PB_R1); press(PB_DOWN); for (int i = 0; i < 9; i++) press(PB_DOWN); press(PB_RIGHT); press(PB_RIGHT);
  padLog("catalog: items scrolled"); shot("pad_09_catalogo_itens");
  press(PB_R1); press(PB_DOWN); press(PB_DOWN); for (int i = 0; i < 20; i++) press(PB_DOWN, 2, 2); padLog("catalog: enemies"); shot("pad_10_catalogo_inimigos");
  press(PB_B); padFrames(6);
  press(PB_DOWN); press(PB_DOWN); press(PB_A); padFrames(6); padLog("shop"); shot("pad_11_loja");
  press(PB_B); padFrames(6);
  press(PB_DOWN); press(PB_DOWN); press(PB_DOWN); press(PB_A); padFrames(6); padLog("about"); shot("pad_12_sobre");
  press(PB_B); padFrames(6);
  press(PB_DOWN); press(PB_DOWN); press(PB_DOWN); press(PB_RIGHT); press(PB_A); padFrames(6); padLog("quit dialog"); shot("pad_13_sair");
  press(PB_A); padFrames(6); padLog("quit dialog: no");
  press(PB_A); padFrames(8); padLog("chars"); shot("pad_14_personagens");
  press(PB_RIGHT); press(PB_RIGHT); padFrames(30); padLog("chars: 2x right"); shot("pad_15_personagens_calcium");
  press(PB_A); padFrames(8); padLog("map"); shot("pad_16_mapa");
  press(PB_DOWN); press(PB_RIGHT); press(PB_A); press(PB_DOWN); press(PB_RIGHT); press(PB_RIGHT); padLog("map: tier2, time +2"); 
  press(PB_DOWN); shot("pad_17_mapa_tempo");
  press(PB_START); padFrames(20); padLog("game start");
  a.game.cfg.godMode = true;
  gPad.ly = -1; padFrames(40); gPad.buttons |= PB_A; padFrames(3); gPad.buttons &= ~PB_A; gPad.rx = 0.8f; padFrames(25); gPad.rx = 0; padFrames(30); gPad.ly = 0;
  padLog("game: moved"); shot("pad_18_jogo_inicio");
  a.game.autoPilot = true; for (int i = 0; i < 30 * 150; i++) { a.game.update(1 / 30.f); if (a.game.state != GS_PLAY) { a.game.pendingLevels = 0; a.game.state = GS_PLAY; } }
  a.game.autoPilot = false; a.game.state = GS_PLAY; a.game.pendingLevels = 0; padFrames(6);
  padLog("game: 2.5 min"); shot("pad_19_jogo_hud");
  press(PB_START); padFrames(16); padLog("pause"); shot("pad_20_pausa");
  press(PB_RIGHT); padLog("pause: right"); press(PB_B); padFrames(6); padLog("pause: B resumes");
  press(PB_Y); padFrames(16); padLog("map (Y)"); shot("pad_21_mapa_completo");
  press(PB_B); padFrames(6); padLog("map: B closes");
  Game& g = a.game;
  g.genLevelOffers(); g.pendingLevels = 1; g.state = GS_LEVELUP; gPad.buttons |= PB_A; padFrames(4); gPad.buttons &= ~PB_A; padFrames(14); padLog("levelup (A mash locked)"); shot("pad_22_level_up");
  press(PB_RIGHT); padLog("levelup: right"); shot("pad_23_level_up_foco"); press(PB_A); padFrames(4); padLog("levelup: picked");
  g.state = GS_PLAY; for (auto& it : g.inters) if (it.type == I_CHEST && !it.used) { g.openChest(it, true); break; }
  g.chestAnim = 2; padFrames(16); padLog("chest"); shot("pad_24_bau"); press(PB_A); padFrames(4); padLog("chest: taken");
  g.state = GS_PLAY; g.genShrineOffers(false); g.state = GS_SHRINE; padFrames(16); shot("pad_25_shrine"); press(PB_A); padFrames(4); padLog("shrine: picked");
  for (auto& it : g.inters) if (it.type == I_SHADY) { g.curInter = (int)(&it - &g.inters[0]); g.P.gold = 5000; g.state = GS_SHOP; break; }
  padFrames(16); padLog("shady"); shot("pad_26_shady"); press(PB_B); padFrames(4); padLog("shady: B exits");
  g.banner("Qualidade automática", "FPS 31 - escala de render 65%", Col(255, 214, 80), 3.f); padFrames(12); shot("pad_27_banner");
  g.state = GS_DEAD; g.deathT = 0; padFrames(50); padLog("dead"); shot("pad_28_fim");
  press(PB_RIGHT); press(PB_A); padFrames(10); padLog("end: menu");
}

// CPU profile of a busy scene: time in game.update, in building the draw lists, and the batch counts
// Regression test for the title-screen audio loop: the background demo run must consume its own level-ups.
static int gLevelUpPlays = 0;
static void attractScenario() {
  App& a = gApp; int stuck = 0, maxPending = 0;
  for (int i = 0; i < 30 * 240; i++) {   // 4 minutes of title screen at 30 fps
    a.updateMenuGame(1 / 30.f);
    Game& m = a.menuGame; maxPending = std::max(maxPending, m.pendingLevels);
    if (m.pendingLevels > 0 && m.state == GS_PLAY) stuck++;
  }
  Game& m = a.menuGame;
  LOGI("attract: level %d, max pending %d, frames with unresolved level-up %d", m.P.level, maxPending, stuck);
  if (maxPending > 3 || stuck > 30 * 2) { LOGI("attract: FAIL (level-up never consumed -> audio loop)"); exit(1); }
  LOGI("attract: PASS");
}
static void perfScenario(int map, int ch, float minutes) {
  App& a = gApp; Game& g = a.game;
  for (float at : {2.f, minutes}) {
    startGame(map, 0, ch, true, 99);
    a.applySettingsToGame(g); g.cfg.maxEnemies = a.set.maxEnemies;
    simGame(g, at * 60);
    g.state = GS_PLAY; g.autoPilot = true;
    double tu = 0, tr = 0, ts = 0; int n = 120; long draws = 0, insts = 0, tris = 0; int maxDraws = 0;
    for (int i = 0; i < n; i++) {
      a.ui.begin(W, H, 1, a.plat, 1 / 60.f); gR.set.renderScale = a.set.renderScale; gR.set.viewDist = a.set.viewDist;
      gR.beginFrame(W, H, 1 / 60.f);
      auto t0 = std::chrono::steady_clock::now();
      g.update(1 / 60.f); if (g.state != GS_PLAY) { g.pendingLevels = 0; g.state = GS_PLAY; }
      auto t1 = std::chrono::steady_clock::now();
      g.render(gR);
      auto t2 = std::chrono::steady_clock::now();
      gR.renderScene(true); a.drawGame(); gR.uiFlush();
      auto t3 = std::chrono::steady_clock::now();
      tu += std::chrono::duration<double, std::milli>(t1 - t0).count(); tr += std::chrono::duration<double, std::milli>(t2 - t1).count(); ts += std::chrono::duration<double, std::milli>(t3 - t2).count();
      draws += gR.stats.draws; insts += gR.stats.insts; tris += gR.stats.tris; maxDraws = std::max(maxDraws, gR.stats.draws);
    }
    int alive = 0; for (auto& e : g.enemies) alive += e.alive;
    LOGI("PERF map=%d t=%.0fmin enemies=%d | update %.3f ms | build lists %.3f ms | scene+ui (llvmpipe) %.2f ms | draws %.0f (max %d) inst %.0f tris %.0f | projs %zu areas %zu parts %zu",
         map, at, alive, tu / n, tr / n, ts / n, (double)draws / n, maxDraws, (double)insts / n, (double)tris / n, g.projs.size(), g.areas.size(), g.parts.size());
  }
}

static void gameShots(int map, int tier, int ch, const std::string& tag, float seconds, u32 seed) {
  App& a = gApp;
  startGame(map, tier, ch, true, seed);
  simGame(a.game, seconds);
  a.game.state = GS_PLAY; a.game.autoPilot = true;
  frame(1 / 30.f, 4);
  a.game.autoPilot = false; a.game.state = GS_PLAY; a.game.pendingLevels = 0; a.game.P.xp = 0; frame(1 / 60.f, 1);
  shot("jogo_" + tag);
}

static void balance(int map, int ch, float minutes, u32 seed) {
  App& a = gApp;
  startGame(map, 0, ch, false, seed, minutes);
  Game& g = a.game;
  float t = 0; int lastMin = -1; auto t0 = std::chrono::steady_clock::now();
  while (g.state != GS_DEAD && g.state != GS_VICTORY && t < minutes * 60 + 240) {
    g.update(1 / 30.f); t += 1 / 30.f;
    if (g.state == GS_TRANSITION) g.update(1.f);
    int m = (int)(g.stageTime / 60);
    if (m != lastMin) {
      lastMin = m; int alive = 0; for (auto& e : g.enemies) if (e.alive) alive++;
      double dps = 0; for (double d : g.dmgBySrc) dps += d; dps /= std::max(1.f, g.runTime);
      LOGI("[%s/%s] t=%5.1fmin lvl=%2d hp=%4.0f/%4.0f kills=%5d alive=%3d gold=%6.0f items=%2d wpn=%zu dps=%7.0f hpMult=%.1f boss=%d portal=%d swarm=%d",
           gMaps[map].name, gChars[ch].name, g.stageTime / 60, g.P.level, g.P.hp, g.P.st[S_MAXHP], g.P.kills, alive, g.P.gold, [&] { int n = 0; for (int i = 0; i < IT_COUNT; i++) n += g.P.items[i]; return n; }(), g.P.weapons.size(), dps, g.hpMult(), g.bossSummoned, g.portalOpen, g.finalSwarm);
    }
  }
  double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  LOGI("RESULT map=%s char=%s state=%s time=%.1fmin level=%d kills=%d lastHit=%s(%.0f) maxhp=%.0f armor=%.0f (sim %.1fs)", gMaps[map].name, gChars[ch].name, g.state == GS_DEAD ? "DEAD" : g.state == GS_VICTORY ? "VICTORY" : "TIMEOUT", g.runTime / 60, g.P.level, g.P.kills, g.lastHitBy.c_str(), g.lastHitDmg, g.P.st[S_MAXHP], g.P.st[S_ARMOR], el);
}

int main(int argc, char** argv) {
  std::string scenario = "ui";
  int bmap = 0, bchar = 0; float bmin = 15; u32 seed = 7; bool handheld = false; std::string langCode;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--out") outDir = next();
    else if (a == "--size") { std::string s = next(); sscanf(s.c_str(), "%dx%d", &W, &H); }
    else if (a == "--scenario") scenario = next();
    else if (a == "--map") bmap = atoi(next().c_str());
    else if (a == "--char") bchar = atoi(next().c_str());
    else if (a == "--minutes") bmin = (float)atof(next().c_str());
    else if (a == "--seed") seed = (u32)atoi(next().c_str());
    else if (a == "--handheld") handheld = true;
    else if (a == "--lang") langCode = next();
  }
  mkdir(outDir.c_str(), 0755);
  auto getPlat = (EGLDisplay(*)(EGLenum, void*, const EGLint*))eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy = getPlat(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr);
  EGLint ma, mi; if (!eglInitialize(dpy, &ma, &mi)) { LOGE("EGL init failed"); return 1; }
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, nullptr, nullptr, ca);
  if (!ctx || !eglMakeCurrent(dpy, nullptr, nullptr, ctx)) { LOGE("context failed"); return 1; }
  LOGI("GL: %s | %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenRenderbuffers(1, &colorRb); glBindRenderbuffer(GL_RENDERBUFFER, colorRb); glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, W, H);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRb);
  glGenRenderbuffers(1, &depthRb); glBindRenderbuffer(GL_RENDERBUFFER, depthRb); glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);
  PlatformInfo pi; pi.width = W; pi.height = H; pi.dataDir = "/tmp/omnibonk_harness"; pi.density = 3;
  mkdir(pi.dataDir.c_str(), 0755); remove((pi.dataDir + "/omnibonk_save.txt").c_str());
  if (handheld) { pi.handheld = true; pi.deviceName = "R36S"; }
  auto t0 = std::chrono::steady_clock::now();
  app_init(pi);
  LOGI("init %.2fs", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  app_gl_init();
  gR.screenFbo = fbo;
  gApp.testMode = true;
  if (!handheld) gApp.set = Settings();
  if (handheld) gApp.ui.padMode = true;
  if (!langCode.empty()) { int li = i18nFromCode(langCode.c_str()); if (li >= 0) { gApp.set.lang = li; i18nSet(li); } }
  if (scenario == "ui") uiShots();
  else if (scenario == "pad") padScenario();
  else if (scenario == "attract") attractScenario();
  else if (scenario == "perf") perfScenario(bmap, bchar, bmin);
  else if (scenario == "art") artSheets();
  else if (scenario == "icon") iconShot();
  else if (scenario == "balance") balance(bmap, bchar, bmin, seed);
  else if (scenario == "smoke") { startGame(bmap, 0, bchar, true, seed); simGame(gApp.game, 5); frame(1 / 30.f, 2); shot("smoke"); }
  else if (scenario == "game") {
    gameShots(MAP_FOREST, 1, C_NINJA, "01_forest_deep_ninja_min3", 180, 11);
    gameShots(MAP_FOREST, 0, C_FOX, "02_forest_green_fox_min8", 480, 12);
    gameShots(MAP_DESERT, 0, C_CALCIUM, "03_desert_calcium_min6", 360, 13);
    gameShots(MAP_GRAVEYARD, 0, C_MONKE, "04_graveyard_monke_min5", 300, 14);
    gameShots(MAP_FOREST, 2, C_OGRE, "05_forest_cursed_ogre_min10", 600, 15);
  } else if (scenario == "overlays") {
    App& a = gApp;
    startGame(MAP_FOREST, 1, C_NINJA, true, 21); simGame(a.game, 240);
    Game& g = a.game; g.autoPilot = false; g.state = GS_PLAY;
    g.genLevelOffers(); g.state = GS_LEVELUP; frame(1 / 30.f, 2); shot("ui_09_level_up");
    g.state = GS_PLAY; for (auto& it : g.inters) if (it.type == I_CHEST && !it.used) { g.openChest(it, true); break; }
    g.chestAnim = 2; frame(1 / 30.f, 2); shot("ui_10_bau");
    g.state = GS_PLAY; g.genShrineOffers(false); g.state = GS_SHRINE; frame(1 / 30.f, 2); shot("ui_11_charge_shrine");
    g.state = GS_PAUSE; a.showMap = false; frame(1 / 30.f, 2); shot("ui_12_pausa_build");
    a.showMap = true; frame(1 / 30.f, 2); shot("ui_13_mapa_completo"); a.showMap = false;
    a.set.dpsMeter = true; g.state = GS_PLAY; frame(1 / 30.f, 3); shot("ui_14_hud_dps_contadores");
    g.state = GS_DEAD; g.deathT = 2; frame(1 / 30.f, 2); shot("ui_15_fim_de_run");
  }
  LOGI("done");
  return 0;
}
