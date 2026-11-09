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
  float w = 0, best = 0; size_t i = 0;
  while (i < str.size()) { int cp = utf8Next(str, i); if (cp == '\n') { best = std::max(best, w); w = 0; continue; } w += gR.atlas.glyphW[cp & 255] * scale; }
  return std::max(best, w);
}

float UI::text(float x, float y, const std::string& str, float scale, Col c, int align) {
  float w = textW(str, scale);
  float cx = align == AL_CENTER ? x - w * 0.5f : align == AL_RIGHT ? x - w : x;
  // snap to physical pixels
  float px = roundf(cx * s), py = roundf(y * s); float k = scale * s;
  size_t i = 0; float startX = px;
  while (i < str.size()) {
    int cp = utf8Next(str, i);
    if (cp == '\n') { px = startX; py += 11 * k; continue; }
    const ARect& g = gR.atlas.glyph[cp & 255];
    if ((cp & 255) != ' ') gR.uiQuad(px - k, py - 2 * k, GLYPH_CW * k, GLYPH_CH * k, g, c);
    px += gR.atlas.glyphW[cp & 255] * k;
  }
  return w;
}

int UI::wrap(const std::string& str, float maxW, float scale, std::vector<std::string>& out) {
  out.clear();
  std::string line, word;
  auto flushWord = [&]() {
    if (word.empty()) return;
    std::string cand = line.empty() ? word : line + " " + word;
    if (textW(cand, scale) > maxW && !line.empty()) { out.push_back(line); line = word; }
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
  for (auto& l : lines) { text(align == AL_CENTER ? x + w * 0.5f : x, yy, l, scale, c, align); yy += lineH * scale; }
  return yy - y;
}

bool UI::clicked(float x, float y, float w, float h) {
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
  return 0;
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
