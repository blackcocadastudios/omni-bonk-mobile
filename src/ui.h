// Immediate-mode pixel UI with multi-touch
#pragma once
#include "base.h"
#include "platform.h"
#include "art.h"

struct Touch { int id; v2 pos, start, prev; float t0 = 0; bool down = true, released = false, began = true; int owner = 0; };

enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };
// gamepad focus navigation: every clickable rect registers itself each frame; the D-pad moves between them
enum { FOCUS_ADJUST = 1, FOCUS_DEFAULT = 2 };
struct FocusRect { float x = 0, y = 0, w = 0, h = 0; int flags = 0, group = 0; };

struct UI {
  float W = 480, H = 270, s = 1;      // virtual size and px per unit
  float safeL = 0, safeR = 0, safeT = 0, safeB = 0;
  int screenW = 0, screenH = 0;
  float time = 0;
  std::vector<Touch> touches;
  bool anyBegan = false;
  void begin(int sw, int sh, float uiScale, const PlatformInfo& pi, float dt);
  void event(const InputEv& e);
  void end();
  // drawing
  void rect(float x, float y, float w, float h, Col c);
  void frame(float x, float y, float w, float h, Col c, float t = 1);
  void panel(float x, float y, float w, float h, Col fill = Col(16, 12, 24, 225), Col border = Col(255, 255, 255, 60));
  void icon(int id, float x, float y, float size, Col c = Col(255, 255, 255));
  void sprite(int sp, float x, float y, float w, float h, Col c);
  float text(float x, float y, const std::string& str, float scale = 1, Col c = Col(255, 255, 255), int align = AL_LEFT);
  float textW(const std::string& str, float scale = 1);
  // Text entry points translate `str` through the i18n table first (exact match on the English text).
  // *Raw variants draw/measure the string as given; the layout helpers translate once and then use them.
  float textRaw(float x, float y, const std::string& str, float scale, Col c, int align);
  float textWRaw(const std::string& str, float scale);
  int wrap(const std::string& str, float maxW, float scale, std::vector<std::string>& out);
  float textBox(float x, float y, float w, const std::string& str, float scale, Col c, int align = AL_LEFT, float lineH = 11);
  // interaction
  bool inside(v2 p, float x, float y, float w, float h) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
  bool clicked(float x, float y, float w, float h);   // released inside after pressing inside (unclaimed)
  bool held(float x, float y, float w, float h);      // a touch currently down inside
  bool button(float x, float y, float w, float h, const std::string& label, Col fill = Col(60, 50, 90, 235), float scale = 1, bool enabled = true, int iconId = -1);
  v2 toV(float px, float py) const { return v2(px / s, py / s); }
  Touch* find(int id) { for (auto& t : touches) if (t.id == id) return &t; return nullptr; }
  float dragY(float x, float y, float w, float h); // vertical drag delta inside area this frame
  std::string fit(const std::string& str, float maxW, float scale = 1); // truncate with ".." to fit maxW
  // ---- gamepad focus
  bool padMode = false, hasFocus = false, focusMoved = false, padAccept = false;
  FocusRect focus; std::vector<FocusRect> foc, focPrev;
  int navX = 0, navY = 0, adjustPending = 0, group = 0, nextFlags = 0, resetFrames = 0;
  float padScroll = 0;
  void padInput(int nx, int ny, bool accept, float scroll) { navX = nx; navY = ny; padAccept = accept; padScroll = scroll; }
  void resetFocus() { hasFocus = false; resetFrames = 1; }        // screen changed: pick the default next frame
  void focusable(float x, float y, float w, float h, int flags = 0);
  bool isFocus(float x, float y, float w, float h) const;
  int adjust(float x, float y, float w, float h);                 // -1/+1: left/right on the focused FOCUS_ADJUST rect
  float focusScroll(int grp, float top, float bottom);            // delta that keeps a list's focused row visible
  void drawFocus();
  void resolveFocus();
};
