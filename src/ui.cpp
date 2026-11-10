#include "i18n.h"
#include "ui.h"
#include "render.h"
#include "audio.h"

void UI::begin(int sw, int sh, float uiScale, const PlatformInfo& pi, float dt) {
  screenW = sw; screenH = sh; time += dt;
  float base = sh / 270.f * uiScale;
  s = std::max(1.f, floorf(base + 0.35f));
  if (sh / s < 200) s = std::max(1.f, s - 1);
  W = sw / s; H = sh / s;
  safeL = pi.safeLeft / s; safeR = pi.safeRight / s; safeT = pi.safeTop / s; safeB = pi.safeBottom / s;
  focPrev.swap(foc); foc.clear(); group = 0; nextFlags = 0; focusMoved = false; adjustPending = 0;
  if (padMode) resolveFocus(); else hasFocus = false;
}

// ---------------------------------------------------------------- gamepad focus
static float fcx(const FocusRect& r) { return r.x + r.w * 0.5f; }
static float fcy(const FocusRect& r) { return r.y + r.h * 0.5f; }
void UI::focusable(float x, float y, float w, float h, int flags) {
  FocusRect r; r.x = x; r.y = y; r.w = w; r.h = h; r.flags = flags | nextFlags; r.group = group; nextFlags = 0;
  foc.push_back(r);
}
bool UI::isFocus(float x, float y, float w, float h) const {
  return padMode && hasFocus && fabsf(x + w * 0.5f - fcx(focus)) < 2.5f && fabsf(y + h * 0.5f - fcy(focus)) < 2.5f;
}
int UI::adjust(float x, float y, float w, float h) {
  if (!adjustPending || !isFocus(x, y, w, h)) return 0;
  int a = adjustPending; adjustPending = 0; return a;
}
float UI::focusScroll(int grp, float top, float bottom) {
  if (!padMode || !hasFocus || !focusMoved || focus.group != grp) return 0;
  float dy = 0;
  if (focus.y < top + 1) dy = top + 1 - focus.y;
  else if (focus.y + focus.h > bottom - 1) dy = bottom - 1 - (focus.y + focus.h);
  focus.y += dy;
  return dy;
}
void UI::resolveFocus() {
  if (resetFrames > 0) { resetFrames--; hasFocus = false; padAccept = false; return; } // old screen's rects: wait a frame
  if (focPrev.empty()) { hasFocus = false; return; }
  int cur = -1;
  if (hasFocus) {
    float best = 1e9f; int bi = -1;
    for (size_t i = 0; i < focPrev.size(); i++) { float d = fabsf(fcx(focPrev[i]) - fcx(focus)) + fabsf(fcy(focPrev[i]) - fcy(focus)); if (d < best) { best = d; bi = (int)i; } }
    cur = bi; if (best > 3) focusMoved = true; // the focused widget went away: nearest one takes over
  } else {
    for (size_t i = 0; i < focPrev.size() && cur < 0; i++) if (focPrev[i].flags & FOCUS_DEFAULT) cur = (int)i;
    if (cur < 0) cur = 0;
    focusMoved = true;
  }
  focus = focPrev[cur]; hasFocus = true;
  if (!navX && !navY) return;
  if ((focus.flags & FOCUS_ADJUST) && navX) { adjustPending = navX; return; }
  // spatial search: nearest rect in the pressed direction, strongly preferring ones aligned with the current one
  int best = -1; float bs = 1e9f;
  for (size_t i = 0; i < focPrev.size(); i++) {
    if ((int)i == cur) continue;
    const FocusRect& r = focPrev[i];
    float dx = fcx(r) - fcx(focus), dy = fcy(r) - fcy(focus);
    float along = navX ? dx * navX : dy * navY, perp = navX ? fabsf(dy) : fabsf(dx);
    if (along < 1.5f) continue;
    float span = navX ? (r.h + focus.h) * 0.5f : (r.w + focus.w) * 0.5f;
    float gap = std::max(0.f, perp - span * 0.8f);
    float score = along + gap * 4 + perp * 0.35f;
    if (gap > along * 2.5f + 8) score += 10000; // far off-axis: only when nothing else lies that way
    if (score < bs) { bs = score; best = (int)i; }
  }
  if (best >= 0) { focus = focPrev[best]; focusMoved = true; audioPlay(SFX_CLICK, 0.45f, 1.5f); }
}
void UI::drawFocus() {
  if (!padMode || !hasFocus || foc.empty()) return;
  const FocusRect* r = nullptr; float best = 24;
  for (auto& f : foc) { float d = fabsf(fcx(f) - fcx(focus)) + fabsf(fcy(f) - fcy(focus)); if (d < best) { best = d; r = &f; } }
  if (!r || r->y + r->h < 0 || r->y > H) return;
  float k = 0.5f + 0.5f * sinf(time * 7);
  Col c(255, (u8)(200 + 40 * k), (u8)(60 + 120 * k), 255);
  frame(r->x - 2, r->y - 2, r->w + 4, r->h + 4, Col(0, 0, 0, 160), 1);
  frame(r->x - 1, r->y - 1, r->w + 2, r->h + 2, c, 1);
  float o = 2 + k; // corner ticks
  rect(r->x - o - 1, r->y - o - 1, 4, 1, c); rect(r->x - o - 1, r->y - o - 1, 1, 4, c);
  rect(r->x + r->w + o - 3, r->y - o - 1, 4, 1, c); rect(r->x + r->w + o, r->y - o - 1, 1, 4, c);
  rect(r->x - o - 1, r->y + r->h + o, 4, 1, c); rect(r->x - o - 1, r->y + r->h + o - 3, 1, 4, c);
  rect(r->x + r->w + o - 3, r->y + r->h + o, 4, 1, c); rect(r->x + r->w + o, r->y + r->h + o - 3, 1, 4, c);
}
std::string UI::fit(const std::string& in, float maxW, float scale) {
  std::string str = i18nActive() ? std::string(trv(in)) : in;   // translated once; the result is drawn with textRaw by callers via text() (no table hit)
  if (textWRaw(str, scale) <= maxW) return str;
  std::string out; size_t i = 0;
  while (i < str.size()) {
    size_t j = i; utf8Next(str, j);
    std::string cand = out + str.substr(i, j - i);
    if (textWRaw(cand + "..", scale) > maxW) break;
    out = cand; i = j;
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out + "..";
}

void UI::event(const InputEv& e) {
  if (e.type == EV_DOWN) {
    Touch t; t.id = e.id; t.pos = t.start = t.prev = toV(e.x, e.y); t.t0 = time; t.down = true; t.began = true;
    for (auto& o : touches) if (o.id == e.id) { o = t; return; }
    touches.push_back(t);
  } else if (e.type == EV_MOVE) {
    if (Touch* t = find(e.id)) t->pos = toV(e.x, e.y);
  } else if (e.type == EV_UP || e.type == EV_CANCEL) {
    if (Touch* t = find(e.id)) { t->pos = toV(e.x, e.y); t->down = false; t->released = e.type == EV_UP; }
  }
}

void UI::end() {
  touches.erase(std::remove_if(touches.begin(), touches.end(), [](const Touch& t) { return !t.down; }), touches.end());
  for (auto& t : touches) { t.began = false; t.prev = t.pos; }
}

void UI::rect(float x, float y, float w, float h, Col c) { gR.uiRect(x * s, y * s, w * s, h * s, c); }
void UI::frame(float x, float y, float w, float h, Col c, float t) {
  rect(x, y, w, t, c); rect(x, y + h - t, w, t, c); rect(x, y + t, t, h - 2 * t, c); rect(x + w - t, y + t, t, h - 2 * t, c);
}
void UI::panel(float x, float y, float w, float h, Col fill, Col border) {
  rect(x + 1, y, w - 2, h, fill); rect(x, y + 1, 1, h - 2, fill); rect(x + w - 1, y + 1, 1, h - 2, fill);
  // pixel corners border
  rect(x + 2, y, w - 4, 1, border); rect(x + 2, y + h - 1, w - 4, 1, border); rect(x, y + 2, 1, h - 4, border); rect(x + w - 1, y + 2, 1, h - 4, border);
  rect(x + 1, y + 1, 1, 1, border); rect(x + w - 2, y + 1, 1, 1, border); rect(x + 1, y + h - 2, 1, 1, border); rect(x + w - 2, y + h - 2, 1, 1, border);
}
void UI::icon(int id, float x, float y, float size, Col c) {
  if (id < 0 || id >= 512) return;
  gR.uiQuad(x * s, y * s, size * s, size * s, gR.atlas.icon[id], c);
}
void UI::sprite(int sp, float x, float y, float w, float h, Col c) { gR.uiQuad(x * s, y * s, w * s, h * s, gR.atlas.sprite[sp], c); }

float UI::textW(const std::string& str, float scale) {
  if (i18nActive()) return textWRaw(std::string(trv(str)), scale);
  return textWRaw(str, scale);
}
float UI::textWRaw(const std::string& str, float scale) {
  float w = 0, best = 0; size_t i = 0;
  while (i < str.size()) { int cp = utf8Next(str, i); if (cp == '\n') { best = std::max(best, w); w = 0; continue; } w += gR.atlas.glyphW[glyphIdx(cp)] * scale; }
  return std::max(best, w);
}

float UI::text(float x, float y, const std::string& str, float scale, Col c, int align) {
  if (i18nActive()) return textRaw(x, y, std::string(trv(str)), scale, c, align);
  return textRaw(x, y, str, scale, c, align);
}
float UI::textRaw(float x, float y, const std::string& str, float scale, Col c, int align) {
  float w = textWRaw(str, scale);
  float cx = align == AL_CENTER ? x - w * 0.5f : align == AL_RIGHT ? x - w : x;
  // snap to physical pixels
  float px = roundf(cx * s), py = roundf(y * s); float k = scale * s;
  size_t i = 0; float startX = px;
  while (i < str.size()) {
    int cp = utf8Next(str, i);
    if (cp == '\n') { px = startX; py += 11 * k; continue; }
    const ARect& g = gR.atlas.glyph[glyphIdx(cp)];
    if (cp != ' ') gR.uiQuad(px - k, py - 2 * k, GLYPH_CW * k, GLYPH_CH * k, g, c);
    px += gR.atlas.glyphW[glyphIdx(cp)] * k;
  }
  return w;
}

int UI::wrap(const std::string& in, float maxW, float scale, std::vector<std::string>& out) {
  out.clear();
  const std::string str = i18nActive() ? std::string(trv(in)) : in;
  std::string line, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    std::string cand = line.empty() ? word : line + " " + word;
    if (textWRaw(cand, scale) > maxW && !line.empty()) { out.push_back(line); line = word; }
    else line = cand;
    word.clear();
  };
  for (size_t i = 0; i < str.size(); i++) {
    char ch = str[i];
    if (ch == ' ') flushWord();
    else if (ch == '\n') { flushWord(); out.push_back(line); line.clear(); }
    else word.push_back(ch);
  }
  flushWord();
  if (!line.empty()) out.push_back(line);
  return (int)out.size();
}

