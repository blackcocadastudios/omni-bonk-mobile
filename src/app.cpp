#include "app.h"
#include "render.h"
#include "audio.h"
#include <ctime>

App gApp;

// ------------------------------------------------------------------ persistence
void App::save() {
  FILE* f = fopen(savePath().c_str(), "w");
  if (!f) return;
  Settings& s = set; Meta& m = meta;
  fprintf(f, "stageMin=%d\nscaleStages=%d\nswarmMode=%d\nanimFps=%d\nautoBhop=%d\ninstantChests=%d\ndmgNumbers=%d\ndpsMeter=%d\ncounters=%d\nautoCam=%d\n",
          s.stageMin, s.scaleStages, s.swarmMode, s.animFps, s.autoBhop, s.instantChests, s.dmgNumbers, s.dpsMeter, s.counters, s.autoCam);
  fprintf(f, "autoQuality=%d\nrenderScale=%.2f\nsnap=%d\nmaxEnemies=%d\nparticles=%d\nshadows=%d\nfpsCap=%d\nfxAlpha=%.2f\nvignette=%d\nviewDist=%.0f\nfov=%.0f\ncamDist=%.1f\nshake=%.2f\n",
          s.autoQuality, s.renderScale, s.snap, s.maxEnemies, s.particles, s.shadows, s.fpsCap, s.fxAlpha, s.vignette, s.viewDist, s.fov, s.camDist, s.shake);
  fprintf(f, "camSens=%.2f\njoySize=%.2f\nuiScale=%.2f\nctrlAlpha=%.2f\nleftHanded=%d\nvibration=%d\nmusic=%.2f\nsfx=%.2f\nshowFps=%d\n",
          s.camSens, s.joySize, s.uiScale, s.ctrlAlpha, s.leftHanded, s.vibration, s.music, s.sfx, s.showFps);
  fprintf(f, "silver=%d\nruns=%d\nwins=%d\ntotalKills=%d\nbestKills=%d\nupReroll=%d\nupSkip=%d\nupBanish=%d\nupHp=%d\nupLuck=%d\nlastChar=%d\nlastMap=%d\nlastTier=%d\n",
          m.silver, m.runs, m.wins, m.totalKills, m.bestKills, m.upReroll, m.upSkip, m.upBanish, m.upHp, m.upLuck, m.lastChar, m.lastMap, m.lastTier);
  for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) fprintf(f, "best_%d_%d=%.0f\n", a, b, m.bestTime[a][b]);
  fclose(f);
}

void App::load() {
  FILE* f = fopen(savePath().c_str(), "r");
  if (!f) return;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '='); if (!eq) continue; *eq = 0;
    std::string k = line; float v = (float)atof(eq + 1);
    Settings& s = set; Meta& m = meta;
#define I_(name) if (k == #name) s.name = (int)v;
#define B_(name) if (k == #name) s.name = v != 0;
#define F_(name) if (k == #name) s.name = v;
#define M_(name) if (k == #name) m.name = (int)v;
    I_(stageMin) B_(scaleStages) I_(swarmMode) I_(animFps) B_(autoBhop) B_(instantChests) B_(dmgNumbers) B_(dpsMeter) B_(counters) B_(autoCam)
    B_(autoQuality) F_(renderScale) B_(snap) I_(maxEnemies) I_(particles) B_(shadows) I_(fpsCap) F_(fxAlpha) B_(vignette) F_(viewDist) F_(fov) F_(camDist) F_(shake)
    F_(camSens) F_(joySize) F_(uiScale) F_(ctrlAlpha) B_(leftHanded) B_(vibration) F_(music) F_(sfx) B_(showFps)
    M_(silver) M_(runs) M_(wins) M_(totalKills) M_(bestKills) M_(upReroll) M_(upSkip) M_(upBanish) M_(upHp) M_(upLuck) M_(lastChar) M_(lastMap) M_(lastTier)
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) if (k == fmt("best_%d_%d", a, b)) m.bestTime[a][b] = v;
  }
  fclose(f);
  selChar = clampi(meta.lastChar, 0, C_COUNT - 1); selMap = clampi(meta.lastMap, 0, MAP_COUNT - 1); selTier = clampi(meta.lastTier, 0, 2);
}

void App::applySettingsToGame(Game& g) {
  g.animFps = set.animFps; g.dmgNumbers = set.dmgNumbers; g.shakeSetting = set.shake; g.fovDeg = set.fov; g.camDist = set.camDist;
  g.shadowsOn = set.shadows; g.particleLevel = set.particles; g.cfg.autoBhop = set.autoBhop; g.cfg.instantChests = set.instantChests;
}

