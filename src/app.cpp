#include "i18n.h"
#include "app.h"
#include "render.h"
#include "audio.h"
#include "music.h"
#include <ctime>

App gApp;

// ------------------------------------------------------------------ persistence
void App::save() {
  FILE* f = fopen(savePath().c_str(), "w");
  if (!f) return;
  Settings& s = set; Meta& m = meta;
  fprintf(f, "lang=%d\n", s.lang);
  fprintf(f, "stageMin=%d\nscaleStages=%d\nswarmMode=%d\nanimFps=%d\nautoBhop=%d\ninstantChests=%d\ndmgNumbers=%d\ndpsMeter=%d\ncounters=%d\nautoCam=%d\n",
          s.stageMin, s.scaleStages, s.swarmMode, s.animFps, s.autoBhop, s.instantChests, s.dmgNumbers, s.dpsMeter, s.counters, s.autoCam);
  fprintf(f, "autoQuality=%d\nrenderScale=%.2f\nsnap=%d\nmaxEnemies=%d\nparticles=%d\nshadows=%d\nfpsCap=%d\nfxAlpha=%.2f\nvignette=%d\nviewDist=%.0f\nfov=%.0f\ncamDist=%.1f\nshake=%.2f\n",
          s.autoQuality, s.renderScale, s.snap, s.maxEnemies, s.particles, s.shadows, s.fpsCap, s.fxAlpha, s.vignette, s.viewDist, s.fov, s.camDist, s.shake);
  fprintf(f, "camSens=%.2f\njoySize=%.2f\nuiScale=%.2f\nctrlAlpha=%.2f\nleftHanded=%d\nvibration=%d\ninvertY=%d\nmusic=%.2f\nsfx=%.2f\nshowFps=%d\n",
          s.camSens, s.joySize, s.uiScale, s.ctrlAlpha, s.leftHanded, s.vibration, s.invertY, s.music, s.sfx, s.showFps);
  fprintf(f, "silver=%d\nruns=%d\nwins=%d\ntotalKills=%d\nbestKills=%d\nupReroll=%d\nupSkip=%d\nupBanish=%d\nupHp=%d\nupLuck=%d\nlastChar=%d\nlastMap=%d\nlastTier=%d\n",
          m.silver, m.runs, m.wins, m.totalKills, m.bestKills, m.upReroll, m.upSkip, m.upBanish, m.upHp, m.upLuck, m.lastChar, m.lastMap, m.lastTier);
  for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) fprintf(f, "best_%d_%d=%.0f\n", a, b, m.bestTime[a][b]);
  fprintf(f, "lastMode=%d\nmutators=%d\ndailyKey=%d\ndailyBest=%d\nbossRushBest=%d\nreverseBest=%d\nreverseWins=%d\n", m.lastMode, m.mutators, m.dailyKey, m.dailyBest, m.bossRushBest, m.reverseBest, m.reverseWins);
  for (int a = 0; a < 3; a++) fprintf(f, "endless_%d=%.0f\n", a, m.endlessBest[a]);
  fclose(f);
}