float UI::textBox(float x, float y, float w, const std::string& str, float scale, Col c, int align, float lineH) {
  std::vector<std::string> lines; wrap(str, w, scale, lines);
  float yy = y;
  for (auto& l : lines) { textRaw(align == AL_CENTER ? x + w * 0.5f : x, yy, l, scale, c, align); yy += lineH * scale; }
  return yy - y;
}

bool UI::clicked(float x, float y, float w, float h) {
  focusable(x, y, w, h);
  if (padMode && padAccept && isFocus(x, y, w, h)) { padAccept = false; return true; }
  for (auto& t : touches) {
    if (!t.released || t.owner != 0) continue;
    if (inside(t.start, x, y, w, h) && inside(t.pos, x - 6, y - 6, w + 12, h + 12) && len(t.pos - t.start) < 24) { t.owner = -1; return true; }
  }
  return false;
}
bool UI::held(float x, float y, float w, float h) {
  for (auto& t : touches) if (t.down && t.owner == 0 && inside(t.pos, x, y, w, h) && inside(t.start, x, y, w, h)) return true;
  return false;
}
float UI::dragY(float x, float y, float w, float h) {
  for (auto& t : touches) if (t.down && inside(t.start, x, y, w, h)) { if (fabsf(t.pos.y - t.start.y) > 6) t.owner = t.owner == 0 ? -2 : t.owner; return t.pos.y - t.prev.y; }
  return padMode ? padScroll : 0;
}