void App::startRun() {
  RunConfig c;
  c.map = selMap; c.tier = selTier; c.character = selChar;
  c.stageMinutes = (float)set.stageMin; c.scaleStages = set.scaleStages; c.swarmMode = set.swarmMode;
  c.seed = (u32)time(nullptr) ^ (u32)(fpsFrames * 2654435761u);
  c.rerolls = 2 + meta.upReroll; c.skips = 2 + meta.upSkip; c.banishes = 2 + meta.upBanish;
  c.maxEnemies = set.maxEnemies; c.autoBhop = set.autoBhop; c.instantChests = set.instantChests;
  game.start(c);
  game.P.shrineBonus[S_MAXHP] += 10.f * meta.upHp; game.P.shrineBonus[S_LUCK] += 3.f * meta.upLuck;
  game.recomputeStats(); game.P.hp = game.P.st[S_MAXHP];
  applySettingsToGame(game);
  game.autoPilot = false;
  scr = SCR_GAME; inRun = true; paused = false; aqWarm = aqAcc = 0; aqFrames = 0;
  meta.runs++; meta.lastChar = selChar; meta.lastMap = selMap; meta.lastTier = selTier; save();
  audioMusic(1 + selMap);
}

void App::updateMenuGame(float dt) {
  if (!menuInit || menuGame.cfg.map != selMap || menuGame.P.ch != selChar) {
    RunConfig c; c.map = selMap; c.tier = 1; c.character = selChar; c.stageMinutes = 15; c.seed = 4242 + selMap; c.maxEnemies = 70; c.godMode = true;
    menuGame.start(c); menuGame.autoPilot = true; menuGame.stageTime = 200; menuInit = true;
    menuGame.banners.clear();
  }
  applySettingsToGame(menuGame);
  menuGame.dmgNumbers = false;
  menuGame.update(dt);
  if (menuGame.state != GS_PLAY) menuGame.state = GS_PLAY;
  if (menuGame.P.hp < 50) menuGame.P.hp = menuGame.P.st[S_MAXHP];
  menuGame.banners.clear();
  // slow cinematic orbit
  menuGame.camYaw += dt * 0.08f;
}

void App::back() {
  audioPlay(SFX_CLICK);
  if (scr == SCR_GAME) {
    if (showMap) { showMap = false; return; }
    if (game.state == GS_PLAY) { game.prevState = GS_PLAY; game.state = GS_PAUSE; }
    else if (game.state == GS_PAUSE) game.state = GS_PLAY;
    return;
  }
  if (scr == SCR_SETTINGS) { scr = settingsBack; save(); return; }
  if (scr == SCR_TITLE) { confirmQuit = !confirmQuit; return; }
  if (scr == SCR_MAP) { scr = SCR_CHARS; return; }
  scr = SCR_TITLE;
}

void App::bgDim(float a) { ui.rect(0, 0, ui.W, ui.H, Col(8, 4, 16, (u8)(a * 255))); }

// "demo[=char,map]" launch argument: jumps straight into a bot-played run (used by the CI emulator smoke test)
void App::startDemo() {
  demoDone = true;
  int ch = C_FOX, mp = MAP_FOREST;
  size_t p = plat.args.find("demo=");
  if (p != std::string::npos) sscanf(plat.args.c_str() + p + 5, "%d,%d", &ch, &mp);
  selChar = std::max(0, std::min(C_COUNT - 1, ch)); selMap = std::max(0, std::min(MAP_COUNT - 1, mp)); selTier = 0;
  startRun();
  game.cfg.godMode = true; game.start(game.cfg); applySettingsToGame(game); game.autoPilot = true;
}