bool App::load() {
  FILE* f = fopen(savePath().c_str(), "r");
  if (!f) return false;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    char* eq = strchr(line, '='); if (!eq) continue; *eq = 0;
    std::string k = line; float v = (float)atof(eq + 1);
    Settings& s = set; Meta& m = meta;
#define I_(name) if (k == #name) s.name = (int)v;
#define B_(name) if (k == #name) s.name = v != 0;
#define F_(name) if (k == #name) s.name = v;
#define M_(name) if (k == #name) m.name = (int)v;
    I_(lang) I_(stageMin) B_(scaleStages) I_(swarmMode) I_(animFps) B_(autoBhop) B_(instantChests) B_(dmgNumbers) B_(dpsMeter) B_(counters) B_(autoCam)
    B_(autoQuality) F_(renderScale) B_(snap) I_(maxEnemies) I_(particles) B_(shadows) I_(fpsCap) F_(fxAlpha) B_(vignette) F_(viewDist) F_(fov) F_(camDist) F_(shake)
    F_(camSens) F_(joySize) F_(uiScale) F_(ctrlAlpha) B_(leftHanded) B_(vibration) B_(invertY) F_(music) F_(sfx) B_(showFps)
    M_(silver) M_(runs) M_(wins) M_(totalKills) M_(bestKills) M_(upReroll) M_(upSkip) M_(upBanish) M_(upHp) M_(upLuck) M_(lastChar) M_(lastMap) M_(lastTier)
    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) if (k == fmt("best_%d_%d", a, b)) m.bestTime[a][b] = v;
    M_(lastMode) M_(dailyBest) M_(bossRushBest) M_(reverseBest) M_(reverseWins)
    if (k == "mutators") m.mutators = (int)strtoul(eq + 1, nullptr, 10);
    if (k == "dailyKey") m.dailyKey = atoi(eq + 1);
    for (int a = 0; a < 3; a++) if (k == fmt("endless_%d", a)) m.endlessBest[a] = v;
  }
  fclose(f);
  set.lang = clampi(set.lang, 0, LANG_COUNT - 1); i18nSet(set.lang);
  selChar = clampi(meta.lastChar, 0, C_COUNT - 1); selMap = clampi(meta.lastMap, 0, MAP_COUNT - 1); selTier = clampi(meta.lastTier, 0, 2);
  selMode = clampi(meta.lastMode, 0, MODE_COUNT - 1); meta.mutators &= (int)((1u << MU_COUNT) - 1);
  return true;
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
  c.mode = selMode; c.mutators = (u32)meta.mutators;
  switch (selMode) {
    case MODE_DAILY: {
      DailySetup d = dailySetup(dailyKeyToday()); runDailyKey = d.key;
      c.map = d.map; c.tier = 0; c.character = d.character; c.seed = d.seed; c.mutators = d.mutators;
      c.stageMinutes = 10; c.scaleStages = true; c.swarmMode = 0;
    } break;
    case MODE_BOSSRUSH: c.map = MAP_FOREST; c.tier = 0; c.stageMinutes = 0; c.scaleStages = false; c.swarmMode = 0; break;
    case MODE_ENDLESS: c.stageMinutes = 0; break;
    case MODE_GAUNTLET: c.character = (int)(hash32(c.seed) % C_COUNT); break;
    case MODE_SHRINEHUNT: c.stageMinutes = 6; c.scaleStages = false; break;
    case MODE_REVERSE: c.character = C_OGRE; c.stageMinutes = 10; c.scaleStages = false; c.swarmMode = 0; break;
    default: break;
  }
  game.start(c);
  game.P.shrineBonus[S_MAXHP] += 10.f * meta.upHp; game.P.shrineBonus[S_LUCK] += 3.f * meta.upLuck;
  game.recomputeStats(); game.P.hp = game.P.st[S_MAXHP];
  applySettingsToGame(game);
  game.autoPilot = false;
  scr = SCR_GAME; inRun = true; paused = false; aqWarm = aqAcc = 0; aqFrames = 0;
  meta.runs++; meta.lastChar = selChar; meta.lastMap = selMap; meta.lastTier = selTier; meta.lastMode = selMode; save();
  audioMusic(1 + game.cfg.map);
}

void App::updateMenuGame(float dt) {
  bool differs = menuGame.cfg.map != selMap || menuGame.P.ch != selChar;
  menuStable = differs ? menuStable + dt : 0;   // wait until the selection settles: regenerating the world is slow on handhelds
  if (!menuInit || (differs && menuStable > (plat.handheld ? 0.6f : 0.f))) {
    menuStable = 0;
    RunConfig c; c.map = selMap; c.tier = 1; c.character = selChar; c.stageMinutes = 15; c.seed = 4242 + selMap; c.maxEnemies = 70; c.godMode = true;
    menuGame.start(c); menuGame.autoPilot = true; menuGame.stageTime = 200; menuInit = true;
    menuGame.banners.clear();
  }
  applySettingsToGame(menuGame);
  menuGame.dmgNumbers = false;
  // Attract mode: the autopilot resolves level-ups, chests and shrines by itself inside Game::update().
  // Never force the state back to GS_PLAY here: that left pendingLevels > 0 forever, so every frame
  // regenerated the offers and replayed the level-up jingle (the endless sound loop on the title screen).
  audioAttract(true);                 // soften/rate-limit gameplay SFX coming from the background run
  menuGame.update(dt);
  audioAttract(false);
  if (menuGame.state == GS_DEAD || menuGame.state == GS_VICTORY) menuInit = false;   // restart the demo run
  if (menuGame.P.hp < 50) menuGame.P.hp = menuGame.P.st[S_MAXHP];
  menuGame.banners.clear();
  // slow cinematic orbit
  menuGame.camYaw += dt * 0.08f;
}