bool UI::button(float x, float y, float w, float h, const std::string& label, Col fill, float scale, bool enabled, int iconId) {
  bool hold = enabled && held(x, y, w, h);
  Col f = enabled ? (hold ? mulc(fill, 0.7f) : fill) : Col(50, 50, 55, 200);
  float yo = hold ? 1 : 0;
  rect(x + 1, y + 2, w, h, Col(0, 0, 0, 120));
  panel(x, y + yo, w, h, f, enabled ? Col(255, 255, 255, 150) : Col(255, 255, 255, 50));
  rect(x + 2, y + yo + 1, w - 4, 1, Col(255, 255, 255, 40));
  float tx = x + w * 0.5f;
  if (iconId >= 0) {
    float isz = std::min(h - 6, 20.f);
    float tw = textW(label, scale);
    float total = isz + (label.empty() ? 0 : 4 + tw);
    icon(iconId, x + (w - total) * 0.5f, y + yo + (h - isz) * 0.5f, isz, enabled ? Col(255, 255, 255) : Col(120, 120, 120));
    if (!label.empty()) text(x + (w - total) * 0.5f + isz + 4, y + yo + (h - 10 * scale) * 0.5f, label, scale, enabled ? Col(255, 255, 255) : Col(140, 140, 140));
  } else if (!label.empty()) text(tx, y + yo + (h - 10 * scale) * 0.5f, label, scale, enabled ? Col(255, 255, 255) : Col(140, 140, 140), AL_CENTER);
  bool c = enabled && clicked(x, y, w, h);
  if (c) audioPlay(SFX_CLICK);
  return c;
}
