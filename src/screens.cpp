// All UI screens, HUD, touch controls and overlays
#include "i18n.h"
#include "app.h"
#include "render.h"
#include "audio.h"

static const Col WHITE(255, 255, 255), GREY(170, 170, 185), DIM(120, 120, 135), GOLD(255, 214, 80), SILV(220, 225, 235), RED(255, 80, 80), GREEN(110, 240, 120), CYAN(90, 210, 255);
static Col RC(int r) { return hexc(kRarityColor[r]); }
static const int kStageOpts[] = {10, 12, 15, 20, 25, 30, 0};

static std::string stageLabel(int m) { return m <= 0 ? tr("INFINITE") : fmt("%d:00", m); }

// "< value >" selector. Touch: two arrow buttons. Gamepad: one focusable, left/right (or A) changes the value.
int App::stepper(float x, float y, float w, float h, const std::string& value, Col c, float scale) {
  UI& u = ui; int d = 0;
  if (u.padMode) {
    u.nextFlags |= FOCUS_ADJUST;
    if (u.clicked(x, y, w, h)) d = 1;
    int a = u.adjust(x, y, w, h); if (a) d = a;
    bool f = u.isFocus(x, y, w, h);
    u.panel(x, y, w, h, f ? Col(70, 50, 110, 240) : Col(30, 24, 44, 230), Col(255, 255, 255, 70));
    u.text(x + 4, y + (h - 10) * 0.5f, "<", 1, f ? WHITE : DIM); u.text(x + w - 4, y + (h - 10) * 0.5f, ">", 1, f ? WHITE : DIM, AL_RIGHT);
    if (d) audioPlay(SFX_CLICK);
  } else {
    if (u.button(x, y, 18, h, "<")) d = -1;
    if (u.button(x + w - 18, y, 18, h, ">")) d = 1;
  }
  u.text(x + w * 0.5f, y + (h - 10 * scale) * 0.5f, u.fit(value, w - 22, scale), scale, c, AL_CENTER);
  return d;
}

// ================================================================== TITLE
void App::drawTitle() {
  UI& u = ui;
  bgDim(0.25f);
  float cx = u.W * 0.5f;
  float ty = u.H * 0.12f;
  float sc = u.W > 520 ? 5 : 4;
  // logo with layered shadow
  u.text(cx + 2, ty + 3, "OMNI BONK", sc, Col(40, 10, 60, 230), AL_CENTER);
  u.text(cx, ty, "OMNI BONK", sc, Col(255, 214, 80), AL_CENTER);
  bool hh = plat.handheld, on = !confirmQuit; // buttons under the quit dialog are inert
  u.text(cx, ty + 12 * sc + 2, (hh ? (plat.deviceName.empty() ? std::string("HANDHELD") : plat.deviceName) : std::string("MOBILE")) + "  •  survivors 3D low-poly", 1, Col(220, 200, 255), AL_CENTER);
  float bw = hh ? 170 : 150, bh = 22, by = ty + 12 * sc + 18;
  Col cb(70, 70, 120, 235);
  if (u.button(cx - bw / 2, by, bw, bh, tr("PLAY"), Col(60, 150, 70, 240), 1.5f, on) || (on && padStart)) scr = SCR_CHARS;
  by += bh + 5;
  if (u.button(cx - bw / 2, by, bw, bh, tr("CATALOG"), cb, 1, on)) { scr = SCR_CATALOG; scrollY[1] = 0; }
  by += bh + 5;
  if (u.button(cx - bw / 2, by, bw, bh, tr("SHOP"), cb, 1, on)) scr = SCR_SHOP;
  by += bh + 5;
  if (hh) { // no OS back gesture on a handheld: explicit quit button
    float w3 = (bw - 8) / 3;
    if (u.button(cx - bw / 2, by, w3, bh, tr("OPTIONS"), cb, 1, on)) { settingsBack = SCR_TITLE; scr = SCR_SETTINGS; scrollY[0] = 0; }
    if (u.button(cx - bw / 2 + w3 + 4, by, w3, bh, tr("ABOUT"), cb, 1, on)) scr = SCR_ABOUT;
    if (u.button(cx - bw / 2 + 2 * (w3 + 4), by, w3, bh, tr("QUIT"), Col(120, 50, 60, 235), 1, on)) confirmQuit = true;
  } else {
    if (u.button(cx - bw / 2, by, bw / 2 - 2, bh, tr("OPTIONS"), cb, 1, on)) { settingsBack = SCR_TITLE; scr = SCR_SETTINGS; scrollY[0] = 0; }
    if (u.button(cx + 2, by, bw / 2 - 2, bh, tr("ABOUT"), cb, 1, on)) scr = SCR_ABOUT;
  }
  u.icon(ICON_UI0 + UI_SILVER, 6 + u.safeL, u.H - 18, 12);
  u.text(21 + u.safeL, u.H - 15, fmtNum(meta.silver), 1, SILV);
  u.text(u.W - 6 - u.safeR, u.H - 15, hh ? tr("v1.1  A: ok   B: back") : "v1.1  fan port", 1, DIM, AL_RIGHT);
  if (confirmQuit) {
    bgDim(0.6f);
    u.panel(cx - 100, u.H * 0.35f, 200, 70);
    u.text(cx, u.H * 0.35f + 10, tr("Quit the game?"), 1.5f, WHITE, AL_CENTER);
    if (u.button(cx - 90, u.H * 0.35f + 38, 85, 22, tr("YES"), Col(150, 50, 50, 240))) { save(); plat_quit(); }
    u.nextFlags = FOCUS_DEFAULT;
    if (u.button(cx + 5, u.H * 0.35f + 38, 85, 22, tr("NO"), Col(70, 70, 120, 240))) confirmQuit = false;
  }
}