void App::back() {
  audioPlay(SFX_CLICK);
  if (scr == SCR_GAME) {
    if (showMap) { showMap = false; game.state = GS_PLAY; return; }
    if (game.state == GS_PLAY) { game.prevState = GS_PLAY; game.state = GS_PAUSE; }
    else if (game.state == GS_PAUSE) game.state = GS_PLAY;
    else if (game.state == GS_SHOP || game.state == GS_MICROWAVE) { game.state = GS_PLAY; microSel = -1; }
    else if (game.state == GS_LEVELUP && banishMode) banishMode = false;
    return;
  }
  if (scr == SCR_SETTINGS) { scr = settingsBack; save(); return; }
  if (scr == SCR_TITLE) { confirmQuit = !confirmQuit; return; }
  if (scr == SCR_MAP) { scr = (selMode == MODE_GAUNTLET || selMode == MODE_REVERSE) ? SCR_MODES : SCR_CHARS; return; }
  if (scr == SCR_CHARS || scr == SCR_MUTATORS) { scr = SCR_MODES; save(); return; }
  if (scr == SCR_CREDITS) { scr = SCR_ABOUT; return; }
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
    game.alert(tr("Auto quality"), fmt(tr("FPS %.0f - render scale %d%%"), avg, (int)roundf(set.renderScale * 100)), Col(255, 214, 80), 2.2f);
  } else if (set.particles > 0) {
    set.particles--; applySettingsToGame(game);
    game.alert(tr("Auto quality"), fmt(tr("FPS %.0f - fewer particles"), avg), Col(255, 214, 80), 2.2f);
  } else if (set.viewDist > 71) {
    set.viewDist = set.viewDist > 91 ? 90.f : 70.f;
    game.alert(tr("Auto quality"), fmt(tr("FPS %.0f - view distance %dm"), avg, (int)set.viewDist), Col(255, 214, 80), 2.2f);
  } else if (set.maxEnemies > 150) {
    set.maxEnemies = set.maxEnemies > 220 ? 220 : 150; game.cfg.maxEnemies = set.maxEnemies;
    game.alert(tr("Auto quality"), fmt(tr("FPS %.0f - max. %d enemies"), avg, set.maxEnemies), Col(255, 214, 80), 2.2f);
  } else return;
  save();
}

// gamepad: menus are driven by focus navigation (D-pad / left stick, A accepts, B goes back,
// L1/R1 switch tabs, Start is the screen's main action); in play the sticks and buttons go to the game
void App::processPad(float dt) {
  unsigned b = pad.buttons;
  padPressed = b & ~padPrev.buttons; padPrev = pad;
  bool stick = fabsf(pad.lx) > 0.5f || fabsf(pad.ly) > 0.5f || fabsf(pad.rx) > 0.5f || fabsf(pad.ry) > 0.5f;
  if (plat.handheld || padPressed || stick) ui.padMode = true;
  if (!plat.handheld) for (auto& t : ui.touches) if (t.began) ui.padMode = false;
  // Select + Start: save and quit (same combo PortMaster ports use)
  if ((b & PB_SELECT) && (b & PB_START)) { quitHold += dt; if (quitHold > 0.3f) { quitHold = -100; save(); plat_quit(); } } else quitHold = 0;
  bool playing = scr == SCR_GAME && game.state == GS_PLAY;
  // new screen / overlay: focus goes to its default widget, and accept is locked for a moment
  // so a jump press does not pick a level-up card the instant it appears
  int key = (int)scr * 1000 + (scr == SCR_GAME ? (int)game.state * 4 + (showMap ? 1 : 0) : (confirmQuit ? 1 : 0));
  if (key != uiKey) {
    uiKey = key; ui.resetFocus();
    acceptLock = scr != SCR_GAME ? 0.12f : (game.state == GS_DEAD || game.state == GS_VICTORY) ? 1.2f : 0.4f;
  }
  acceptLock = std::max(0.f, acceptLock - dt);
  int nx = 0, ny = 0; bool accept = false; float scroll = 0;
  padStart = false; padTab = 0;
  if (ui.padMode && !playing) {
    if ((b & PB_UP) || pad.ly < -0.6f) ny = -1; else if ((b & PB_DOWN) || pad.ly > 0.6f) ny = 1;
    else if ((b & PB_LEFT) || pad.lx < -0.6f) nx = -1; else if ((b & PB_RIGHT) || pad.lx > 0.6f) nx = 1;
    if (nx != navDirX || ny != navDirY) { navDirX = nx; navDirY = ny; navRepeat = 0.38f; }
    else if (nx || ny) { navRepeat -= dt; if (navRepeat <= 0) navRepeat = 0.12f; else nx = ny = 0; }
    if (acceptLock <= 0) {
      accept = (padPressed & PB_A) != 0;
      padStart = (padPressed & PB_START) != 0;
      padTab = ((padPressed & PB_R1) ? 1 : 0) - ((padPressed & PB_L1) ? 1 : 0);
      if (padPressed & PB_B) back();
    }
    if (fabsf(pad.ry) > 0.25f) scroll = -pad.ry * 170 * dt;
  } else navDirX = navDirY = 0;
  ui.padInput(nx, ny, accept, scroll);
}