// mobile auto quality: if gameplay FPS stays well below the target, step the render scale down
// (0.1 at a time, floor 50%), then the particle level
void App::autoQualityTick() {
  if (!set.autoQuality || realDt <= 0 || paused || game.state != GS_PLAY) return;
  aqWarm += realDt;
  if (aqWarm < 4.f) return;               // skip loading hitches at the start of a stage
  aqAcc += realDt; aqFrames++;
  if (aqAcc < 5.f) return;
  float avg = aqFrames / aqAcc, target = std::min(set.fpsCap > 0 ? (float)set.fpsCap : 60.f, 60.f);
  aqAcc = 0; aqFrames = 0;
  if (avg >= target * 0.72f) return;
  if (set.renderScale > 0.51f) {
    set.renderScale = std::max(0.5f, set.renderScale - 0.1f);
    game.banner("Qualidade automática", fmt("FPS %.0f - escala de render %d%%", avg, (int)roundf(set.renderScale * 100)), Col(255, 214, 80), 2.2f);
  } else if (set.particles > 0) {
    set.particles--; applySettingsToGame(game);
    game.banner("Qualidade automática", fmt("FPS %.0f - menos partículas", avg), Col(255, 214, 80), 2.2f);
  } else return;
  save();
}

void App::frame(float dt) {
  if (!demoDone && plat.args.find("demo") != std::string::npos) startDemo();
  frameDt = dt;
  fpsAcc += realDt > 0 ? realDt : dt; fpsFrames++;
  if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }
#ifdef __ANDROID__
  static float logT = 0; logT += realDt;
  if (logT > 10.f) { logT = 0; LOGI("fps %.1f | screen %d | %dx%d | enemies %d | t %.0fs", fps, (int)scr, plat.width, plat.height, (int)game.enemies.size(), game.runTime); }
#endif
  ui.begin(plat.width, plat.height, set.uiScale, plat, dt);
  gR.set.renderScale = set.renderScale; gR.set.snap = set.snap; gR.set.fxAlpha = set.fxAlpha; gR.set.vignette = set.vignette ? 0.35f : 0; gR.set.viewDist = set.viewDist;
  audioSetVolume(set.music, set.sfx);
  gR.beginFrame(plat.width, plat.height, dt);
  if (scr == SCR_GAME) {
    if (!testMode) gameControls();
    if (!paused) game.update(dt);
    autoQualityTick();
    audioMusic(game.bossesAlive > 0 || game.finalArena ? 4 : 1 + game.cfg.map);
    audioSetIntensity(game.u());
    game.render(gR);
    gR.renderScene(true);
    drawGame();
  } else {
    updateMenuGame(dt);
    menuGame.render(gR);
    gR.renderScene(true);
    audioMusic(0);
    switch (scr) {
      case SCR_TITLE: drawTitle(); break;
      case SCR_CHARS: drawChars(); break;
      case SCR_MAP: drawMap(); break;
      case SCR_SETTINGS: drawSettings(); break;
      case SCR_SHOP: drawShop(); break;
      case SCR_ABOUT: drawAbout(); break;
      case SCR_CATALOG: drawCatalog(); break;
      default: break;
    }
  }
  if (set.showFps) ui.text(ui.W - 4 - ui.safeR, ui.H - 12, fmt("%d FPS", (int)roundf(fps)), 1, Col(160, 255, 160), AL_RIGHT);
  gR.uiFlush();
  ui.end();
}

// ------------------------------------------------------------------ platform hooks
static bool gInit = false;
void app_init(const PlatformInfo& info) {
  gApp.plat = info;
  if (!gInit) {
    dataInit();
    gModels.build();
    artBuildAtlas(gR.atlas, buildIconList());
    audioInit(48000);
    gApp.load();
    gInit = true;
  }
}
void app_gl_init() { gR.init(); gR.curWorld = nullptr; gR.curGen = -1; gApp.glReady = true; }
void app_gl_lost() { gR.destroy(); gApp.glReady = false; }
void app_resize(int w, int h) { gApp.plat.width = w; gApp.plat.height = h; }
void app_event(const InputEv& e) {
  if (e.type == EV_BACK) { gApp.back(); return; }
  gApp.ui.event(e);
}
void app_frame(float dt) { gApp.realDt = dt; if (gApp.glReady) gApp.frame(std::min(dt, 0.05f)); }
void app_pause() { if (gApp.scr == SCR_GAME && gApp.game.state == GS_PLAY) gApp.game.state = GS_PAUSE; gApp.save(); }
void app_resume() {}
void app_save() { gApp.save(); }
void app_audio(float* stereo, int frames, int rate) { (void)rate; audioRender(stereo, frames); }
int app_fps_cap() { return gApp.set.fpsCap; }
void app_set_insets(float l, float r, float t, float b) { gApp.plat.safeLeft = l; gApp.plat.safeRight = r; gApp.plat.safeTop = t; gApp.plat.safeBottom = b; }
void plat_vibrate_safe(int ms) { if (gApp.set.vibration && !gApp.testMode) plat_vibrate(ms); }