// ================================================================== CHARACTER SELECT
void App::drawChars() {
  UI& u = ui;
  bgDim(0.35f);
  float L = 8 + u.safeL, R = u.W - 8 - u.safeR, Wd = R - L;
  u.text(u.W * 0.5f, 8, tr("CHOOSE YOUR CHARACTER"), 2, WHITE, AL_CENTER);
  int n = C_COUNT;
  float cw = std::min(92.f, (Wd - (n - 1) * 5) / n), ch = 86;
  float x0 = u.W * 0.5f - (n * cw + (n - 1) * 5) * 0.5f, y0 = 34;
  for (int i = 0; i < n; i++) {
    const CharDef& c = gChars[i];
    float x = x0 + i * (cw + 5);
    bool sel = i == selChar;
    u.panel(x, y0, cw, ch, sel ? Col(70, 50, 110, 240) : Col(20, 14, 30, 220), sel ? GOLD : Col(255, 255, 255, 60));
    u.icon(ICON_UI0 + UI_CHAR0 + i, x + cw * 0.5f - 16, y0 + 6, 32);
    u.text(x + cw * 0.5f, y0 + 42, c.name, 1, sel ? GOLD : WHITE, AL_CENTER);
    u.icon(ICON_WEAPON0 + c.weapon, x + cw * 0.5f - 9, y0 + 56, 18);
    u.text(x + cw * 0.5f, y0 + 75, gWeapons[c.weapon].name, 1, GREY, AL_CENTER);
    if (sel) u.nextFlags = FOCUS_DEFAULT;
    bool ck = u.clicked(x, y0, cw, ch);
    if (u.isFocus(x, y0, cw, ch)) selChar = i;                       // gamepad: moving the focus selects
    if (ck) { selChar = i; audioPlay(SFX_CLICK); if (u.padMode) scr = SCR_MAP; }
  }
  // details
  const CharDef& c = gChars[selChar];
  float dy = y0 + ch + 6, dh = u.H - dy - 34;
  u.panel(L, dy, Wd, dh);
  u.text(L + 8, dy + 6, std::string(tr("Passive: ")) + c.passive, 1, GOLD);
  float yy = dy + 18 + u.textBox(L + 8, dy + 18, Wd * 0.55f, c.passiveDesc, 1, WHITE, AL_LEFT, 11);
  { std::vector<std::string> bl; int n = u.wrap(c.blurb, Wd * 0.55f, 1, bl); if (yy + 3 + n * 11 <= dy + dh - 3) u.textBox(L + 8, yy + 3, Wd * 0.55f, c.blurb, 1, GREY, AL_LEFT, 11); }
  float sx = L + Wd * 0.6f, sy = dy + 6;
  u.text(sx, sy, tr("Stats"), 1, CYAN); sy += 12;
  for (auto& m : c.stats) { u.text(sx, sy, u.fit(statValueStr(m.stat, m.v) + " " + tr(kStats[m.stat].name), R - sx - 4), 1, m.v >= 0 ? GREEN : RED); sy += 11; if (sy > dy + dh - 8) break; }
  if (u.button(L, u.H - 28, 90, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_TITLE;
  if (u.button(R - 120, u.H - 28, 120, 22, tr("NEXT"), Col(60, 150, 70, 240)) || padStart) scr = SCR_MAP;
}

// ================================================================== MAP SELECT
void App::drawMap() {
  UI& u = ui;
  bgDim(0.35f);
  float L = 8 + u.safeL, R = u.W - 8 - u.safeR, Wd = R - L;
  u.text(u.W * 0.5f, 8, tr("MAP, TIER AND TIME"), 2, WHITE, AL_CENTER);
  float cw = std::min(150.f, (Wd - 10) / 3), ch = 70, x0 = u.W * 0.5f - (cw * 3 + 10) * 0.5f, y0 = 32;
  for (int m = 0; m < MAP_COUNT; m++) {
    float x = x0 + m * (cw + 5); bool sel = m == selMap;
    const Palette& p = gMaps[m].tiers[selTier].pal;
    u.panel(x, y0, cw, ch, Col(20, 14, 30, 230), sel ? GOLD : Col(255, 255, 255, 60));
    // palette preview: sky gradient + ground
    for (int k = 0; k < 8; k++) u.rect(x + 3, y0 + 3 + k * 3, cw - 6, 3, mixc(hexc(p.skyTop), hexc(p.skyHorizon), k / 7.f));
    u.rect(x + 3, y0 + 27, cw - 6, 6, hexc(p.foliage2)); u.rect(x + 3, y0 + 33, cw - 6, 4, hexc(p.ground1));
    u.text(x + cw * 0.5f, y0 + 41, gMaps[m].name, 1.5f, sel ? GOLD : WHITE, AL_CENTER);
    u.text(x + cw * 0.5f, y0 + 57, gMaps[m].tiers[selTier].name, 1, GREY, AL_CENTER);
    if (sel) u.nextFlags = FOCUS_DEFAULT;
    bool ck = u.clicked(x, y0, cw, ch);
    if (u.isFocus(x, y0, cw, ch)) selMap = m;
    if (ck) { selMap = m; audioPlay(SFX_CLICK); }
  }
  float y = y0 + ch + 8;
  bool nar = narrow();
  const char* tierInfo[3] = {tr("1 stage"), tr("2 stages"), nar ? tr("3 + final boss") : tr("3 stages + final boss")};
  float tw = std::min(120.f, (Wd - 10) / 3);
  for (int t = 0; t < 3; t++) {
    float x = u.W * 0.5f - (tw * 3 + 10) * 0.5f + t * (tw + 5);
    if (u.button(x, y, tw, 20, fmt("TIER %d", t + 1), t == selTier ? Col(150, 110, 40, 240) : Col(50, 44, 70, 235))) selTier = t;
    u.text(x + tw * 0.5f, y + 23, tierInfo[t], 1, t == selTier ? GOLD : DIM, AL_CENTER);
  }
  y += 38;
  // stage timer (main request: adjustable, default 15 min)
  u.panel(L, y, Wd, nar ? 60 : 44);
  u.text(L + 8, y + 6, tr("TIME PER STAGE"), 1, CYAN);
  int idx = 0; for (int i = 0; i < 7; i++) if (kStageOpts[i] == set.stageMin) idx = i;
  float vx = nar ? R - 124 : L + 130;
  if (int d = stepper(vx, y + 3, nar ? 120 : 100, 16, stageLabel(set.stageMin), GOLD, 1.5f)) { idx = (idx + d + 7) % 7; set.stageMin = kStageOpts[idx]; }
  std::string stages;
  if (set.stageMin > 0) for (int s = 0; s <= selTier; s++) { float m = set.stageMin * (set.scaleStages ? (s == 0 ? 1.f : s == 1 ? 0.9f : 0.8f) : 1.f); stages += (s ? "  /  " : "") + fmtTime(m * 60); }
  else stages = tr("no timer, no automatic Final Swarm");
  u.text(L + 8, y + 25, std::string(tr("Stages: ")) + stages, 1, GREY);
  float tgw = nar ? (Wd - 12) / 2 : 96, tx0 = nar ? L + 4 : R - 100, tx1 = nar ? L + 8 + tgw : R - 100, ty0 = nar ? y + 40 : y + 3, ty1 = nar ? y + 40 : y + 23;
  if (u.button(tx0, ty0, tgw, 16, set.swarmMode == 0 ? tr("Swarm: at the end") : tr("Swarm: after boss"), Col(50, 44, 70, 235))) set.swarmMode ^= 1;
  if (u.button(tx1, ty1, tgw, 16, set.scaleStages ? tr("Scale: YES") : tr("Scale: NO"), Col(50, 44, 70, 235))) set.scaleStages = !set.scaleStages;
  if (u.button(L, u.H - 28, 90, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_CHARS;
  if (u.button(R - 130, u.H - 28, 130, 22, tr("START!"), Col(60, 150, 70, 240), 1.5f) || padStart) { save(); startRun(); }
}

// ================================================================== SETTINGS
struct OptRow { std::string label, value; std::function<void(int)> change; std::string hint; };
void App::drawSettings() {
  UI& u = ui;
  bgDim(0.6f);
  float L = 8 + u.safeL, R = u.W - 8 - u.safeR, Wd = R - L;
  u.text(u.W * 0.5f, 6, tr("SETTINGS"), 2, WHITE, AL_CENTER);
  const char* tabs[4] = {tr("GAME"), tr("GRAPHICS"), tr("CONTROLS"), tr("AUDIO")};
  float tw = (Wd - 15) / 4;
  if (padTab) { settingsTab = (settingsTab + padTab + 4) % 4; scrollY[0] = 0; audioPlay(SFX_CLICK); }
  for (int t = 0; t < 4; t++) if (u.button(L + t * (tw + 5), 28, tw, 18, tabs[t], t == settingsTab ? Col(150, 110, 40, 240) : Col(50, 44, 70, 235))) { settingsTab = t; scrollY[0] = 0; }
  bool pm = u.padMode;
  std::vector<OptRow> rows;
  auto onoff = [](bool b) { return std::string(b ? tr("ON") : tr("OFF")); };
  auto cyc = [](float v, std::vector<float> opts, int d) { int i = 0; float bd = 1e9; for (size_t k = 0; k < opts.size(); k++) if (fabsf(opts[k] - v) < bd) { bd = fabsf(opts[k] - v); i = (int)k; } i = (i + d + (int)opts.size()) % (int)opts.size(); return opts[i]; };
  Settings& s = set;
  if (settingsTab == 0) {
    rows.push_back({"Language", i18nName(s.lang), [&](int d) { s.lang = (s.lang + d + LANG_COUNT) % LANG_COUNT; i18nSet(s.lang); save(); }, "Changes the interface and game text."});
    rows.push_back({tr("Time per stage"), stageLabel(s.stageMin), [&](int d) { int i = 0; for (int k = 0; k < 7; k++) if (kStageOpts[k] == s.stageMin) i = k; s.stageMin = kStageOpts[(i + d + 7) % 7]; }, tr("Default 15 min (the original uses 10).")});
    rows.push_back({tr("Scale stages 2/3"), onoff(s.scaleStages), [&](int) { s.scaleStages = !s.scaleStages; }, tr("Later stages use 90%/80% of the time.")});
    rows.push_back({tr("Final Swarm"), s.swarmMode ? tr("AFTER THE BOSS") : tr("TIMER END"), [&](int) { s.swarmMode ^= 1; }, ""});
    rows.push_back({tr("Animation FPS"), s.animFps >= 24 ? tr("SMOOTH (24)") : fmt("%d (stepped)", s.animFps), [&](int d) { s.animFps = (int)cyc((float)s.animFps, {6, 8, 12, 24}, d); }, tr("12 = the original's 'on twos' style.")});
    rows.push_back({tr("Damage numbers"), onoff(s.dmgNumbers), [&](int) { s.dmgNumbers = !s.dmgNumbers; }, ""});
    rows.push_back({tr("DPS meter on the HUD"), onoff(s.dpsMeter), [&](int) { s.dpsMeter = !s.dpsMeter; }, tr("Like the StatHUD mod.")});
    rows.push_back({tr("Chest/shrine counter"), onoff(s.counters), [&](int) { s.counters = !s.counters; }, tr("Like the SpawnTracker mod.")});
    rows.push_back({tr("Open chests without animation"), onoff(s.instantChests), [&](int) { s.instantChests = !s.instantChests; }, tr("Like the InstantChests mod.")});
    rows.push_back({tr("Auto bunny hop (hold jump)"), onoff(s.autoBhop), [&](int) { s.autoBhop = !s.autoBhop; }, tr("Like the BHop mod.")});
    rows.push_back({tr("Screen shake"), fmt("%d%%", (int)(s.shake * 100)), [&](int d) { s.shake = cyc(s.shake, {0, 0.5f, 1}, d); }, ""});
  } else if (settingsTab == 1) {
    rows.push_back({tr("Auto quality"), onoff(s.autoQuality), [&](int) { s.autoQuality = !s.autoQuality; }, tr("Lowers the render scale if FPS drops.")});
    rows.push_back({tr("Render scale"), fmt("%d%%", (int)roundf(s.renderScale * 100)), [&](int d) { s.renderScale = cyc(s.renderScale, {0.4f, 0.5f, 0.6f, 0.75f, 0.9f, 1.0f}, d); }, tr("Lower = more FPS and more pixelated.")});
    rows.push_back({tr("Max enemies"), fmt("%d", s.maxEnemies), [&](int d) { s.maxEnemies = (int)cyc((float)s.maxEnemies, {150, 220, 300, 400, 550}, d); }, tr("550 = the PC value.")});
    rows.push_back({"Vertex snapping (PS1)", onoff(s.snap), [&](int) { s.snap = !s.snap; }, ""});
    rows.push_back({tr("Effects opacity"), fmt("%d%%", (int)roundf(s.fxAlpha * 100)), [&](int d) { s.fxAlpha = cyc(s.fxAlpha, {0.25f, 0.5f, 0.75f, 1.f}, d); }, tr("Like the patch 1.0.17 slider.")});
    rows.push_back({tr("Particles"), s.particles == 0 ? tr("LOW") : s.particles == 1 ? tr("MEDIUM") : tr("HIGH"), [&](int d) { s.particles = (s.particles + d + 3) % 3; }, ""});
    rows.push_back({tr("Shadows"), onoff(s.shadows), [&](int) { s.shadows = !s.shadows; }, ""});
    rows.push_back({tr("View distance"), fmt("%dm", (int)s.viewDist), [&](int d) { s.viewDist = cyc(s.viewDist, {70, 90, 120, 160}, d); }, ""});
    rows.push_back({tr("FPS limit"), fmt("%d", s.fpsCap), [&](int d) { s.fpsCap = (int)cyc((float)s.fpsCap, {30, 60, 90, 120}, d); }, ""});
    rows.push_back({tr("Vignette"), onoff(s.vignette), [&](int) { s.vignette = !s.vignette; }, ""});
    rows.push_back({tr("Show FPS"), onoff(s.showFps), [&](int) { s.showFps = !s.showFps; }, ""});
  } else if (settingsTab == 2) {
    rows.push_back({tr("Camera sensitivity"), fmt("%.1fx", s.camSens), [&](int d) { s.camSens = cyc(s.camSens, {0.5f, 0.75f, 1.f, 1.5f, 2.f}, d); }, ""});
    rows.push_back({tr("Auto camera"), onoff(s.autoCam), [&](int) { s.autoCam = !s.autoCam; }, tr("Follows the movement direction.")});
    if (pm) rows.push_back({tr("Invert vertical camera"), onoff(s.invertY), [&](int) { s.invertY = !s.invertY; }, tr("Right stick up looks down.")});
    rows.push_back({tr("Camera distance"), fmt("%.1f", s.camDist), [&](int d) { s.camDist = cyc(s.camDist, {5.5f, 6.5f, 7.5f, 9.f, 11.f}, d); }, ""});
    rows.push_back({tr("Field of view"), fmt("%d°", (int)s.fov), [&](int d) { s.fov = cyc(s.fov, {55, 62, 70, 80}, d); }, tr("Helps with motion sickness (stepped animation).")});
    if (!plat.handheld) {
      rows.push_back({tr("Joystick size"), fmt("%d%%", (int)(s.joySize * 100)), [&](int d) { s.joySize = cyc(s.joySize, {0.8f, 1.f, 1.2f, 1.4f}, d); }, ""});
      rows.push_back({tr("Controls opacity"), fmt("%d%%", (int)(s.ctrlAlpha * 100)), [&](int d) { s.ctrlAlpha = cyc(s.ctrlAlpha, {0.4f, 0.6f, 0.85f, 1.f}, d); }, ""});
      rows.push_back({tr("Left-handed mode"), onoff(s.leftHanded), [&](int) { s.leftHanded = !s.leftHanded; }, tr("Swaps joystick and buttons.")});
      rows.push_back({tr("Interface scale"), fmt("%d%%", (int)(s.uiScale * 100)), [&](int d) { s.uiScale = cyc(s.uiScale, {0.8f, 0.9f, 1.f, 1.15f, 1.3f}, d); }, ""});
      rows.push_back({tr("Vibration"), onoff(s.vibration), [&](int) { s.vibration = !s.vibration; }, ""});
    }
  } else {
    rows.push_back({tr("Music"), fmt("%d%%", (int)roundf(s.music * 100)), [&](int d) { s.music = cyc(s.music, {0, 0.25f, 0.4f, 0.55f, 0.7f, 0.85f, 1.f}, d); }, ""});
    rows.push_back({tr("Sound effects"), fmt("%d%%", (int)roundf(s.sfx * 100)), [&](int d) { s.sfx = cyc(s.sfx, {0, 0.25f, 0.5f, 0.65f, 0.8f, 1.f}, d); }, ""});
  }
  // gamepad: compact rows (the whole row is one focusable, left/right changes it) and the hint of the
  // focused row goes to a footer; touch: taller rows with arrow buttons and inline hints
  float top = 52, bottom = pm ? u.H - 52 : u.H - 32, rh = pm ? 19 : 24;
  u.group = 1;
  scrollY[0] += u.dragY(L, top, Wd, bottom - top) + u.focusScroll(1, top, bottom);
  float maxScroll = std::max(0.f, rows.size() * rh - (bottom - top));
  scrollY[0] = clampf(scrollY[0], -maxScroll, 0);
  float vw = narrow() ? 104 : 128;
  std::string footer;
  for (size_t i = 0; i < rows.size(); i++) {
    float y = top + i * rh + scrollY[0];
    OptRow& r = rows[i];
    bool hidden = y < top - 2 || y + rh > bottom + 2;
    if (pm) {
      if (hidden) { u.focusable(L, y, Wd, rh - 3, FOCUS_ADJUST); continue; }
      u.nextFlags = FOCUS_ADJUST;
      int d = u.clicked(L, y, Wd, rh - 3) ? 1 : 0;
      if (int a = u.adjust(L, y, Wd, rh - 3)) d = a;
      bool f = u.isFocus(L, y, Wd, rh - 3);
      if (d) { r.change(d); audioPlay(SFX_CLICK); }
      if (f) footer = r.hint;
      u.panel(L, y, Wd, rh - 3, f ? Col(60, 44, 96, 235) : Col(20, 14, 30, 220));
      u.text(L + 6, y + 3, u.fit(r.label, Wd - vw - 12), 1, WHITE);
      float vx = R - vw;
      u.text(vx, y + 3, "<", 1, f ? WHITE : DIM); u.text(R - 5, y + 3, ">", 1, f ? WHITE : DIM, AL_RIGHT);
      u.text(vx + (vw - 5) * 0.5f, y + 3, r.value, 1, GOLD, AL_CENTER);
      continue;
    }
    if (hidden) continue;
    u.panel(L, y, Wd, rh - 3, Col(20, 14, 30, 220));
    u.text(L + 6, y + 2, r.label, 1, WHITE);
    if (!r.hint.empty()) u.text(L + 6, y + 10.5f, u.fit(r.hint, Wd - 142), 1, DIM);
    float vx = R - 128;
    if (u.button(vx, y + 2, 18, rh - 7, "<")) r.change(-1);
    u.text(vx + 64, y + 6, r.value, 1, GOLD, AL_CENTER);
    if (u.button(R - 22, y + 2, 18, rh - 7, ">")) r.change(1);
  }
  u.group = 0;
  if (maxScroll > 0) { // scrollbar
    float track = bottom - top - 4, th = std::max(14.f, track * (bottom - top) / (rows.size() * rh));
    float ty = top + (-scrollY[0] / maxScroll) * (track - th);
    u.rect(R + 3, top, 3, track, Col(255, 255, 255, 40));
    u.rect(R + 3, ty, 3, th, Col(255, 214, 80, 220));
    if (!pm && scrollY[0] > -maxScroll + 1) u.text(R - 64, bottom - 1, tr("drag for more"), 1, DIM, AL_CENTER);
  }
  if (pm) u.textBox(L + 2, bottom + 1, Wd - 4, footer.empty() ? std::string(tr("L1/R1: tabs   B: back")) : footer, 1, footer.empty() ? DIM : GREY, AL_CENTER, 10);
  if (u.button(u.W * 0.5f - 60, u.H - 27, 120, 22, "OK", Col(60, 150, 70, 240)) || padStart) { save(); scr = settingsBack; }
}

// ================================================================== SHOP (meta, silver)
void App::drawShop() {
  UI& u = ui;
  bgDim(0.6f);
  float L = 8 + u.safeL, R = u.W - 8 - u.safeR, Wd = R - L;
  u.text(u.W * 0.5f, 6, tr("SILVER SHOP"), 2, WHITE, AL_CENTER);
  u.icon(ICON_UI0 + UI_SILVER, u.W * 0.5f - 30, 26, 12); u.text(u.W * 0.5f - 14, 28, fmtNum(meta.silver), 1, SILV);
  struct Up { const char* name; const char* desc; int* lvl; int max; int icon; };
  Up ups[5] = {{tr("Reroll +1"), tr("Reroll level-up offers"), &meta.upReroll, 4, ICON_UI0 + UI_REROLL}, {tr("Skip +1"), tr("Skip a level-up"), &meta.upSkip, 4, ICON_UI0 + UI_SKIP},
               {tr("Banish +1"), tr("Remove an option from the run"), &meta.upBanish, 4, ICON_UI0 + UI_BANISH}, {tr("HP +10"), tr("Starting max health"), &meta.upHp, 5, ICON_UI0 + UI_HEART}, {tr("Luck +3%"), tr("Better rarity"), &meta.upLuck, 5, ICON_TOME0 + T_LUCK}};
  float y = 44;
  for (int i = 0; i < 5; i++) {
    Up& up = ups[i]; int cost = 25 * (*up.lvl + 1) * (*up.lvl + 1);
    u.panel(L, y, Wd, 28, Col(20, 14, 30, 220));
    u.icon(up.icon, L + 5, y + 4, 20);
    u.text(L + 30, y + 4, fmt("%s  (%d/%d)", up.name, *up.lvl, up.max), 1, WHITE);
    u.text(L + 30, y + 15, up.desc, 1, DIM);
    bool can = *up.lvl < up.max && meta.silver >= cost;
    if (u.button(R - 90, y + 4, 86, 20, *up.lvl >= up.max ? tr("MAX") : fmt(tr("%d silver"), cost), can ? Col(60, 120, 160, 240) : Col(50, 50, 55, 200), 1, can)) { meta.silver -= cost; (*up.lvl)++; save(); audioPlay(SFX_COIN); }
    y += 31;
  }
  if (u.button(u.W * 0.5f - 60, u.H - 27, 120, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_TITLE;
}

// ================================================================== ABOUT
void App::drawAbout() {
  UI& u = ui;
  bgDim(0.7f);
  float L = 12 + u.safeL, Wd = u.W - 24 - u.safeL - u.safeR;
  u.text(u.W * 0.5f, 6, tr("ABOUT"), 2, WHITE, AL_CENTER);
  std::string t = plat.handheld ?
    std::string(tr("OMNI BONK is a fan port inspired by Megabonk (vedinad, 2025). Models, textures, icons, sounds and music are original and generated by code in the same style: ")) +
    tr("low-poly, pixel art, PS1 vertex snapping and 'stepped' animation at 12 FPS.\n\n") +
    tr("Left stick: move  •  right: camera\nA or R1: jump  •  B or L1: slide\nX or R2: interact  •  Y or L2: map\nStart: pause  •  Select+Start: quit\n\n") +
    tr("Attacks are automatic. Time per stage is adjustable (default 15 min).")
    :
    std::string(tr("OMNI BONK MOBILE is a fan port/prototype for Android inspired by Megabonk (vedinad, 2025). ")) +
    tr("All 3D models, textures, icons, sounds and music are original and procedurally generated in the game's style: ") +
    tr("low-poly, pixelated textures, PS1-style vertex snapping and 'stepped' animation at 12 FPS (mesh flip-book).\n\n") +
    tr("QoL extras: adjustable time per stage (default 15 min), optional Final Swarm, DPS meter, chest and shrine counter, ") +
    tr("full map, effects opacity, chests without animation, auto bunny hop and optional smooth animation.\n\n") +
    tr("Controls: joystick on the left, drag on the right to rotate the camera, big button = JUMP, small button = SLIDE, ") +
    tr("a hand appears near chests and shrines. Attacks are automatic.");
  u.panel(L - 4, 26, Wd + 8, u.H - 60);
  u.textBox(L + 2, 32, Wd - 4, t, 1, WHITE, AL_LEFT, 11);
  if (u.button(8 + u.safeL, u.H - 27, 110, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_TITLE;
  if (u.button(u.W - 8 - u.safeR - 110, u.H - 27, 110, 22, tr("CREDITS"), Col(150, 110, 40, 235))) scr = SCR_CREDITS;
}

// ================================================================== CREDITS
void App::drawCredits() {
  UI& u = ui;
  bgDim(0.75f);
  float L = 12 + u.safeL, Wd = u.W - 24 - u.safeL - u.safeR;
  u.text(u.W * 0.5f, 6, tr("CREDITS"), 2, WHITE, AL_CENTER);
  u.panel(L - 4, 26, Wd + 8, u.H - 60);
  float y = 40;
  const char* heads[3] = {tr("Developer"), tr("Tribute"), tr("Technology")};
  const char* lines[3] = {"BlackCocada Studios\n(Solo Indie Dev)", tr("Non-profit fan port in honor of the success of Megabonk."), tr("Developed with the help of Claude Opus 5.5")};
  for (int i = 0; i < 3; i++) {
    u.text(u.W * 0.5f, y, heads[i], 1.5f, GOLD, AL_CENTER); y += 18;
    y += u.textBox(L + 2, y, Wd - 4, lines[i], 1, WHITE, AL_CENTER, 11) + 14;
  }
  if (u.button(u.W * 0.5f - 60, u.H - 27, 120, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_ABOUT;
}

// ================================================================== CATALOG
void App::drawCatalog() {
  UI& u = ui;
  bgDim(0.7f);
  float L = 6 + u.safeL, R = u.W - 6 - u.safeR, Wd = R - L;
  const char* tabs[5] = {tr("WEAPONS"), tr("TOMES"), tr("ITEMS"), tr("ENEMIES"), tr("OTHERS")};
  float tw = (Wd - 20) / 5;
  if (padTab) { catTab = (catTab + padTab + 5) % 5; scrollY[1] = 0; audioPlay(SFX_CLICK); }
  for (int t = 0; t < 5; t++) if (u.button(L + t * (tw + 5), 4, tw, 18, tabs[t], t == catTab ? Col(150, 110, 40, 240) : Col(50, 44, 70, 235))) { catTab = t; scrollY[1] = 0; scrollY[2] = 0; }
  float listW = Wd * 0.56f, top = 26, bottom = u.H - 30;
  static int sel[5] = {0, 0, 0, 0, 0};
  int count = catTab == 0 ? W_COUNT : catTab == 1 ? T_COUNT : catTab == 2 ? IT_COUNT : catTab == 3 ? E_COUNT : I_COUNT + PU_COUNT;
  u.group = 2;
  scrollY[1] += u.dragY(L, top, listW, bottom - top) + u.focusScroll(2, top, bottom);
  if (catTab <= 2 || catTab == 4) {
    float cell = 26; int cols = std::max(1, (int)(listW / cell));
    int rows = (count + cols - 1) / cols;
    scrollY[1] = clampf(scrollY[1], -std::max(0.f, rows * cell - (bottom - top)), 0);
    for (int i = 0; i < count; i++) {
      float x = L + (i % cols) * cell, y = top + (i / cols) * cell + scrollY[1];
      if (y < top - 4 || y + cell > bottom + 4) { u.focusable(x, y, cell - 2, cell - 2); continue; }
      if (u.isFocus(x, y, cell - 2, cell - 2)) sel[catTab] = i;
      int icon = catTab == 0 ? ICON_WEAPON0 + i : catTab == 1 ? ICON_TOME0 + i : catTab == 2 ? ICON_ITEM0 + i : (i < I_COUNT ? ICON_INTER0 + i : ICON_PU0 + i - I_COUNT);
      Col bc = catTab == 2 ? RC(gItems[i].rarity) : Col(255, 255, 255, 80);
      u.panel(x, y, cell - 2, cell - 2, sel[catTab] == i ? Col(80, 60, 120, 240) : Col(20, 14, 30, 220), bc);
      u.icon(icon, x + 2, y + 2, cell - 6);
      if (u.clicked(x, y, cell - 2, cell - 2)) { sel[catTab] = i; audioPlay(SFX_CLICK); }
    }
  } else {
    float rh = 13;
    scrollY[1] = clampf(scrollY[1], -std::max(0.f, count * rh - (bottom - top)), 0);
    for (int i = 0; i < count; i++) {
      float y = top + i * rh + scrollY[1];
      if (y < top - 2 || y + rh > bottom + 2) { u.focusable(L, y - 1, listW, rh); continue; }
      if (u.isFocus(L, y - 1, listW, rh)) sel[3] = i;
      const EnemyDef& e = gEnemies[i];
      Col c = e.rank == RK_BOSS ? RED : e.rank == RK_MINIBOSS ? GOLD : WHITE;
      if (sel[3] == i) u.rect(L, y - 1, listW, rh, Col(80, 60, 120, 200));
      u.text(L + 2, y + 1, e.name, 1, c);
      if (u.clicked(L, y - 1, listW, rh)) sel[3] = i;
    }
  }
  u.group = 0;
  // detail panel
  float dx = L + listW + 6, dw = R - dx;
  u.panel(dx, top, dw, bottom - top);
  int s = sel[catTab];
  float y = top + 6;
  auto title = [&](const std::string& t, Col c) { u.text(dx + 6, y, t, 1, c); y += 13; };
  auto body = [&](const std::string& t, Col c = WHITE) { y += u.textBox(dx + 6, y, dw - 12, t, 1, c, AL_LEFT, 11) + 3; };
  if (catTab == 0) {
    const WeaponDef& w = gWeapons[s]; u.icon(ICON_WEAPON0 + s, dx + dw - 30, top + 4, 24); title(w.name, GOLD); body(w.desc);
    body(fmt(tr("Damage %.1f  •  Proj. %d  •  Cooldown %.2fs"), w.dmg, w.proj, w.cd), CYAN);
    std::string pool = tr("Upgrades: "); for (size_t i = 0; i < w.pool.size(); i++) { auto& g = w.pool[i]; pool += (i ? ", " : "") + std::string(g.stat == U_DMG ? fmt(tr("+%.1f Damage"), g.v) : g.stat == U_PROJ ? tr("+1 Proj.") : g.stat == U_CRIT ? fmt(tr("+%.0f%% Crit."), g.v) : g.stat == U_ATKSPD ? fmt(tr("+%.0f%% Spd."), g.v) : g.stat == U_BOUNCE ? tr("+1 Ricochet") : fmt("+%.0f%% %s", g.v * 100, tr(kUpStatName[g.stat]))); }
    body(pool, GREY);
  } else if (catTab == 1) {
    const TomeDef& t = gTomes[s]; u.icon(ICON_TOME0 + s, dx + dw - 30, top + 4, 24); title(t.name, GOLD); body(t.desc);
    body(tr("Max level 99. Rarity multiplies the value: Uncommon 1.2x • Rare 1.4x • Epic 1.6x • Legendary 2x."), GREY);
  } else if (catTab == 2) {
    const ItemDef& it = gItems[s]; u.icon(ICON_ITEM0 + s, dx + dw - 30, top + 4, 24); title(it.name, RC(it.rarity)); u.text(dx + 6, y, kRarityName[it.rarity], 1, RC(it.rarity)); y += 13; body(it.desc);
    if (it.maxStack) body(fmt(tr("Max stacks: %d"), it.maxStack), GREY);
    if (!it.inPool) body(tr("Special item: does not appear in chests."), GREY);
  } else if (catTab == 3) {
    const EnemyDef& e = gEnemies[s];
    title(e.name, e.rank == RK_BOSS ? RED : e.rank == RK_MINIBOSS ? GOLD : WHITE);
    std::string maps; if (e.maps & 1) maps += "Forest "; if (e.maps & 2) maps += "Desert "; if (e.maps & 4) maps += "Graveyard";
    body(std::string(e.rank == RK_BOSS ? tr("Boss") : e.rank == RK_MINIBOSS ? tr("Miniboss") : e.flying ? tr("Flying") : tr("Ground")) + "  •  " + maps, GREY);
    body(fmt("HP base %.0f\nDano base %.0f\nVelocidade %.1f\nXP %.0f", e.hp, e.dmg, e.spd / 0.72f, e.xp), CYAN);
    body(tr("Scales with stage time, tier and the Difficulty stat."), DIM);
  } else {
    if (s < I_COUNT) { const InterDef& d = gInters[s]; u.icon(ICON_INTER0 + s, dx + dw - 30, top + 4, 24); title(d.name, GOLD); body(d.desc); }
    else { const PowerupDef& p = gPowerups[s - I_COUNT]; u.icon(ICON_PU0 + s - I_COUNT, dx + dw - 30, top + 4, 24); title(std::string(tr("Powerup: ")) + tr(p.name), GOLD); body(p.desc); }
  }
  u.text(L, u.H - 22, u.fit(u.padMode ? fmt(tr("L1/R1: tabs  •  %d items"), IT_COUNT) : fmt(tr("%d weapons • %d tomes • %d items • %d enemies • %d interactables"), W_COUNT, T_COUNT, IT_COUNT, E_COUNT, I_COUNT), R - 96 - L), 1, DIM);
  if (u.button(R - 90, u.H - 27, 90, 22, tr("BACK"), Col(70, 70, 120, 235))) scr = SCR_TITLE;
}

// ================================================================== GAME: controls
void App::gameControls() {
  UI& u = ui;
  Game& g = game;
  g.in.camDX = g.in.camDY = 0; g.in.camManual = false; g.in.move = v2(); g.in.jumpHeld = false; g.in.slide = false;
  if (g.state != GS_PLAY) { joyTouch = camTouch = jumpTouch = slideTouch = -1; for (auto& t : u.touches) if (t.owner > 0) t.owner = -1; return; }
  bool lh = set.leftHanded;
  float jr = 34 * set.joySize;
  float bR = 30, sR = 17;
  float bx = lh ? 16 + bR + u.safeL : u.W - 16 - bR - u.safeR, byy = u.H - 16 - bR - u.safeB;
  float sx = lh ? bx + bR + sR + 8 : bx - bR - sR - 8, sy = u.H - 14 - sR - u.safeB;
  float ix = bx, iy = byy - bR - 26;
  int ni = g.nearestInter(2.8f);
  float mmR = 34, mmx = u.W - 6 - mmR - u.safeR, mmy = 40 + mmR;
  float pbx = mmx - mmR - 26, pby = 30;
  for (auto& t : u.touches) {
    if (t.began && t.owner == 0) {
      if (u.inside(t.pos, pbx - 4, pby - 4, 28, 28)) { t.owner = 9; continue; }
      if (len(t.pos - v2(mmx, mmy)) < mmR) { t.owner = 10; continue; }
      if (len(t.pos - v2(bx, byy)) < bR + 8) { t.owner = 3; jumpTouch = t.id; g.in.jump = true; continue; }
      if (len(t.pos - v2(sx, sy)) < sR + 7) { t.owner = 4; slideTouch = t.id; continue; }
      if (ni >= 0 && len(t.pos - v2(ix, iy)) < 22) { t.owner = 5; continue; }
      bool leftSide = lh ? t.pos.x > u.W * 0.55f : t.pos.x < u.W * 0.45f;
      if (leftSide && joyTouch < 0) { t.owner = 1; joyTouch = t.id; joyBase = t.pos; continue; }
      if (camTouch < 0) { t.owner = 2; camTouch = t.id; continue; }
    }
    if (t.owner == 1) {
      joyCur = t.pos; v2 d = (t.pos - joyBase) / jr; float l = len(d);
      if (l > 1) { joyBase = joyBase + (t.pos - joyBase) * ((l - 1) / l) * 0.6f; d = d / l; }
      g.in.move = v2(d.x, -d.y);
      if (!t.down) joyTouch = -1;
    } else if (t.owner == 2) {
      v2 d = t.pos - t.prev;
      g.in.camDX += -d.x * 0.012f * set.camSens * (lh ? 1 : 1); g.in.camDY += d.y * 0.008f * set.camSens; g.in.camManual = len(d) > 0.3f || true;
      if (!t.down) camTouch = -1;
    } else if (t.owner == 3) { g.in.jumpHeld = t.down; if (!t.down) jumpTouch = -1; }
    else if (t.owner == 4) { g.in.slide = t.down; if (!t.down) slideTouch = -1; }
    else if (t.owner == 5) { if (t.released && ni >= 0) { g.interact(ni); audioPlay(SFX_CLICK); } }
    else if (t.owner == 9) { if (t.released) { g.prevState = g.state; g.state = GS_PAUSE; audioPlay(SFX_CLICK); } }
    else if (t.owner == 10) { if (t.released) { showMap = true; g.state = GS_PAUSE; audioPlay(SFX_CLICK); } }
  }
  if (u.padMode) {
    unsigned b = pad.buttons;
    v2 mv(pad.lx, -pad.ly);
    if (b & PB_LEFT) mv.x -= 1;
    if (b & PB_RIGHT) mv.x += 1;
    if (b & PB_UP) mv.y += 1;
    if (b & PB_DOWN) mv.y -= 1;
    float l = len(mv);
    if (l > 0.2f) g.in.move = l > 1 ? mv / l : mv;
    v2 cam(pad.rx, pad.ry); float cl = len(cam);
    if (cl > 0.18f) {
      float k = std::min(1.f, (cl - 0.18f) / 0.8f); k = k * (0.35f + 0.65f * k); // finer control near the centre
      cam = cam / cl * k;
      g.in.camDX += -cam.x * 3.1f * set.camSens * frameDt;
      g.in.camDY += cam.y * 1.7f * set.camSens * frameDt * (set.invertY ? -1.f : 1.f);
      g.in.camManual = true;
    }
    if (padPressed & (PB_A | PB_R1)) g.in.jump = true;
    if (b & (PB_A | PB_R1)) g.in.jumpHeld = true;
    if (b & (PB_B | PB_L1)) g.in.slide = true;
    if ((padPressed & (PB_X | PB_R2)) && ni >= 0) { g.interact(ni); audioPlay(SFX_CLICK); }
    else if (padPressed & (PB_Y | PB_L2)) { showMap = true; g.state = GS_PAUSE; audioPlay(SFX_CLICK); }
    else if (padPressed & PB_START) { g.prevState = g.state; g.state = GS_PAUSE; audioPlay(SFX_CLICK); }
  }
  if (!set.autoCam) g.camManualT = 99;
}

// ================================================================== GAME: HUD
static void bar(UI& u, float x, float y, float w, float h, float k, Col fill, Col bg = Col(20, 10, 20, 220)) {
  u.rect(x, y, w, h, Col(0, 0, 0, 200)); u.rect(x + 1, y + 1, w - 2, h - 2, bg);
  u.rect(x + 1, y + 1, (w - 2) * sat(k), h - 2, fill); u.rect(x + 1, y + 1, (w - 2) * sat(k), 1, mulc(fill, 1.3f));
}

void App::drawMinimap(float cx, float cy, float r) {
  UI& u = ui; Game& g = game;
  u.sprite(SP_SOFT, cx - r - 2, cy - r - 2, 2 * r + 4, 2 * r + 4, Col(0, 0, 0, 160));
  // terrain dots
  float scale = r / 46.f; // world meters -> virtual px
  float cyaw = g.camYaw;
  float cs = cosf(cyaw), sn = sinf(cyaw);
  auto proj = [&](v3 w, v2& out) {
    float dx = w.x - g.P.pos.x, dz = w.z - g.P.pos.z;
    float rx = -(dx * cs - dz * sn), ry = -(dx * sn + dz * cs);
    out = v2(cx + rx * scale, cy + ry * scale);
    return rx * rx + ry * ry < (46 * 46);
  };
  const Palette& pal = g.world.pal;
  int N = 21; float step = 92.f / N;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      float lx = -46 + (i + 0.5f) * step, ly = -46 + (j + 0.5f) * step;
      if (lx * lx + ly * ly > 46 * 46) continue;
      float wx = g.P.pos.x - (lx * cs + ly * sn), wz = g.P.pos.z - (-lx * sn + ly * cs);
      float h = g.world.T.height(wx, wz) - g.P.pos.y;
      Col c = mixc(hexc(pal.ground1), hexc(pal.ground2), sat(0.5f + h * 0.05f));
      if (!g.world.inBounds(wx, wz, 20)) c = Col(30, 25, 35);
      u.rect(cx + lx * scale - step * scale * 0.5f, cy + ly * scale - step * scale * 0.5f, step * scale + 0.6f, step * scale + 0.6f, Col(c.r, c.g, c.b, 200));
    }
  u.sprite(SP_RING, cx - r * 1.25f, cy - r * 1.25f, r * 2.5f, r * 2.5f, Col(255, 255, 255, 120));
  // markers
  for (auto& it : g.inters) {
    v2 p; if (!proj(it.pos, p)) continue;
    if (it.used && it.type != I_BOSS_PORTAL) continue;
    switch (it.type) {
      case I_CHEST: u.sprite(SP_JOY_KNOB, p.x - 2, p.y - 2, 4, 4, Col(255, 160, 40)); break;
      case I_CHEST_GOLD: u.sprite(SP_JOY_KNOB, p.x - 3, p.y - 3, 6, 6, GOLD); break;
      case I_CHARGE_SHRINE: u.rect(p.x - 2, p.y - 2, 4, 4, it.rarity == R_LEGENDARY ? GOLD : GREEN); break;
      case I_POT: case I_POT_BIG: case I_PUMPKIN: case I_POT_SILVER: case I_TUMBLEWEED: u.rect(p.x - 0.5f, p.y - 0.5f, 1.5f, 1.5f, Col(255, 255, 255, 200)); break;
      case I_BOSS_PORTAL: u.rect(p.x - 3, p.y - 3, 6, 6, g.portalOpen ? CYAN : RED); break;
      case I_SHADY: case I_MOAI: case I_MICROWAVE: u.rect(p.x - 2, p.y - 2, 4, 4, Col(200, 120, 255)); break;
      case I_CRYPT_KEY: u.rect(p.x - 2, p.y - 2, 4, 4, Col(127, 255, 159)); break;
      case I_ALTAR: u.rect(p.x - 2, p.y - 2, 4, 4, RED); break;
      default: u.rect(p.x - 1.5f, p.y - 1.5f, 3, 3, Col(200, 200, 200)); break;
    }
  }
  for (auto& e : g.enemies) { if (!e.alive || (e.rank == RK_NORMAL && !e.elite)) continue; v2 p; if (!proj(e.pos, p)) continue; u.rect(p.x - 2, p.y - 2, 4, 4, e.rank == RK_BOSS ? Col(255, 40, 40) : Col(255, 120, 60)); }
  // player arrow (always up = camera forward)
  float py = g.P.yaw - cyaw;
  v2 tip(cx - sinf(py) * 5, cy - cosf(py) * 5), l(cx - sinf(py + 2.5f) * 4, cy - cosf(py + 2.5f) * 4), rr(cx - sinf(py - 2.5f) * 4, cy - cosf(py - 2.5f) * 4);
  u.rect(cx - 2, cy - 2, 4, 4, Col(80, 160, 255)); u.rect(tip.x - 1, tip.y - 1, 2, 2, Col(120, 200, 255)); (void)l; (void)rr;
  // compass letters
  const char* L4[4] = {"N", "E", "S", "W"};
  for (int k = 0; k < 4; k++) {
    float a = k * PI / 2; float wx = -sinf(a), wz = cosf(a); // N = +z, E = -x (screen right when facing north)
    float rx = -(wx * cs - wz * sn), ry = -(wx * sn + wz * cs);
    u.text(cx + rx * (r + 5), cy + ry * (r + 5) - 4, L4[k], 1, k == 0 ? Col(255, 120, 120) : WHITE, AL_CENTER);
  }
}

void App::drawFullMap() {
  UI& u = ui; Game& g = game;
  bgDim(0.8f);
  float sz = std::min(u.W - 40, u.H - 50), x0 = u.W * 0.5f - sz * 0.5f, y0 = 20;
  float half = g.world.playableHalf() + 8;
  int N = 48; float cell = sz / N;
  const Palette& pal = g.world.pal;
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      float wx = -half + (i + 0.5f) / N * 2 * half, wz = half - (j + 0.5f) / N * 2 * half;
      float h = g.world.T.height(wx, wz);
      Col c = mixc(hexc(pal.ground1), hexc(pal.ground2), sat(0.4f + h * 0.04f));
      c = mixc(c, hexc(pal.rock), sat((1 - g.world.T.normal(wx, wz).y - 0.2f) * 3));
      u.rect(x0 + i * cell, y0 + j * cell, cell + 0.5f, cell + 0.5f, c);
    }
  auto P2 = [&](v3 w) { return v2(x0 + (w.x + half) / (2 * half) * sz, y0 + (half - w.z) / (2 * half) * sz); };
  for (auto& it : g.inters) {
    if (it.used && it.type != I_BOSS_PORTAL && it.type != I_CHEST) continue;
    v2 p = P2(it.pos); Col c = WHITE; float s = 3;
    switch (it.type) { case I_CHEST: c = it.used ? Col(110, 80, 50) : Col(255, 160, 40); break; case I_CHEST_GOLD: c = GOLD; s = 4; break; case I_CHARGE_SHRINE: c = GREEN; break; case I_BOSS_PORTAL: c = g.portalOpen ? CYAN : RED; s = 6; break;
      case I_SHADY: case I_MOAI: case I_MICROWAVE: c = Col(200, 120, 255); break; case I_CRYPT_KEY: c = Col(127, 255, 159); s = 4; break; case I_GREED_SHRINE: c = GOLD; break; case I_CURSED_SHRINE: c = Col(170, 60, 255); break; case I_CHALLENGE_SHRINE: c = Col(220, 140, 255); break; default: s = 1.5f; c = Col(230, 230, 230, 160); }
    u.rect(p.x - s / 2, p.y - s / 2, s, s, c);
  }
  v2 pp = P2(g.P.pos); u.rect(pp.x - 3, pp.y - 3, 6, 6, Col(80, 160, 255)); u.frame(pp.x - 5, pp.y - 5, 10, 10, WHITE);
  u.frame(x0 - 1, y0 - 1, sz + 2, sz + 2, Col(255, 255, 255, 120));
  float lx = x0 + sz + 8; float ly = y0;
  auto leg = [&](Col c, const char* t) { if (lx + 60 > u.W) return; u.rect(lx, ly + 2, 5, 5, c); u.text(lx + 9, ly, t, 1, GREY); ly += 12; };
  leg(Col(255, 160, 40), tr("Chest")); leg(GOLD, tr("Golden chest")); leg(GREEN, "Charge Shrine"); leg(RED, tr("Boss portal")); leg(Col(200, 120, 255), "Moai/Shady/Micro"); leg(Col(80, 160, 255), tr("You"));
  if (u.button(u.W * 0.5f - 50, u.H - 26, 100, 20, tr("CLOSE"), Col(70, 70, 120, 235)) || padStart || (u.padMode && acceptLock <= 0 && (padPressed & (PB_Y | PB_L2)))) { showMap = false; g.state = GS_PLAY; }
}

void App::drawWorldUI() {
  UI& u = ui; Game& g = game;
  // damage numbers
  for (auto& t : g.texts) {
    v2 sp; if (!gR.worldToScreen(t.pos, sp)) continue;
    float k = t.t / t.life; Col c = t.c; c.a = (u8)(255 * (1 - std::max(0.f, k - 0.6f) / 0.4f));
    float pop = t.scale * (k < 0.12f ? 1.3f : 1.f);
    u.text(sp.x / u.s, sp.y / u.s, t.s, pop, c, AL_CENTER);
  }
  // elites / minibosses: crown + bar
  for (auto& e : g.enemies) {
    if (!e.alive || (!e.elite && e.rank != RK_MINIBOSS)) continue;
    v2 sp; if (!gR.worldToScreen(e.pos + v3(0, 2.4f * e.scale + (e.rank ? 1.5f : 0.3f), 0), sp)) continue;
    float x = sp.x / u.s, y = sp.y / u.s;
    if (x < -20 || x > u.W + 20 || y < -20 || y > u.H + 20) continue;
    u.icon(ICON_UI0 + UI_CROWN, x - 5, y - 14, 10, e.challenge ? Col(210, 140, 255) : Col(255, 255, 255));
    if (e.rank == RK_MINIBOSS || e.hp < e.maxHp) bar(u, x - 14, y - 2, 28, 4, e.hp / e.maxHp, Col(255, 70, 70));
  }
  // interaction prompt: compact tag above the object, kept clear of the HUD column and the minimap
  int ni = g.nearestInter(2.8f);
  if (ni >= 0) {
    v2 sp; if (gR.worldToScreen(g.inters[ni].pos + v3(0, 2.4f, 0), sp)) {
      std::string lb = g.interLabel(ni), cs; int cost = g.interCost(ni);
      if (cost >= 0) cs = fmt("%d", cost);
      float kw = u.padMode ? 11 : 0, tw = u.textWRaw(lb, 1), cw = cost >= 0 ? 12 + u.textWRaw(cs, 1) : 0;
      float w = kw + tw + cw + 8, h = 12;
      float x = clampf(sp.x / u.s - w / 2, 2, std::max(2.f, u.W - w - 2)), y = std::max(24.f, sp.y / u.s - 8);
      auto hits = [&](float rx, float ry, float rw, float rh) { return x < rx + rw && x + w > rx && y < ry + rh && y + h > ry; };
      if (hits(0, 0, hudR, hudB)) x = hudR + 3;
      if (hits(hudTR, 0, u.W - hudTR, hudTRB)) x = hudTR - w - 3;
      if (x < 2 || hits(0, 0, hudR, hudB)) { x = clampf(x, 2, std::max(2.f, u.W - w - 2)); y = std::max(hudB, hudTRB) + 2; }
      u.panel(x, y, w, h, Col(16, 12, 24, 205), Col(255, 214, 80, 170));
      float tx = x + 4;
      if (u.padMode) { u.rect(tx - 1, y + 1, 9, 10, Col(200, 150, 40)); u.textRaw(tx + 1, y + 2, "X", 1, WHITE, AL_LEFT); tx += kw; }
      tx += u.textRaw(tx, y + 2, lb, 1, WHITE, AL_LEFT);
      if (cost >= 0) { u.icon(ICON_UI0 + UI_GOLD, tx + 3, y + 2, 8); u.textRaw(tx + 12, y + 2, cs, 1, g.P.gold >= cost ? GOLD : Col(255, 110, 90), AL_LEFT); }
    }
  }
  // charge shrine progress text
  for (auto& it : g.inters) if (it.type == I_CHARGE_SHRINE && !it.used && it.progress > 0.01f) { v2 sp; if (gR.worldToScreen(it.pos + v3(0, 4.6f, 0), sp)) u.text(sp.x / u.s, sp.y / u.s, fmt("%d%%", (int)(it.progress * 100)), 1, CYAN, AL_CENTER); }
  // off-screen boss portal indicator when time is running out
  if ((g.u() > 0.7f || g.portalOpen) && !g.finalArena) {
    for (auto& it : g.inters) if (it.type == I_BOSS_PORTAL) {
      v2 sp; bool on = gR.worldToScreen(it.pos + v3(0, 3, 0), sp);
      float x = sp.x / u.s, y = sp.y / u.s;
      if (on && x > 10 && x < u.W - 10 && y > 10 && y < u.H - 10) { u.icon(ICON_UI0 + UI_PORTAL, x - 7, y - 16, 14); }
      else {
        v3 d = it.pos - g.P.pos; float a = atan2f(d.x, d.z) - g.camYaw;
        float ex = u.W * 0.5f + sinf(a) * -1 * 0 + sinf(-a) * (u.W * 0.42f), ey = u.H * 0.5f - cosf(a) * (u.H * 0.4f);
        u.icon(ICON_UI0 + UI_PORTAL, ex - 7, ey - 7, 14, g.portalOpen ? CYAN : RED);
      }
    }
  }
}

void App::drawHUD() {
  UI& u = ui; Game& g = game; Player& P = g.P;
  float L = 6 + u.safeL, R = u.W - 6 - u.safeR;
  // XP bar across the top
  bar(u, 0, 0, u.W, 5, P.xp / P.xpNext, Col(56, 184, 255), Col(10, 10, 30, 230));
  bool nar = narrow(), pm = u.padMode;
  // center timer
  bool infinite = g.cfg.stageMinutes <= 0;
  std::string tm; Col tc = WHITE;
  if (g.finalSwarm) { tm = tr("SWARM ") + fmtTime(g.finalSwarmT); tc = fmodf(g.realTime, 0.8f) < 0.4f ? RED : Col(255, 180, 180); }
  else if (infinite) tm = fmtTime(g.stageTime);
  else { float left = g.stageDur - g.stageTime; tm = fmtTime(left); if (left < 60) tc = fmodf(g.realTime, 1.f) < 0.5f ? RED : WHITE; }
  if (g.finalArena) { tm = (nar ? tr("BOSS ") : tr("FINAL BOSS ")) + fmtTime(std::max(0.f, g.stageDur - g.stageTime)); }
  float mmR = 34, mmx = R - mmR, mmy = 40 + mmR;
  float hpMax = P.st[S_MAXHP];
  if (nar) {
    // 4:3 handheld (320 units wide): HP bar on the first row, counters below it, timer centred, level on the right
    float tscale = u.textW(tm, 2) > 96 ? 1.f : 2.f;
    u.text(u.W * 0.5f + 14, tscale > 1 ? 7 : 11, tm, tscale, tc, AL_CENTER);
    u.text(R, 8, fmt(tr("LVL %d"), P.level), 1.5f, WHITE, AL_RIGHT);
    bar(u, L, 8, 104, 11, P.hp / hpMax, Col(225, 40, 40));
    if (P.hp > hpMax) { float ok = sat((P.hp - hpMax) / hpMax); u.rect(L + 1, 9, 102 * ok, 9, Col(255, 200, 80)); }
    u.text(L + 52, 9, fmt("%d/%d", (int)ceilf(P.hp), (int)roundf(hpMax)), 1, WHITE, AL_CENTER);
    if (P.st[S_SHIELD] > 0) bar(u, L, 20, 104, 4, P.shield / P.st[S_SHIELD], Col(80, 190, 255));
    float y = 27;
    u.icon(ICON_UI0 + UI_SKULL, L, y, 10); u.text(L + 12, y + 1, fmtShort(P.kills), 1, WHITE);
    u.icon(ICON_UI0 + UI_GOLD, L + 42, y, 10); u.text(L + 54, y + 1, fmtShort(floor(P.gold)), 1, GOLD);
    u.icon(ICON_UI0 + UI_SILVER, L + 84, y, 10); u.text(L + 96, y + 1, fmtNum(P.silver), 1, SILV);
  } else {
    // top-left row
    float y = 8;
    u.icon(ICON_UI0 + UI_CLOCK, L, y, 10); u.text(L + 13, y + 1, fmtTime(g.runTime), 1, WHITE);
    u.icon(ICON_UI0 + UI_SILVER, L + 50, y, 10); u.text(L + 63, y + 1, fmtNum(P.silver), 1, SILV);
    u.icon(ICON_UI0 + UI_GOLD, L + 100, y, 10); u.text(L + 113, y + 1, fmtShort(floor(P.gold)), 1, GOLD);
    u.icon(ICON_UI0 + UI_SKULL, L + 150, y, 10); u.text(L + 163, y + 1, fmtShort(P.kills), 1, WHITE);
    u.text(u.W * 0.5f, 7, tm, 2, tc, AL_CENTER);
    u.text(R, 7, fmt(tr("LVL %d"), P.level), 2, WHITE, AL_RIGHT);
    // HP + shield
    bar(u, L, 21, 120, 11, P.hp / hpMax, Col(225, 40, 40));
    if (P.hp > hpMax) { float ok = sat((P.hp - hpMax) / hpMax); u.rect(L + 1, 22, 118 * ok, 9, Col(255, 200, 80)); }
    u.text(L + 60, 22, fmt("%d/%d", (int)ceilf(P.hp), (int)roundf(hpMax)), 1, WHITE, AL_CENTER);
    if (P.st[S_SHIELD] > 0) bar(u, L, 33, 120, 4, P.shield / P.st[S_SHIELD], Col(80, 190, 255));
  }
  drawMinimap(mmx, mmy, mmR);
  // pause button (touch)
  float pbx = mmx - mmR - 26, pby = 30;
  if (!pm) { u.panel(pbx, pby, 20, 20, Col(16, 12, 24, 200), Col(255, 255, 255, 120)); u.icon(ICON_UI0 + UI_PAUSE, pbx + 3, pby + 3, 14); }
  // weapons and tomes grids
  float gy = 40, gs = 19;
  for (int i = 0; i < 4; i++) {
    float x = L + (i % 2) * (gs + 1), yy = gy + (i / 2) * (gs + 1);
    u.panel(x, yy, gs, gs, Col(20, 16, 30, 200), Col(255, 255, 255, 50));
    if (i < (int)P.weapons.size()) { u.icon(ICON_WEAPON0 + P.weapons[i].def, x + 1, yy + 1, gs - 2); u.text(x + gs - 1, yy + gs - 9, fmt("%d", P.weapons[i].level), 1, WHITE, AL_RIGHT); }
  }
  for (int i = 0; i < 4; i++) {
    float x = L + 42 + (i % 2) * (gs + 1), yy = gy + (i / 2) * (gs + 1);
    u.panel(x, yy, gs, gs, Col(20, 16, 30, 200), Col(255, 255, 255, 50));
    if (i < P.ntomes) { u.icon(ICON_TOME0 + P.tomes[i].def, x + 1, yy + 1, gs - 2); u.text(x + gs - 1, yy + gs - 9, fmt("%d", P.tomes[i].level), 1, WHITE, AL_RIGHT); }
  }
  // quest box
  float qy = gy + 2 * (gs + 1) + 3;
  std::string quest; bool done = false;
  if (g.finalArena) quest = tr("Defeat the Ghost King");
  else if (g.cfg.map == MAP_GRAVEYARD && g.cryptKeys < 4) quest = fmt(tr("Crypt Keys %d/4"), g.cryptKeys);
  else if (!g.bossSummoned) quest = tr("Find the Boss");
  else if (!g.bossDefeated) quest = fmt(tr("Defeat the boss (%d)"), g.bossesAlive);
  else { quest = tr("Enter the portal"); done = false; }
  float qw = u.textW(quest) + 22;
  u.panel(L, qy, qw, 14, Col(0, 0, 0, 200), Col(255, 255, 255, 40));
  u.frame(L + 4, qy + 3, 8, 8, WHITE); if (done) u.rect(L + 6, qy + 5, 4, 4, GREEN);
  u.text(L + 16, qy + 2, quest, 1, WHITE);
  float cy = qy + 17, colR = std::max(L + 82, L + qw);
  if (set.counters) {
    // one row of icons: chests left (+ next chest price), charge shrines left/total, shady guys
    int chests = 0, shrines = 0, shrTot = 0, shady = 0;
    for (auto& it : g.inters) { if (it.type == I_CHEST && !it.used) chests++; if (it.type == I_CHARGE_SHRINE) { shrTot++; if (!it.used) shrines++; } if (it.type == I_SHADY) shady++; }
    float x = L;
    auto cnt = [&](int icon, const std::string& v, Col c) { u.icon(icon, x, cy - 1, 10); x += 11; x += u.textRaw(x, cy, v, 1, c, AL_LEFT) + 5; };
    cnt(ICON_INTER0 + I_CHEST, fmt("%d", chests), WHITE);
    cnt(ICON_UI0 + UI_GOLD, fmt("%d", g.chestCost()), GOLD);
    cnt(ICON_INTER0 + I_CHARGE_SHRINE, fmt("%d/%d", shrines, shrTot), WHITE);
    if (!nar) cnt(ICON_INTER0 + I_SHADY, fmt("%d", shady), WHITE);
    colR = std::max(colR, x); cy += 12;
  }
  if (set.dpsMeter && g.runTime > 1) {
    struct E { int src; double d; }; std::vector<E> v;
    for (int i = 0; i < SRC_COUNT; i++) if (g.dmgBySrc[i] > 0) v.push_back({i, g.dmgBySrc[i]});
    std::sort(v.begin(), v.end(), [](const E& a, const E& b) { return a.d > b.d; });
    for (size_t k = 0; k < v.size() && k < 5; k++) {
      int s = v[k].src; std::string nm; int ic = -1;
      if (s < (int)P.weapons.size()) { nm = gWeapons[P.weapons[s].def].name; ic = ICON_WEAPON0 + P.weapons[s].def; }
      else if (s >= SRC_ITEM && s < SRC_ITEM + IT_COUNT) { nm = gItems[s - SRC_ITEM].name; ic = ICON_ITEM0 + s - SRC_ITEM; }
      else nm = tr("Others");
      if (ic >= 0) u.icon(ic, L, cy, 9);
      colR = std::max(colR, L + 12 + u.text(L + 12, cy, fmt("%s %s/s", nm.c_str(), fmtShort(v[k].d / std::max(1.f, g.runTime)).c_str()), 1, Col(255, 230, 160))); cy += 10;
    }
  }
  hudR = colR + 2; hudB = cy; hudTR = (pm ? mmx - mmR : mmx - mmR - 26) - 2; hudTRB = mmy + mmR + 12;
  // active powerups under the timer
  float px = u.W * 0.5f - (nar ? 22 : 30), puy = nar ? 30 : 26; int npu = 0;
  auto pu = [&](int icon, float t) { if (t <= 0) return; u.icon(icon, px + npu * 22, puy, 12); u.text(px + npu * 22 + 6, puy + 14, fmt("%d", (int)ceilf(t)), 1, WHITE, AL_CENTER); npu++; };
  pu(ICON_PU0 + PU_RAGE, P.rageT); pu(ICON_PU0 + PU_SHIELD, P.shieldT); pu(ICON_PU0 + PU_GOLD, P.goldT); pu(ICON_PU0 + PU_CLOCK, g.freezeEnemiesT);
  // boss bar
  for (auto& e : g.enemies) if (e.alive && e.rank == RK_BOSS) {
    float bw = u.W * 0.42f, bx = u.W * 0.5f - bw * 0.5f, by = npu ? 56 : (nar ? 32 : 30);
    u.text(u.W * 0.5f, by, gEnemies[e.type].name, 1, e.invuln ? Col(190, 170, 255) : Col(255, 120, 120), AL_CENTER);
    bar(u, bx, by + 11, bw, 7, e.hp / e.maxHp, e.invuln ? Col(150, 130, 255) : Col(220, 30, 50));
    break;
  }
  // banners
  float byy = u.H * 0.28f;
  for (auto& b : g.banners) {
    float a = sat(std::min(b.t / 0.25f, (b.life - b.t) / 0.5f));
    Col c = b.c; c.a = (u8)(255 * a);
    float bs = 3; while (bs > 1 && u.textW(b.text, bs) > u.W - 12) bs -= 0.5f;
    if (nar) u.rect(0, byy - 3, u.W, 11 * bs + (b.sub.empty() ? 5 : 17), Col(8, 4, 16, (u8)(190 * a))); // 320-wide: the banner crosses the HUD column
    u.text(u.W * 0.5f, byy, b.text, bs, c, AL_CENTER);
    if (!b.sub.empty()) u.text(u.W * 0.5f, byy + 11 * bs + 1, u.fit(b.sub, u.W - 8), 1, Col(255, 255, 255, (u8)(220 * a)), AL_CENTER);
    byy += 11 * bs + 19;
  }
  if (pm) {
    if (g.runTime < 9 && g.stage == 0) {
      u8 a = (u8)(255 * sat((9 - g.runTime) / 1.5f));
      const char* h1 = tr("A/R1 jump   B/L1 slide"); const char* h2 = tr("X/R2 interact   Y/L2 map   START pause");
      float w = std::max(u.textW(h1), u.textW(h2)) + 12;
      u.panel(u.W * 0.5f - w / 2, u.H - 34, w, 26, Col(10, 8, 16, (u8)(a * 0.75f)), Col(255, 255, 255, (u8)(a * 0.3f)));
      u.text(u.W * 0.5f, u.H - 31, h1, 1, Col(255, 255, 255, a), AL_CENTER); u.text(u.W * 0.5f, u.H - 20, h2, 1, Col(255, 255, 255, a), AL_CENTER);
    }
    return;
  }
  // touch controls (pixel style from the concept image)
  bool lh = set.leftHanded;
  float jr = 34 * set.joySize, bR = 30, sR = 17;
  u8 ca = (u8)(255 * set.ctrlAlpha);
  v2 jb = joyTouch >= 0 ? joyBase : v2(lh ? u.W - 18 - jr - u.safeR : 18 + jr + u.safeL, u.H - 18 - jr - u.safeB);
  v2 jk = joyTouch >= 0 ? jb + v2(g.in.move.x, -g.in.move.y) * jr : jb;
  u.sprite(SP_JOY_BASE, jb.x - jr, jb.y - jr, jr * 2, jr * 2, Col(255, 255, 255, ca));
  for (int k = 0; k < 4; k++) { float a = k * PI / 2; u.sprite(SP_ARROW, jb.x + sinf(a) * jr * 0.72f - 3, jb.y - cosf(a) * jr * 0.72f - 3, 6, 6, Col(255, 255, 255, (u8)(ca * 0.8f))); }
  u.sprite(SP_JOY_KNOB, jk.x - jr * 0.38f, jk.y - jr * 0.38f, jr * 0.76f, jr * 0.76f, Col(255, 255, 255, (u8)(ca * 0.9f)));
  float bx = lh ? 16 + bR + u.safeL : u.W - 16 - bR - u.safeR, by2 = u.H - 16 - bR - u.safeB;
  bool jh = jumpTouch >= 0;
  u.sprite(SP_BTN_BIG, bx - bR, by2 - bR, bR * 2, bR * 2, Col(255, 255, 255, jh ? 255 : ca));
  u.icon(ICON_UI0 + UI_JUMP, bx - 12, by2 - 12, 24, Col(255, 255, 255, ca));
  float sx = lh ? bx + bR + sR + 8 : bx - bR - sR - 8, sy = u.H - 14 - sR - u.safeB;
  u.sprite(SP_BTN_SMALL, sx - sR, sy - sR, sR * 2, sR * 2, Col(255, 255, 255, slideTouch >= 0 ? 255 : ca));
  u.icon(ICON_UI0 + UI_SLIDE, sx - 8, sy - 8, 16, Col(255, 255, 255, ca));
  int ni = g.nearestInter(2.8f);
  if (ni >= 0) {
    float ix = bx, iy = by2 - bR - 26;
    float pulse = 1 + 0.06f * sinf(g.realTime * 8);
    u.sprite(SP_BTN_SMALL, ix - 19 * pulse, iy - 19 * pulse, 38 * pulse, 38 * pulse, Col(255, 220, 90, 255));
    u.icon(ICON_UI0 + UI_INTERACT, ix - 9, iy - 9, 18, GOLD);
  }
}

// ================================================================== OVERLAYS
void App::drawOfferCard(const Offer& o, float x, float y, float w, float h, bool sel) {
  UI& u = ui; Game& g = game;
  Col rc = RC(o.rarity);
  u.rect(x + 2, y + 3, w, h, Col(0, 0, 0, 140));
  u.panel(x, y, w, h, sel ? Col(70, 50, 100, 245) : Col(22, 16, 34, 245), rc);
  u.rect(x + 2, y + 2, w - 4, 12, mulc(rc, 0.45f));
  u.text(x + w * 0.5f, y + 3, o.kind == 4 ? tr("BONUS") : kRarityName[o.rarity], 1, rc, AL_CENTER);
  int ic = offerIcon(o);
  if (o.kind == 4) ic = ICON_UI0 + UI_ARROWUP;
  u.sprite(SP_GLOW, x + w * 0.5f - 22, y + 16, 44, 44, Col(rc.r, rc.g, rc.b, 90));
  u.icon(ic, x + w * 0.5f - 16, y + 22, 32);
  float ty = y + 58;
  ty += u.textBox(x + 4, ty, w - 8, offerTitle(o, g), 1, WHITE, AL_CENTER, 11) + 2;
  u.textBox(x + 5, ty, w - 10, offerDesc(o, g), 1, o.kind == 1 || o.kind == 4 ? GREEN : GREY, AL_CENTER, 10);
}

void App::drawLevelUp() {
  UI& u = ui; Game& g = game;
  bgDim(0.55f);
  u.text(u.W * 0.5f, u.H * 0.06f, banishMode ? tr("CHOOSE WHAT TO BANISH") : tr("LEVEL UP!"), 2, banishMode ? RED : GOLD, AL_CENTER);
  int n = (int)g.offers.size();
  float cw = std::min(140.f, (u.W - 40 - u.safeL - u.safeR) / 3), ch = std::min(150.f, u.H * 0.6f);
  float x0 = u.W * 0.5f - (n * cw + (n - 1) * 8) * 0.5f, y0 = u.H * 0.06f + 26;
  for (int i = 0; i < n; i++) {
    float x = x0 + i * (cw + 8);
    drawOfferCard(g.offers[i], x, y0, cw, ch, false);
    if (banishMode) u.frame(x - 1, y0 - 1, cw + 2, ch + 2, RED, 2);
    if (u.clicked(x, y0, cw, ch)) {
      Offer o = g.offers[i];
      if (banishMode) {
        if (o.kind <= 1) g.P.banishedW.push_back(o.id); else if (o.kind <= 3) g.P.banishedT.push_back(o.id);
        g.P.banishes--; banishMode = false; g.genLevelOffers(); audioPlay(SFX_DENY);
      } else {
        g.applyOffer(o); g.pendingLevels--; g.state = GS_PLAY; audioPlay(SFX_POWERUP);
        if (g.pendingLevels > 0) { g.genLevelOffers(); g.state = GS_LEVELUP; }
      }
      return;
    }
  }
  float by = y0 + ch + 8, bw = 92;
  float bx = u.W * 0.5f - (bw * 3 + 12) * 0.5f;
  if (u.button(bx, by, bw, 20, fmt(tr("REROLL %d"), g.P.rerolls), Col(50, 90, 140, 240), 1, g.P.rerolls > 0, ICON_UI0 + UI_REROLL)) { g.P.rerolls--; g.genLevelOffers(); }
  if (u.button(bx + bw + 6, by, bw, 20, fmt(tr("SKIP %d"), g.P.skips), Col(80, 80, 90, 240), 1, g.P.skips > 0, ICON_UI0 + UI_SKIP)) { g.P.skips--; g.pendingLevels--; g.state = g.pendingLevels > 0 ? GS_LEVELUP : GS_PLAY; if (g.pendingLevels > 0) g.genLevelOffers(); }
  if (u.button(bx + 2 * (bw + 6), by, bw, 20, fmt(tr("BANISH %d"), g.P.banishes), banishMode ? Col(170, 40, 40, 240) : Col(110, 40, 40, 240), 1, g.P.banishes > 0, ICON_UI0 + UI_BANISH)) banishMode = !banishMode;
}

void App::drawChest() {
  UI& u = ui; Game& g = game;
  bgDim(0.55f);
  float w = 190, h = 160, x = u.W * 0.5f - w / 2, y = u.H * 0.5f - h / 2 - 8;
  float reveal = set.instantChests ? 0 : 0.9f;
  bool shown = g.chestAnim >= reveal;
  int rar = g.chestRarity;
  u.panel(x, y, w, h, Col(22, 16, 34, 245), shown ? RC(rar) : Col(255, 255, 255, 100));
  u.text(x + w / 2, y + 6, tr("CHEST"), 1.5f, GOLD, AL_CENTER);
  if (!shown) {
    int fake = (int)(g.chestAnim * 22) % IT_COUNT;
    u.sprite(SP_GLOW, x + w / 2 - 30, y + 30, 60, 60, Col(255, 255, 255, 80));
    u.icon(ICON_ITEM0 + fake, x + w / 2 - 20, y + 40, 40, Col(255, 255, 255, 200));
    for (int k = 0; k < 6; k++) { float a = g.realTime * 6 + k; u.sprite(SP_STAR, x + w / 2 + cosf(a) * 40 - 3, y + 60 + sinf(a) * 30 - 3, 6, 6, RC((k + (int)(g.chestAnim * 10)) % 5)); }
  } else {
    const ItemDef& it = gItems[g.chestItem];
    u.sprite(SP_GLOW, x + w / 2 - 34, y + 22, 68, 68, Col(RC(rar).r, RC(rar).g, RC(rar).b, 130));
    u.icon(ICON_ITEM0 + g.chestItem, x + w / 2 - 20, y + 34, 40);
    u.text(x + w / 2, y + 80, it.name, 1, WHITE, AL_CENTER);
    u.text(x + w / 2, y + 92, kRarityName[rar], 1, RC(rar), AL_CENTER);
    u.textBox(x + 8, y + 105, w - 16, it.desc, 1, GREY, AL_CENTER, 10);
    if (u.button(x + w / 2 - 50, y + h + 6, 100, 22, tr("TAKE"), Col(60, 150, 70, 240)) || padStart) { g.addItem(g.chestItem); g.recomputeStats(); g.state = GS_PLAY; audioPlay(SFX_POWERUP); }
  }
}

void App::drawShrine() {
  UI& u = ui; Game& g = game;
  bgDim(0.5f);
  u.text(u.W * 0.5f, u.H * 0.06f, "CHARGE SHRINE", 2, CYAN, AL_CENTER);
  u.text(u.W * 0.5f, u.H * 0.06f + 22, tr("Choose a permanent bonus"), 1, GREY, AL_CENTER);
  int n = (int)g.offers.size();
  float cw = std::min(130.f, (u.W - 40) / 3), ch = std::min(130.f, u.H * 0.55f);
  float x0 = u.W * 0.5f - (n * cw + (n - 1) * 8) * 0.5f, y0 = u.H * 0.06f + 36;
  for (int i = 0; i < n; i++) {
    float x = x0 + i * (cw + 8);
    drawOfferCard(g.offers[i], x, y0, cw, ch, false);
    if (u.clicked(x, y0, cw, ch)) { g.applyOffer(g.offers[i]); g.state = GS_PLAY; audioPlay(SFX_POWERUP); return; }
  }
}

void App::drawMoai() {
  UI& u = ui; Game& g = game;
  bgDim(0.5f);
  u.text(u.W * 0.5f, u.H * 0.06f, "MOAI", 2, Col(200, 200, 200), AL_CENTER);
  u.text(u.W * 0.5f, u.H * 0.06f + 22, tr("Choose 1 item"), 1, GREY, AL_CENTER);
  int n = (int)g.offers.size();
  float cw = std::min(140.f, (u.W - 40) / 3), ch = std::min(150.f, u.H * 0.6f);
  float x0 = u.W * 0.5f - (n * cw + (n - 1) * 8) * 0.5f, y0 = u.H * 0.06f + 36;
  for (int i = 0; i < n; i++) {
    float x = x0 + i * (cw + 8);
    drawOfferCard(g.offers[i], x, y0, cw, ch, false);
    if (u.clicked(x, y0, cw, ch)) { g.applyOffer(g.offers[i]); g.state = GS_PLAY; audioPlay(SFX_POWERUP); return; }
  }
}

void App::drawShopOverlay() {
  UI& u = ui; Game& g = game;
  bgDim(0.5f);
  if (g.curInter < 0 || g.curInter >= (int)g.inters.size()) { g.state = GS_PLAY; return; }
  Inter& it = g.inters[g.curInter];
  u.text(u.W * 0.5f, u.H * 0.05f, "SHADY GUY", 2, RC(it.rarity), AL_CENTER);
  u.text(u.W * 0.5f, u.H * 0.05f + 22, fmt(tr("Your gold: %s"), fmtNum(floor(g.P.gold)).c_str()), 1, GOLD, AL_CENTER);
  float cw = std::min(140.f, (u.W - 40) / 3), ch = std::min(140.f, u.H * 0.55f);
  float x0 = u.W * 0.5f - (3 * cw + 16) * 0.5f, y0 = u.H * 0.05f + 36;
  float mult[R_COUNT] = {1.f, 1.3f, 1.6f, 2.4f, 3.5f};
  for (size_t i = 0; i < it.stock.size(); i++) {
    float x = x0 + i * (cw + 8);
    Offer o; o.kind = 5; o.id = it.stock[i]; o.rarity = gItems[o.id].rarity;
    int price = (int)(g.chestCost() * mult[o.rarity]);
    drawOfferCard(o, x, y0, cw, ch, false);
    if (it.sold[i]) { u.rect(x, y0, cw, ch, Col(0, 0, 0, 170)); u.text(x + cw / 2, y0 + ch / 2 - 5, tr("SOLD"), 1.5f, DIM, AL_CENTER); continue; }
    bool can = g.P.gold >= price;
    if (u.button(x + 8, y0 + ch + 4, cw - 16, 20, fmt(tr("%d gold"), price), can ? Col(150, 120, 40, 240) : Col(50, 50, 55, 200), 1, can)) { g.P.gold -= price; it.sold[i] = true; g.addItem(o.id); g.recomputeStats(); audioPlay(SFX_COIN); }
  }
  if (u.button(u.W * 0.5f - 50, u.H - 26 - u.safeB, 100, 22, tr("QUIT"), Col(70, 70, 120, 235)) || padStart) g.state = GS_PLAY;
}

void App::drawMicrowave() {
  UI& u = ui; Game& g = game;
  bgDim(0.5f);
  if (g.curInter < 0 || g.curInter >= (int)g.inters.size()) { g.state = GS_PLAY; return; }
  Inter& it = g.inters[g.curInter];
  int rar = it.rarity == R_UNCOMMON ? R_RARE : it.rarity;
  u.text(u.W * 0.5f, 8, "MICROWAVE", 2, RC(rar), AL_CENTER);
  u.textBox(8, 28, u.W - 16, fmt(tr("Duplicates a %s item (consumes another of the same rarity) • %d uses"), tr(kRarityName[rar]), it.uses), 1, GREY, AL_CENTER, 10);
  std::vector<int> owned; for (int i = 0; i < IT_COUNT; i++) if (g.P.items[i] > 0 && gItems[i].rarity == rar) owned.push_back(i);
  float cell = 30; int cols = std::max(1, (int)((u.W - 60) / cell));
  float x0 = u.W * 0.5f - std::min((int)owned.size(), cols) * cell * 0.5f, y0 = 54;
  if (owned.empty()) u.text(u.W * 0.5f, u.H * 0.4f, tr("You have no items of this rarity."), 1, DIM, AL_CENTER);
  for (size_t k = 0; k < owned.size(); k++) {
    float x = x0 + (k % cols) * cell, y = y0 + (k / cols) * cell;
    u.panel(x, y, cell - 2, cell - 2, microSel == owned[k] ? Col(80, 60, 120, 240) : Col(20, 14, 30, 220), RC(rar));
    u.icon(ICON_ITEM0 + owned[k], x + 3, y + 3, cell - 8);
    u.text(x + cell - 4, y + cell - 12, fmt("%d", g.P.items[owned[k]]), 1, WHITE, AL_RIGHT);
    if (u.clicked(x, y, cell - 2, cell - 2) || u.isFocus(x, y, cell - 2, cell - 2)) microSel = owned[k];
  }
  if (microSel >= 0) u.textBox(30, u.H - 70, u.W - 60, std::string(gItems[microSel].name) + ": " + tr(gItems[microSel].desc), 1, WHITE, AL_CENTER, 10);
  bool can = microSel >= 0 && it.uses > 0 && g.P.items[microSel] > 0;
  if (u.button(u.W * 0.5f - 105, u.H - 28 - u.safeB, 100, 22, tr("DUPLICATE"), can ? Col(60, 150, 70, 240) : Col(50, 50, 55, 200), 1, can)) {
    std::vector<int> others; for (int i : owned) if (i != microSel && g.P.items[i] > 0) others.push_back(i);
    if (!others.empty()) g.P.items[others[g.rng.irange((int)others.size())]]--;
    g.addItem(microSel); it.uses--; g.recomputeStats(); audioPlay(SFX_CHEST);
    if (it.uses <= 0) { it.used = true; g.state = GS_PLAY; }
  }
  if (u.button(u.W * 0.5f + 5, u.H - 28 - u.safeB, 100, 22, tr("QUIT"), Col(70, 70, 120, 235))) { g.state = GS_PLAY; microSel = -1; }
}

void App::drawPause() {
  UI& u = ui; Game& g = game; Player& P = g.P;
  bgDim(0.7f);
  float L = 8 + u.safeL, R = u.W - 8 - u.safeR, Wd = R - L;
  u.text(u.W * 0.5f, 5, tr("PAUSED"), 2, WHITE, AL_CENTER);
  float lw = Wd * 0.55f, top = 26, bottom = u.H - 32;
  u.panel(L, top, lw, bottom - top);
  float y = top + 5;
  bool nar = narrow();
  if (nar) { u.text(L + 5, y, u.fit(fmt("%s  •  %s", gChars[P.ch].name, gMaps[g.cfg.map].tiers[std::min(2, g.cfg.tier)].name), lw - 10), 1, GOLD); y += 11; u.text(L + 5, y, fmt(tr("Stage %d/%d  •  %s"), g.stage + 1, g.numStages, fmtTime(g.runTime).c_str()), 1, GREY); y += 13; }
  else { u.text(L + 5, y, fmt(tr("%s  •  %s  •  Stage %d/%d"), gChars[P.ch].name, gMaps[g.cfg.map].tiers[std::min(2, g.cfg.tier)].name, g.stage + 1, g.numStages), 1, GOLD); y += 13; }
  for (size_t i = 0; i < P.weapons.size(); i++) {
    const WeaponInst& w = P.weapons[i];
    u.icon(ICON_WEAPON0 + w.def, L + 5, y, 14);
    std::string dps = nar ? fmt("%s/s", fmtShort(w.dmgDealt / std::max(1.f, g.runTime)).c_str()) : fmt(tr("%s/s  •  %d kills"), fmtShort(w.dmgDealt / std::max(1.f, g.runTime)).c_str(), w.kills);
    u.text(L + 22, y + 3, u.fit(fmt(tr("%s Lv%d"), gWeapons[w.def].name, w.level), lw - 32 - u.textW(dps)), 1, WHITE);
    u.text(L + lw - 6, y + 3, dps, 1, Col(255, 230, 160), AL_RIGHT);
    y += 16;
  }
  for (int i = 0; i < P.ntomes; i++) { u.icon(ICON_TOME0 + P.tomes[i].def, L + 5 + i * 34, y, 14); u.text(L + 21 + i * 34, y + 3, fmt("%d", P.tomes[i].level), 1, WHITE); }
  if (P.ntomes) y += 17;
  int col = 0; float ix = L + 5;
  for (int i = 0; i < IT_COUNT; i++) {
    if (!P.items[i]) continue;
    float x = ix + col * 20; if (x + 18 > L + lw) { col = 0; y += 20; x = ix; }
    if (y > bottom - 20) break;
    u.panel(x, y, 18, 18, Col(20, 14, 30, 220), RC(gItems[i].rarity)); u.icon(ICON_ITEM0 + i, x + 1, y + 1, 16);
    if (P.items[i] > 1) u.text(x + 18, y + 9, fmt("%d", P.items[i]), 1, WHITE, AL_RIGHT);
    col++;
  }
  // stats
  float sx = L + lw + 6, sw = R - sx;
  u.panel(sx, top, sw, bottom - top);
  scrollY[3] += u.dragY(sx, top, sw, bottom - top);
  float rows = S_COUNT * 10.f; scrollY[3] = clampf(scrollY[3], -std::max(0.f, rows - (bottom - top - 10)), 0);
  for (int s = 0; s < S_COUNT; s++) {
    float yy = top + 5 + s * 10 + scrollY[3]; if (yy < top + 2 || yy > bottom - 10) continue;
    std::string sv = statValueStr(s, P.st[s], false);
    u.text(sx + 5, yy, u.fit(kStats[s].name, sw - 14 - u.textW(sv)), 1, GREY);
    u.text(R - 5, yy, sv, 1, WHITE, AL_RIGHT);
  }
  float bw = (Wd - 15) / 4;
  if (u.button(L, u.H - 27, bw, 22, tr("CONTINUE"), Col(60, 150, 70, 240)) || padStart) g.state = GS_PLAY;
  if (u.button(L + bw + 5, u.H - 27, bw, 22, tr("MAP"), Col(70, 70, 120, 235))) showMap = true;
  if (u.button(L + 2 * (bw + 5), u.H - 27, bw, 22, tr("OPTIONS"), Col(70, 70, 120, 235))) { settingsBack = SCR_GAME; scr = SCR_SETTINGS; }
  if (u.button(L + 3 * (bw + 5), u.H - 27, bw, 22, tr("GIVE UP"), Col(150, 50, 50, 240))) { g.P.hp = 0; g.state = GS_DEAD; g.deathT = 0; }
}

void App::drawEnd() {
  UI& u = ui; Game& g = game; Player& P = g.P;
  static bool recorded = false; static const Game* who = nullptr;
  if (g.deathT < 0.05f) recorded = false;
  if (!recorded || who != &g) {
    recorded = true; who = &g;
    meta.silver += P.silver; meta.totalKills += P.kills; meta.bestKills = std::max(meta.bestKills, P.kills);
    if (g.state == GS_VICTORY) { meta.wins++; float& bt = meta.bestTime[g.cfg.map][g.cfg.tier]; if (bt <= 0 || g.runTime < bt) bt = g.runTime; }
    save();
  }
  bgDim(sat(g.deathT * 1.5f) * 0.75f);
  bool win = g.state == GS_VICTORY;
  u.text(u.W * 0.5f, 10, win ? tr("VICTORY!") : tr("YOU DIED"), 3, win ? GOLD : RED, AL_CENTER);
  float L = 10 + u.safeL, R = u.W - 10 - u.safeR, Wd = R - L, top = 48, bottom = u.H - 34;
  u.panel(L, top, Wd * 0.45f, bottom - top);
  float y = top + 6;
  auto line = [&](const std::string& a, const std::string& b, Col c = WHITE) { u.text(L + 6, y, a, 1, GREY); u.text(L + Wd * 0.45f - 6, y, b, 1, c, AL_RIGHT); y += 12; };
  line(tr("Character"), gChars[P.ch].name, GOLD);
  line(tr("Map"), fmt(narrow() ? "%s T%d" : tr("%s (Tier %d)"), gMaps[g.cfg.map].name, g.cfg.tier + 1));
  line(tr("Stage"), fmt("%d/%d", g.stage + 1, g.numStages));
  line(tr("Time"), fmtTime(g.runTime));
  line(tr("Level"), fmt("%d", P.level));
  line(tr("Kills"), fmtNum(P.kills));
  line(tr("Gold"), fmtNum(floor(P.gold)), GOLD);
  line(narrow() ? tr("Silver") : tr("Silver earned"), fmtNum(P.silver), SILV);
  line(tr("Items"), fmt("%d", [&] { int n = 0; for (int i = 0; i < IT_COUNT; i++) n += P.items[i]; return n; }()));
  float dx = L + Wd * 0.45f + 6, dw = R - dx;
  u.panel(dx, top, dw, bottom - top);
  u.text(dx + 6, top + 6, tr("DAMAGE BY SOURCE"), 1, CYAN);
  struct E { int src; double d; }; std::vector<E> v; double tot = 0;
  for (int i = 0; i < SRC_COUNT; i++) if (g.dmgBySrc[i] > 0) { v.push_back({i, g.dmgBySrc[i]}); tot += g.dmgBySrc[i]; }
  std::sort(v.begin(), v.end(), [](const E& a, const E& b) { return a.d > b.d; });
  float yy = top + 20;
  for (size_t k = 0; k < v.size() && yy < bottom - 12; k++) {
    int s = v[k].src; std::string nm; int ic = -1;
    if (s < (int)P.weapons.size()) { nm = gWeapons[P.weapons[s].def].name; ic = ICON_WEAPON0 + P.weapons[s].def; }
    else if (s >= SRC_ITEM && s < SRC_ITEM + IT_COUNT) { nm = gItems[s - SRC_ITEM].name; ic = ICON_ITEM0 + s - SRC_ITEM; }
    else nm = tr("Others");
    if (ic >= 0) u.icon(ic, dx + 6, yy - 1, 10);
    float frac = (float)(v[k].d / std::max(1.0, tot));
    std::string dv = fmtShort(v[k].d) + fmt(" (%d%%)", (int)(frac * 100));
    u.text(dx + 19, yy, u.fit(nm, dw - 29 - u.textW(dv)), 1, WHITE);
    u.rect(dx + 6, yy + 9, (dw - 12) * frac, 2, Col(255, 200, 80));
    u.text(dx + dw - 6, yy, dv, 1, Col(255, 230, 160), AL_RIGHT);
    yy += 14;
  }
  float bw = 120;
  if (u.button(u.W * 0.5f - bw - 4, u.H - 28, bw, 22, tr("PLAY AGAIN"), Col(60, 150, 70, 240))) { startRun(); return; }
  if (u.button(u.W * 0.5f + 4, u.H - 28, bw, 22, tr("MENU"), Col(70, 70, 120, 235))) { scr = SCR_TITLE; inRun = false; }
}

void App::drawGame() {
  Game& g = game;
  if (g.state == GS_TRANSITION) { bgDim(sat(g.transitionT / 0.6f)); ui.text(ui.W * 0.5f, ui.H * 0.45f, "...", 3, WHITE, AL_CENTER); return; }
  drawWorldUI();
  bool overlay = g.state != GS_PLAY;
  if (g.state != GS_DEAD && g.state != GS_VICTORY && !(overlay && narrow())) drawHUD();   // 320-wide screens: overlays replace the HUD
  switch (g.state) {
    case GS_LEVELUP: drawLevelUp(); break;
    case GS_CHEST: drawChest(); break;
    case GS_SHRINE: drawShrine(); break;
    case GS_MOAI: drawMoai(); break;
    case GS_SHOP: drawShopOverlay(); break;
    case GS_MICROWAVE: drawMicrowave(); break;
    case GS_PAUSE: if (showMap) drawFullMap(); else drawPause(); break;
    case GS_DEAD: case GS_VICTORY: drawEnd(); break;
    default: break;
  }
}
