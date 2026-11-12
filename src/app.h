// App: screens, settings, persistence, input routing
#pragma once
#include "base.h"
#include "platform.h"
#include "ui.h"
#include "game.h"

struct Settings {
  int lang = 0;               // Lang enum (src/i18n.h); 0 = English (default)
  // gameplay (QoL)
  int stageMin = 15;          // 10,12,15,20,25,30, 0 = infinite (no timer)
  bool scaleStages = true;    // stage 2/3 use 90%/80% of the time like the original
  int swarmMode = 0;          // 0 final swarm when timer ends, 1 only after the boss is dead
  int animFps = 12;           // 6, 8, 12 (default stepped "on twos"), 24 (smooth)
  bool autoBhop = false, instantChests = false, dmgNumbers = true, dpsMeter = false, counters = true, autoCam = true;
  // graphics
  bool autoQuality = true;    // lower render scale / particles when FPS stays low
  float renderScale = 0.75f; bool snap = true; int maxEnemies = 300; int particles = 2; bool shadows = true; int fpsCap = 60; float fxAlpha = 1.f; bool vignette = true; float viewDist = 120; float fov = 62; float camDist = 7.5f; float shake = 1.f;
  // controls
  float camSens = 1.f, joySize = 1.f, uiScale = 1.f, ctrlAlpha = 0.85f; bool leftHanded = false, vibration = true, invertY = false;
  // audio
  float music = 0.55f, sfx = 0.8f; bool showFps = false;
};
struct Meta {
  int silver = 0, runs = 0, wins = 0, totalKills = 0, bestKills = 0; float bestTime[3][3] = {{0}};
  int upReroll = 0, upSkip = 0, upBanish = 0, upHp = 0, upLuck = 0;
  int lastChar = 0, lastMap = 0, lastTier = 0;
  int lastMode = 0, mutators = 0;                       // mode screen selection and active mutators (bit mask)
  int dailyKey = 0, dailyBest = 0, bossRushBest = 0; float endlessBest[3] = {0, 0, 0};
};
enum Screen { SCR_TITLE, SCR_CHARS, SCR_MAP, SCR_SETTINGS, SCR_SHOP, SCR_ABOUT, SCR_CATALOG, SCR_GAME, SCR_CREDITS, SCR_MODES, SCR_MUTATORS };

struct App {
  PlatformInfo plat; Settings set; Meta meta;
  Screen scr = SCR_TITLE, settingsBack = SCR_TITLE; int settingsTab = 0, catTab = 0; float scrollY[8] = {0};
  Game game, menuGame; bool menuInit = false, inRun = false;
  UI ui; bool glReady = false, paused = false;
  float fps = 60, fpsAcc = 0; int fpsFrames = 0; float frameDt = 0.016f;
  int selChar = 0, selMap = 0, selTier = 0, selMode = 0, mutFocus = 0, mutPage = 0, runDailyKey = 0, lastDailyScore = 0;
  float hudR = 0, hudB = 0, hudTR = 0, hudTRB = 0; // last frame's HUD extents (left column, top-right cluster) so world labels avoid them
  int pauseTab = 0; bool showMap = false; bool banishMode = false; bool confirmQuit = false; int microSel = -1;
  // touch controls
  int joyTouch = -1, camTouch = -1, jumpTouch = -1, slideTouch = -1; v2 joyBase, joyCur;
  // gamepad (handhelds): focus navigation in menus, sticks + buttons in game
  PadState pad, padPrev; unsigned padPressed = 0; int navDirX = 0, navDirY = 0, uiKey = -1, padTab = 0;
  float navRepeat = 0, acceptLock = 0, quitHold = 0, menuStable = 0; bool padStart = false, hadSave = false;
  void processPad(float dt);
  bool narrow() const { return ui.W < 400; }
  int stepper(float x, float y, float w, float h, const std::string& value, Col c, float scale = 1);
  // scripted/test hooks (desktop harness)
  bool testMode = false, demoDone = false;
  float realDt = 0;  // unclamped frame time (FPS counter)
  float aqWarm = 0, aqAcc = 0; int aqFrames = 0; // auto quality sampling
  void autoQualityTick();
  void startDemo();
  std::string savePath() const { return plat.dataDir + "/omnibonk_save.txt"; }
  bool load(); void save();
  void startRun();
  void applySettingsToGame(Game& g);
  void frame(float dt);
  void updateMenuGame(float dt);
  void drawModes(); void drawMutators(); void modeNext(); void charsNext();
  void drawTitle(); void drawChars(); void drawMap(); void drawSettings(); void drawShop(); void drawAbout(); void drawCredits(); void drawCatalog();
  void drawGame(); void gameControls(); void drawHUD(); void drawWorldUI(); void drawMinimap(float cx, float cy, float r); void drawFullMap();
  void drawLevelUp(); void drawChest(); void drawShrine(); void drawShopOverlay(); void drawMicrowave(); void drawMoai(); void drawPause(); void drawEnd();
  void drawOfferCard(const Offer& o, float x, float y, float w, float h, bool sel);
  void back();
  void bgDim(float a = 0.55f);
};
extern App gApp;