void App::frame(float dt) {
  processPad(dt);
  if (!demoDone && plat.args.find("demo") != std::string::npos) startDemo();
  frameDt = dt;
  fpsAcc += realDt > 0 ? realDt : dt; fpsFrames++;
  if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }
#ifdef __ANDROID__
  static int lastScr = -1;
  if ((int)scr != lastScr) {
    static const char* kScrNames[] = {"TITLE", "CHARS", "MAP", "SETTINGS", "SHOP", "ABOUT", "CATALOG", "GAME", "CREDITS", "MODES", "MUTATORS"};
    LOGI("screen %s -> %s", lastScr < 0 ? "-" : kScrNames[lastScr], kScrNames[(int)scr]);
    lastScr = (int)scr;
  }
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
    audioMusic(game.finalArena ? 5 : game.bossesAlive > 0 ? 4 : 1 + game.cfg.map);
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
      case SCR_MODES: drawModes(); break;
      case SCR_MUTATORS: drawMutators(); break;
      case SCR_CHARS: drawChars(); break;
      case SCR_MAP: drawMap(); break;
      case SCR_SETTINGS: drawSettings(); break;
      case SCR_SHOP: drawShop(); break;
      case SCR_ABOUT: drawAbout(); break;
      case SCR_CREDITS: drawCredits(); break;
      case SCR_CATALOG: drawCatalog(); break;
      default: break;
    }
  }
  if (set.showFps) ui.text(ui.W - 4 - ui.safeR, ui.H - 12, fmt("%d FPS", (int)roundf(fps)), 1, Col(160, 255, 160), AL_RIGHT);
  ui.drawFocus();
  gR.uiFlush();
  ui.end();
}

// ------------------------------------------------------------------ platform hooks
static bool gInit = false;
static int gPackTracks = 0;
void app_init(const PlatformInfo& info) {
  gApp.plat = info;
  if (!gInit) {
    dataInit();
    gModels.build();
    artBuildAtlas(gR.atlas, buildIconList());
    audioInit(48000);
    gPackTracks = audioMusicPack(info.musicDir);
    gApp.hadSave = gApp.load();
    if (info.handheld && !gApp.hadSave) { // first run on a handheld (RK3326 class): lighter defaults
      Settings& s = gApp.set; s.maxEnemies = 220; s.viewDist = 90; s.particles = 1; s.renderScale = 0.75f; s.vibration = false;
    }
    { // "lang=<code>" launch argument (e.g. lang=pt-BR) overrides the saved language for this run (screenshots, smoke tests)
      size_t lp = info.args.find("lang=");
      if (lp != std::string::npos) { int li = i18nFromCode(info.args.substr(lp + 5, info.args.find(' ', lp) - lp - 5).c_str()); if (li >= 0) { gApp.set.lang = li; i18nSet(li); } }
    }
    gInit = true;
  }
}
void app_gl_init() { gR.init(); gR.curWorld = nullptr; gR.curGen = -1; gApp.glReady = true; }
void app_gl_lost() { gR.destroy(); gApp.glReady = false; }
void app_resize(int w, int h) { gApp.plat.width = w; gApp.plat.height = h; }
void app_gamepad(const PadState& s) { gApp.pad = s; }
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
std::string app_status() {
  const App& a = gApp; const Renderer::Stats& st = gR.lastStats;
  std::string s = fmt("screen %d state %d | %dx%d scale %.2f | enemies %d | draws %d inst %d tris %d | t %.0fs", (int)a.scr, (int)a.game.state, a.plat.width, a.plat.height,
                      a.set.renderScale, (int)a.game.enemies.size(), st.draws, st.insts, st.tris, a.game.runTime);
  if (gPackTracks) s += fmt(" | music pack underruns %d", musicPackUnderruns());
  return s;
}
void app_set_insets(float l, float r, float t, float b) { gApp.plat.safeLeft = l; gApp.plat.safeRight = r; gApp.plat.safeTop = t; gApp.plat.safeBottom = b; }
void plat_vibrate_safe(int ms) { if (gApp.set.vibration && !gApp.testMode) plat_vibrate(ms); }
