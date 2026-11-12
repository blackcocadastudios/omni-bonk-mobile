#include "art.h"

// =====================================================================
//  Texture layers (64x64, tileable, point-filtered pixel textures)
// =====================================================================
static float tfbm(float x, float y, int period, u32 seed, int oct) {
  float s = 0, a = 0.5f, n = 0; int p = period;
  for (int i = 0; i < oct; i++) { s += a * vnoiseTile(x, y, p, seed + i * 77); n += a; x *= 2; y *= 2; p *= 2; a *= 0.5f; }
  return s / n;
}
static inline u8 b8(float v) { return (u8)clampf(v, 0, 255); }
struct Layer {
  Col p[LAYER_SIZE * LAYER_SIZE];
  void set(int x, int y, Col c) { p[(y & 63) * 64 + (x & 63)] = c; }
  Col& at(int x, int y) { return p[(y & 63) * 64 + (x & 63)]; }
};
static Col gray(float v, u8 a = 255) { u8 g = b8(v); return Col(g, g, g, a); }
static Col tint(float v, float r, float g, float b) { return Col(b8(v * r), b8(v * g), b8(v * b)); }

static void genLayer(int id, Layer& L) {
  const int N = LAYER_SIZE;
  u32 sd = 1000 + id * 31;
  for (int y = 0; y < N; y++)
    for (int x = 0; x < N; x++) {
      float u = x / (float)N, v = y / (float)N;
      float n1 = tfbm(u * 4, v * 4, 4, sd, 3);
      float px = hashf(x / 2, y / 2, sd);          // 2x2 pixel blocks
      float p1 = hashf(x, y, sd + 7);
      Col c(255, 255, 255, 255);
      switch (id) {
        case L_PLAIN: c = gray(238 + p1 * 12); break;
        case L_NOISE: c = gray(205 + px * 50); break;
        case L_GRASS: {
          float bl = hashf(x, y / 3, sd + 3);
          float val = 175 + n1 * 60 + (px - 0.5f) * 40;
          if (bl > 0.86f) val += 45; else if (bl < 0.10f) val -= 45;
          c = tint(val, 0.98f, 1.0f, 0.94f);
        } break;
        case L_DIRT: {
          float val = 165 + n1 * 70 + (px - .5f) * 40;
          if (p1 > 0.95f) val += 50;
          c = tint(val, 1.0f, 0.93f, 0.85f);
        } break;
        case L_SAND: {
          float r = sinf((v * 6 + n1 * 1.5f) * TAU) * 0.5f + 0.5f;
          float val = 200 + r * 30 + (px - .5f) * 30; if (p1 > 0.97f) val -= 50;
          c = tint(val, 1.0f, 0.95f, 0.85f);
        } break;
        case L_STONE: case L_GRAVEL: {
          // cell noise cobbles
          int cs = id == L_GRAVEL ? 8 : 16;
          float best = 9, second = 9; int bi = 0;
          for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++) {
              int cx = (x / cs + ox), cy = (y / cs + oy);
              int wx = ((cx % (N / cs)) + N / cs) % (N / cs), wy = ((cy % (N / cs)) + N / cs) % (N / cs);
              float fx = cx * cs + hashf(wx, wy, sd) * cs, fy = cy * cs + hashf(wx, wy, sd + 1) * cs;
              float d = sqrtf(sq(fx - x) + sq(fy - y));
              if (d < best) { second = best; best = d; bi = wx * 13 + wy; } else if (d < second) second = d;
            }
          float edge = second - best;
          float val = 150 + hashf(bi, 0, sd) * 60 + (px - .5f) * 30;
          if (edge < 1.5f) val = 85;
          c = gray(val);
        } break;
        case L_BRICK: {
          int bh = 8, bw = 16; int row = y / bh; int xo = (row & 1) ? bw / 2 : 0;
          int bx = ((x + xo) / bw), lx = (x + xo) % bw, ly = y % bh;
          float val = 160 + hashf(bx, row, sd) * 50 + (px - .5f) * 35;
          if (lx == 0 || ly == 0) val = 80;
          c = gray(val);
        } break;
        case L_BARK: {
          float s = tfbm(u * 8, v * 1, 8, sd, 3);
          float val = 150 + s * 90 + (px - .5f) * 30;
          if (fractf(u * 6 + s * 0.6f) < 0.12f) val = 80;
          c = tint(val, 1.0f, 0.92f, 0.85f);
        } break;
        case L_LEAVES: {
          float f = tfbm(u * 6, v * 6, 6, sd, 3);
          float val;
          if (f < 0.38f) val = 85; else if (f < 0.5f) val = 140; else if (f < 0.62f) val = 190; else val = 235;
          if (p1 > 0.93f) val = 255;
          if (hashf(x / 2, y / 2, sd + 9) < 0.08f) val *= 0.6f;
          c = tint(val, 0.95f, 1.0f, 0.95f);
        } break;
        case L_PINE: {
          float d = fractf((x + y * 0.5f) / 5.f + hashf(x / 5, 0, sd) * 0.5f);
          float val = 130 + n1 * 70 + (d < 0.35f ? 60 : 0) + (px - .5f) * 30;
          c = tint(val, 0.95f, 1.0f, 0.95f);
        } break;
        case L_WOOD: {
          int pl = y / 16; float g = tfbm(u * 2 + pl * 3.1f, v * 16, 2, sd + pl, 2);
          float val = 170 + g * 60 + (px - .5f) * 20;
          if (y % 16 == 0) val = 85;
          if (fractf(u * 3 + pl * 0.37f) < 0.03f) val = 95;
          c = tint(val, 1.0f, 0.9f, 0.8f);
        } break;
        case L_METAL: {
          float val = 185 + (hashf(0, y, sd) - .5f) * 30 + (px - .5f) * 15;
          int lx = x % 32, ly = y % 32;
          if ((lx - 4) * (lx - 4) + (ly - 4) * (ly - 4) < 5) val = 250;
          if (lx == 0 || ly == 0) val = 120;
          c = gray(val);
        } break;
        case L_GOLD: {
          float s = fractf((x + y) / 22.f);
          float val = 200 + (s < 0.2f ? 55 : 0) + (px - .5f) * 25;
          c = tint(val, 1.0f, 0.95f, 0.85f);
        } break;
        case L_CLOTH: {
          float val = 200 + (((x + y) & 3) == 0 ? 25 : 0) + (((x - y) & 3) == 0 ? -20 : 0) + (px - .5f) * 25;
          c = gray(val);
        } break;
        case L_SKIN: c = gray(225 + n1 * 30 + (px - .5f) * 14); break;
        case L_BONE: {
          float val = 225 + (px - .5f) * 24;
          if (fabsf(tfbm(u * 5, v * 5, 5, sd, 2) - 0.5f) < 0.02f) val = 150;
          c = tint(val, 1.0f, 0.97f, 0.88f);
        } break;
        case L_SLIME: {
          float val = 190 + n1 * 40;
          float bx = fractf(u * 4 + hashf(int(v * 4), 3, sd)), by = fractf(v * 4);
          if (sq(bx - .5f) + sq(by - .5f) < 0.03f) val = 255;
          c = gray(val);
        } break;
        case L_CHITIN: {
          float val = 170 + n1 * 50 + (px - .5f) * 20;
          if (y % 12 < 2) val = 100;
          if (y % 12 == 2) val = 240;
          c = gray(val);
        } break;
        case L_FUR: {
          float st = hashf(x, y / 4 + x * 3, sd);
          float val = 200 + n1 * 30 + (st > 0.7f ? 40 : st < 0.2f ? -40 : 0);
          c = gray(val);
        } break;
        case L_WRAPS: {
          float d = fractf((y + x * 0.35f) / 10.f);
          float val = 215 + (px - .5f) * 30 - (d < 0.12f ? 70 : 0) + n1 * 20;
          c = tint(val, 1.0f, 0.96f, 0.86f);
        } break;
        case L_ROCK: {
          float f = tfbm(u * 3, v * 3, 3, sd, 4);
          float val = 140 + f * 90 + (px - .5f) * 40;
          if (fabsf(tfbm(u * 2, v * 2, 2, sd + 5, 2) - 0.5f) < 0.025f) val = 70;
          c = gray(val);
        } break;
        case L_CRYSTAL: {
          float f = fractf((x * 0.7f + y) / 21.f);
          c = gray(170 + f * 85);
        } break;
        case L_TUFT: {
          // grass blades with alpha cutout
          c = Col(0, 0, 0, 0);
          for (int b = 0; b < 9; b++) {
            float bxp = 4 + b * 7 + hashf(b, 0, sd) * 4, hgt = 30 + hashf(b, 1, sd) * 32, lean = (hashf(b, 2, sd) - .5f) * 14;
            float t = (63 - y) / hgt; if (t < 0 || t > 1) continue;
            float cx = bxp + lean * t * t, wdt = 3.2f * (1 - t) + 0.4f;
            if (fabsf(x - cx) < wdt) c = gray(150 + t * 105 + (px - .5f) * 20);
          }
        } break;
        case L_FLOWER: {
          c = Col(0, 0, 0, 0);
          // stems
          for (int b = 0; b < 4; b++) {
            float bx = 8 + b * 15 + hashf(b, 0, sd) * 6, top = 18 + hashf(b, 1, sd) * 20;
            if (y > top && fabsf(x - bx) < 1.2f) c = Col(70, 140, 60);
            float d = sqrtf(sq(x - bx) + sq(y - top));
            if (d < 5.5f) c = (b & 1) ? Col(250, 240, 220) : Col(255, 220, 90);
            if (d < 2) c = Col(250, 170, 40);
          }
        } break;
        case L_TOMB: {
          float val = 165 + n1 * 50 + (px - .5f) * 30;
          if ((x > 28 && x < 35 && y > 10 && y < 40) || (y > 18 && y < 24 && x > 20 && x < 43)) val = 95;
          c = gray(val);
        } break;
        case L_PUMPKIN: {
          float r = fabsf(sinf(u * TAU * 4));
          c = gray(150 + r * 90 + (px - .5f) * 20);
        } break;
        case L_CACTUS: {
          float r = fabsf(sinf(u * TAU * 5));
          float val = 140 + r * 90 + (px - .5f) * 20;
          if ((x % 13 == 6) && (y % 9 == 4)) val = 255;
          c = gray(val);
        } break;
        case L_PORTAL: {
          float dx = u - .5f, dy = v - .5f; float a = atan2f(dy, dx), r = sqrtf(dx * dx + dy * dy);
          float s = fractf(a / TAU * 3 + r * 4);
          c = gray(120 + (s < 0.5f ? 135 : 30));
        } break;
        case L_RUNE: {
          float val = 120 + n1 * 50 + (px - .5f) * 30;
          bool rn = ((x % 16 == 8) && (y % 16 > 3 && y % 16 < 13)) || ((y % 16 == 8) && (x % 16 > 5 && x % 16 < 11)) || ((x + y) % 16 == 0 && (x % 16) < 6);
          c = rn ? Col(255, 255, 255, 255) : gray(val);
        } break;
        case L_LAVA: {
          float f = tfbm(u * 4, v * 4, 4, sd, 3);
          c = fabsf(f - .5f) < 0.04f ? Col(255, 200, 80) : gray(90 + f * 40);
        } break;
        case L_FEATHER: {
          float d = fractf((x - y * 0.6f) / 4.f);
          c = gray(200 + (d < 0.3f ? 40 : 0) + (px - .5f) * 20 - (abs(x - 32) < 1 ? 60 : 0));
        } break;
        default: c = gray(240);
      }
      L.set(x, y, c);
    }
}

void artBuildLayers(std::vector<u8>& out) {
  out.resize((size_t)L_COUNT * LAYER_SIZE * LAYER_SIZE * 4);
  Layer L;
  for (int i = 0; i < L_COUNT; i++) {
    genLayer(i, L);
    memcpy(&out[(size_t)i * LAYER_SIZE * LAYER_SIZE * 4], L.p, LAYER_SIZE * LAYER_SIZE * 4);
  }
}

// =====================================================================
//  Canvas
// =====================================================================
void Canvas::blend(int x, int y, Col c) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  Col& d = px[y * w + x];
  float a = c.a / 255.f;
  if (d.a == 0) { d = c; return; }
  d.r = (u8)lerpf(d.r, c.r, a); d.g = (u8)lerpf(d.g, c.g, a); d.b = (u8)lerpf(d.b, c.b, a); d.a = (u8)std::max<int>(d.a, c.a);
}
void Canvas::rect(float x0, float y0, float x1, float y1, Col c) {
  for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++) for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) blend(x, y, c);
}
void Canvas::circle(float cx, float cy, float r, Col c) { ellipse(cx, cy, r, r, c); }
void Canvas::ellipse(float cx, float cy, float rx, float ry, Col c) {
  for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
    for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++)
      if (sq((x + .5f - cx) / rx) + sq((y + .5f - cy) / ry) <= 1.f) blend(x, y, c);
}
void Canvas::ring(float cx, float cy, float r0, float r1, Col c) {
  for (int y = (int)(cy - r1 - 1); y <= (int)(cy + r1 + 1); y++)
    for (int x = (int)(cx - r1 - 1); x <= (int)(cx + r1 + 1); x++) {
      float d = sqrtf(sq(x + .5f - cx) + sq(y + .5f - cy));
      if (d >= r0 && d <= r1) blend(x, y, c);
    }
}
void Canvas::arc(float cx, float cy, float r0, float r1, float a0, float a1, Col c) {
  for (int y = (int)(cy - r1 - 1); y <= (int)(cy + r1 + 1); y++)
    for (int x = (int)(cx - r1 - 1); x <= (int)(cx + r1 + 1); x++) {
      float dx = x + .5f - cx, dy = y + .5f - cy, d = sqrtf(dx * dx + dy * dy);
      if (d < r0 || d > r1) continue;
      float an = atan2f(dy, dx) / DEG; // -180..180
      float t = an; while (t < a0) t += 360; while (t > a0 + 360) t -= 360;
      if (t <= a1) blend(x, y, c);
    }
}
void Canvas::erase(float x0, float y0, float x1, float y1) {
  for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++) for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) if (x >= 0 && y >= 0 && x < w && y < h) px[y * w + x] = Col(0, 0, 0, 0);
}
void Canvas::line(float x0, float y0, float x1, float y1, float th, Col c) {
  float minx = std::min(x0, x1) - th, maxx = std::max(x0, x1) + th, miny = std::min(y0, y1) - th, maxy = std::max(y0, y1) + th;
  v2 a(x0, y0), b(x1, y1), ab = b - a; float l2 = dot(ab, ab);
  for (int y = (int)floorf(miny); y <= (int)ceilf(maxy); y++)
    for (int x = (int)floorf(minx); x <= (int)ceilf(maxx); x++) {
      v2 p(x + .5f, y + .5f);
      float t = l2 > 0 ? clampf(dot(p - a, ab) / l2, 0, 1) : 0;
      if (len(p - (a + ab * t)) <= th * .5f) blend(x, y, c);
    }
}
void Canvas::tri(v2 a, v2 b, v2 c, Col col) { poly({a, b, c}, col); }
void Canvas::poly(const std::vector<v2>& p, Col c) {
  float minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
  for (auto& q : p) { minx = std::min(minx, q.x); maxx = std::max(maxx, q.x); miny = std::min(miny, q.y); maxy = std::max(maxy, q.y); }
  for (int y = (int)floorf(miny); y <= (int)ceilf(maxy); y++)
    for (int x = (int)floorf(minx); x <= (int)ceilf(maxx); x++) {
      float px_ = x + .5f, py = y + .5f; bool in = false;
      for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++)
        if (((p[i].y > py) != (p[j].y > py)) && (px_ < (p[j].x - p[i].x) * (py - p[i].y) / (p[j].y - p[i].y) + p[i].x)) in = !in;
      if (in) blend(x, y, c);
    }
}
void Canvas::outline(Col c) {
  std::vector<Col> o = px;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      if (get(x, y).a > 0) continue;
      bool n = false;
      for (int dy = -1; dy <= 1 && !n; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && get(x + dx, y + dy).a > 128) { n = true; break; }
      if (n) o[y * w + x] = c;
    }
  px = o;
}
void Canvas::shadeEdges(float k) {
  std::vector<Col> o = px;
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      Col cc = get(x, y); if (cc.a == 0) continue;
      bool br = get(x + 1, y).a == 0 || get(x, y + 1).a == 0;
      bool tl = get(x - 1, y).a == 0 || get(x, y - 1).a == 0;
      if (br) o[y * w + x] = mulc(cc, 1 - k); else if (tl) o[y * w + x] = mulc(cc, 1 + k * 0.8f);
    }
  px = o;
}

// =====================================================================
//  Icons
// =====================================================================
static Col C(u32 h) { return hexc(h); }
static void drawSymbol(Canvas& c, int sym, float cx, float cy, Col col) {
  Col w = col;
  switch (sym) {
    case 1: c.line(cx - 5, cy + 5, cx + 5, cy - 5, 2.5f, w); c.line(cx - 6, cy + 2, cx - 2, cy + 6, 2, w); break;    // sword
    case 2: c.ring(cx, cy, 4, 6, w); c.line(cx, cy, cx, cy - 4, 1.5f, w); c.line(cx, cy, cx + 3, cy, 1.5f, w); break; // clock
    case 3: c.rect(cx - 1.5f, cy - 6, cx + 1.5f, cy + 6, w); c.rect(cx - 6, cy - 1.5f, cx + 6, cy + 1.5f, w); break;  // plus
    case 4: c.ring(cx, cy, 4, 5.5f, w); c.rect(cx - .7f, cy - 7, cx + .7f, cy + 7, w); c.rect(cx - 7, cy - .7f, cx + 7, cy + .7f, w); break;
    case 5: c.line(cx - 6, cy - 6, cx + 6, cy + 6, 2, w); c.line(cx - 6, cy + 6, cx + 6, cy - 6, 2, w); c.circle(cx, cy, 2.5f, w); break;
    case 6: c.circle(cx - 3, cy - 2, 3.5f, w); c.circle(cx + 3, cy - 2, 3.5f, w); c.tri({cx - 6.5f, cy - 1}, {cx + 6.5f, cy - 1}, {cx, cy + 6.5f}, w); break;
    case 7: c.poly({{cx - 6, cy - 6}, {cx + 6, cy - 6}, {cx + 6, cy}, {cx, cy + 7}, {cx - 6, cy}}, w); break;
    case 8: c.ring(cx, cy, 4.5f, 6.5f, w); c.circle(cx - 2, cy - 2, 1.5f, w); break;
    case 9: c.rect(cx - 1.5f, cy - 5, cx + 1.5f, cy + 5, w); c.rect(cx - 5, cy - 1.5f, cx + 5, cy + 1.5f, w); c.ring(cx, cy, 6, 7.3f, w); break;
    case 10: for (int i = 0; i < 3; i++) c.line(cx - 6, cy - 4 + i * 4, cx + 5 - i * 2, cy - 4 + i * 4, 1.6f, w); break;
    case 11: c.circle(cx, cy + 2, 4.5f, w); c.tri({cx - 4.2f, cy + 1}, {cx + 4.2f, cy + 1}, {cx, cy - 7}, w); break;
    case 12: for (int i = 0; i < 4; i++) c.tri({cx - 7 + i * 3.6f, cy + 5}, {cx - 3.4f + i * 3.6f, cy + 5}, {cx - 5.2f + i * 3.6f, cy - 5}, w); break;
    case 13: c.tri({cx - 5, cy - 6}, {cx + 5, cy - 6}, {cx, cy}, w); c.tri({cx - 5, cy + 6}, {cx + 5, cy + 6}, {cx, cy}, w); break;
    case 14: c.arc(cx, cy + 1, 3, 6.5f, -180, 0, w); c.rect(cx - 6.5f, cy + 1, cx - 3, cy + 6, w); c.rect(cx + 3, cy + 1, cx + 6.5f, cy + 6, w); break;
    case 15: c.poly({{cx - 4, cy - 6}, {cx + 1, cy - 6}, {cx + 1, cy + 2}, {cx + 7, cy + 2}, {cx + 7, cy + 6}, {cx - 4, cy + 6}}, w); break;
    case 16: c.circle(cx, cy, 5, w); c.rect(cx - 5, cy - 1, cx + 5, cy, Col(0, 0, 0, 120)); break;
    case 17: c.line(cx - 6, cy, cx + 4, cy, 2.5f, w); c.tri({cx + 2, cy - 5}, {cx + 2, cy + 5}, {cx + 8, cy}, w); break;
    case 18: { std::vector<v2> st; for (int i = 0; i < 10; i++) { float a = -PI / 2 + i * PI / 5, r = (i & 1) ? 3 : 7; st.push_back({cx + cosf(a) * r, cy + sinf(a) * r}); } c.poly(st, w); } break;
    case 19: c.circle(cx, cy, 6, C(0xffd23f)); c.ring(cx, cy, 3.5f, 4.5f, C(0xb8860b)); break;
    case 20: c.circle(cx, cy, 6, C(0xd8dde6)); c.ring(cx, cy, 3.5f, 4.5f, C(0x8a94a6)); break;
    case 21: for (int i = 0; i < 4; i++) { float a = i * PI / 2 + PI / 4; c.circle(cx + cosf(a) * 3.5f, cy + sinf(a) * 3.5f, 3.2f, w); } break;
    case 22: c.circle(cx, cy - 1, 5.5f, w); c.rect(cx - 3, cy + 3, cx + 3, cy + 7, w); c.circle(cx - 2, cy - 1, 1.5f, Col(30, 10, 40)); c.circle(cx + 2, cy - 1, 1.5f, Col(30, 10, 40)); break;
    case 23: c.arc(cx, cy - 2, 3, 5, -180, 90, w); c.rect(cx - 1, cy + 2, cx + 1.5f, cy + 4, w); c.rect(cx - 1, cy + 5, cx + 1.5f, cy + 7, w); break;
    default: c.circle(cx, cy, 4, w);
  }
}

void artDrawIcon(Canvas& c, const IconRecipe& r) {
  Col a = C(r.c1), b = C(r.c2);
  Col wood = C(0x8b5a2b), steel = C(0xc9d1db), dark = C(0x2b2533), gold = C(0xf5c542), wht = C(0xf4f1ea), red = C(0xd83a3a);
  switch (r.shape) {
    case IS_SWORD: c.line(9, 23, 25, 7, 4, a); c.line(9, 23, 21, 11, 1.2f, mulc(a, 1.25f)); c.line(7, 17, 15, 25, 3, b); c.line(5, 27, 9, 23, 3, wood); c.circle(5, 27, 2, b); break;
    case IS_STAFF: c.line(7, 28, 20, 10, 3, wood); c.circle(22, 8, 6, a); c.circle(20.5f, 6.5f, 2, wht); c.ring(22, 8, 6, 7.5f, b); break;
    case IS_BOW: c.arc(21, 16, 11, 13.5f, 90, 270, wood);
      c.line(21, 3, 21, 29, 1, wht); c.line(5, 16, 27, 16, 1.6f, wood); c.tri({27, 13}, {27, 19}, {31, 16}, steel); c.tri({5, 16}, {9, 12}, {9, 20}, a); break;
    case IS_REVOLVER: c.rect(9, 9, 28, 13, steel); c.circle(13, 14, 4.5f, C(0x8a94a6)); c.poly({{6, 13}, {13, 13}, {12, 26}, {6, 26}}, wood); c.rect(12, 16, 16, 19, dark); break;
    case IS_BONE: c.line(9, 23, 23, 9, 4, wht); c.circle(7, 22, 3, wht); c.circle(10, 25, 3, wht); c.circle(22, 7, 3, wht); c.circle(25, 10, 3, wht); break;
    case IS_AURA: c.ring(16, 16, 12, 15, a); c.ring(16, 16, 7, 9, mulc(a, 1.2f)); c.circle(16, 16, 3.5f, wht); break;
    case IS_AXE: c.line(8, 27, 22, 7, 3, wood); c.poly({{18, 4}, {28, 8}, {29, 18}, {20, 15}}, a); c.line(19, 4, 28, 17, 1.4f, wht); break;
    case IS_SHIELD: c.poly({{6, 5}, {26, 5}, {26, 15}, {16, 28}, {6, 15}}, a); c.rect(14.5f, 7, 17.5f, 24, b); c.rect(8, 11, 24, 14, b); break;
    case IS_BANANA:
      for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        float d1 = sqrtf(sq(x - 14.f) + sq(y - 12.f)), d2 = sqrtf(sq(x - 17.f) + sq(y - 8.f));
        if (d1 < 13 && d2 > 12) c.blend(x, y, a);
      }
      c.circle(4, 14, 1.5f, wood); c.circle(25, 21, 1.5f, wood); break;
    case IS_HOLE: c.circle(16, 16, 12, b); c.ring(16, 16, 9, 11, a); c.ring(16, 16, 5, 6.5f, mulc(a, 1.3f)); c.circle(16, 16, 4, dark); break;
    case IS_BLOODDROP: c.circle(16, 20, 8, a); c.tri({8.5f, 18}, {23.5f, 18}, {16, 3}, a); c.circle(13, 19, 2, wht); break;
    case IS_ROCKS: c.circle(11, 20, 6, a); c.circle(22, 13, 5, mulc(a, 0.85f)); c.circle(21, 24, 4, mulc(a, 1.1f)); c.line(3, 8, 9, 8, 1.2f, wht); c.line(5, 12, 10, 12, 1.2f, wht); break;
    case IS_DICE: c.rect(6, 6, 26, 26, wht); c.circle(11, 11, 2, red); c.circle(21, 11, 2, red); c.circle(16, 16, 2, red); c.circle(11, 21, 2, red); c.circle(21, 21, 2, red); break;
    case IS_FLAMECONE: c.poly({{4, 16}, {28, 5}, {28, 27}}, C(0xd83a3a)); c.poly({{6, 16}, {26, 9}, {26, 23}}, C(0xff8c1a)); c.poly({{9, 16}, {22, 12}, {22, 20}}, C(0xffe066)); break;
    case IS_FIREFOOT: c.poly({{9, 6}, {17, 6}, {17, 19}, {25, 22}, {25, 27}, {9, 27}}, b); c.poly({{2, 28}, {6, 16}, {9, 28}}, C(0xff8c1a)); c.poly({{24, 30}, {28, 18}, {31, 30}}, C(0xffd23f)); break;
    case IS_ICEFOOT: c.poly({{9, 6}, {17, 6}, {17, 19}, {25, 22}, {25, 27}, {9, 27}}, b); for (int i = 0; i < 3; i++) { float a2 = i * PI / 3; c.line(26 - cosf(a2) * 5, 9 - sinf(a2) * 5, 26 + cosf(a2) * 5, 9 + sinf(a2) * 5, 1.5f, a); } break;
    case IS_HEROSWORD: c.line(10, 22, 26, 6, 5, gold); c.line(11, 21, 24, 8, 1.5f, wht); c.line(6, 16, 16, 26, 3.5f, a); c.line(4, 28, 9, 23, 3, b); c.circle(11, 21, 2, C(0x4fc3f7)); break;
    case IS_KATANA: c.line(8, 24, 26, 5, 2.5f, steel); c.line(9, 23, 25, 6, 1, wht); c.circle(8, 24, 3, gold); c.line(3, 29, 8, 24, 3, b); break;
    case IS_BOLT: c.line(6, 28, 17, 13, 3, wood); c.poly({{20, 2}, {13, 14}, {18, 14}, {14, 25}, {26, 10}, {20, 10}, {25, 2}}, a); break;
    case IS_MINE: c.ellipse(16, 20, 11, 7, dark); c.circle(16, 17, 6, C(0x4a4458)); c.circle(16, 15, 2.5f, red); for (int i = 0; i < 5; i++) c.line(6 + i * 5, 22, 6 + i * 5, 27, 1.5f, steel); break;
    case IS_FLASK: c.circle(16, 21, 8, wht); c.circle(16, 21, 6.5f, a); c.rect(13, 5, 19, 14, wht); c.rect(12, 3, 20, 6, wood); c.circle(13, 19, 1.5f, wht); break;
    case IS_SCYTHE: c.line(10, 28, 18, 4, 2.5f, wood); c.arc(8, 16, 9, 13, -100, -20, a); c.arc(8, 16, 11, 13, -100, -20, mulc(a, 1.3f)); break;
    case IS_SHOTGUN: c.rect(6, 11, 29, 14, steel); c.rect(6, 14, 29, 16, C(0x8a94a6)); c.poly({{2, 15}, {12, 14}, {12, 18}, {6, 24}, {2, 22}}, wood); c.rect(14, 16, 20, 19, wood); break;
    case IS_ROCKET: c.line(8, 24, 23, 9, 6, a); c.tri({23, 5}, {27, 5}, {27, 9}, red); c.circle(24, 8, 3.5f, red); c.poly({{5, 21}, {11, 27}, {6, 29}, {3, 24}}, b); c.circle(5, 27, 3, C(0xffb020)); break;
    case IS_SNIPER: c.rect(3, 15, 30, 17, steel); c.rect(10, 11, 20, 14, dark); c.circle(10, 12.5f, 2, C(0x4fc3f7)); c.poly({{1, 16}, {9, 16}, {8, 22}, {1, 21}}, wood); break;
    case IS_NOODLE: for (int x = 3; x < 29; x++) { float y = 16 + sinf(x * 0.45f) * 6; c.circle(x, y, 2.3f, a); } c.circle(4, 16, 3, b); c.circle(28, 16 + sinf(28 * .45f) * 6, 3, b); break;
    case IS_TORNADO: for (int i = 0; i < 6; i++) { float y = 6 + i * 4, rx = 13 - i * 1.8f; c.ellipse(16 + sinf(i) * 2, y, rx, 2.2f, i & 1 ? a : b); } break;
    case IS_DAGGER: c.line(6, 26, 18, 14, 3.5f, steel); c.line(5, 21, 11, 27, 2.5f, gold); c.line(3, 29, 6, 26, 2.5f, wood); for (int i = 0; i < 3; i++) c.arc(19, 13, 4 + i * 4, 5.5f + i * 4, -80, -10, i == 1 ? a : b); break;
    case IS_CORRUPT: c.line(9, 23, 25, 7, 4.5f, a); c.line(13, 19, 17, 17, 1.2f, C(0xff4fd8)); c.line(18, 13, 21, 12, 1.2f, C(0xff4fd8)); c.line(7, 17, 15, 25, 3, b); c.line(4, 28, 9, 23, 3, dark); break;
    case IS_DEXE: c.poly({{12, 4}, {26, 4}, {26, 18}, {14, 20}}, a); c.line(13, 5, 25, 5, 1.2f, wht); c.line(14, 18, 5, 28, 3.5f, wood); c.circle(22, 9, 1.5f, dark); break;
    case IS_BOOK: c.rect(6, 4, 26, 28, a); c.rect(23, 5, 27, 28, wht); c.rect(6, 26, 26, 29, wht); c.rect(6, 4, 8, 28, mulc(a, 0.7f)); drawSymbol(c, r.sym, 16, 15, b); break;
    case IS_RING: c.ring(16, 19, 7, 10, a); c.poly({{12, 9}, {20, 9}, {22, 5}, {16, 1.5f}, {10, 5}}, b); c.circle(14, 5, 1, wht); break;
    case IS_BOTTLE: c.rect(10, 12, 22, 29, a); c.rect(13, 5, 19, 12, a); c.rect(12, 2, 20, 5, wood); c.rect(10, 17, 22, 23, b); c.rect(11, 13, 13, 27, Col(255, 255, 255, 120)); break;
    case IS_MUG: c.ring(22, 18, 3, 6, wht); c.rect(7, 10, 21, 28, C(0xf2b632)); c.ellipse(14, 9, 8.5f, 4, wht); c.circle(9, 12, 2.5f, wht); break;
    case IS_BURGER: c.ellipse(16, 11, 12, 6, C(0xe0a050)); c.rect(4, 15, 28, 18, C(0x4caf50)); c.rect(5, 17, 27, 20, C(0xffd23f)); c.rect(4, 19, 28, 23, C(0x6d3b1f)); c.ellipse(16, 25, 12, 3.5f, C(0xe0a050)); c.circle(12, 9, 1, wht); c.circle(18, 8, 1, wht); break;
    case IS_CLOVER: for (int i = 0; i < 4; i++) { float an = i * PI / 2 + PI / 4; c.circle(16 + cosf(an) * 5.5f, 14 + sinf(an) * 5.5f, 5, a); } c.line(16, 16, 20, 29, 2, mulc(a, 0.7f)); break;
    case IS_CLOAK: c.poly({{12, 4}, {20, 4}, {27, 28}, {5, 28}}, a); c.circle(16, 8, 5, mulc(a, 0.8f)); c.circle(16, 9, 3, dark); break;
    case IS_CARD: c.rect(3, 8, 29, 25, a); c.rect(3, 12, 29, 15, dark); c.rect(7, 18, 12, 22, gold); c.rect(15, 19, 26, 21, wht); break;
    case IS_DOLL: c.circle(16, 9, 6, C(0xc9a26b)); c.rect(10, 14, 22, 26, C(0xc9a26b)); c.line(10, 16, 4, 20, 3, C(0xc9a26b)); c.line(22, 16, 28, 20, 3, C(0xc9a26b)); c.line(12, 25, 11, 30, 3, C(0xc9a26b)); c.line(20, 25, 21, 30, 3, C(0xc9a26b));
      c.line(12, 6, 15, 9, 1, dark); c.line(15, 6, 12, 9, 1, dark); c.line(17, 6, 20, 9, 1, dark); c.line(20, 6, 17, 9, 1, dark); c.line(18, 18, 26, 12, 1, steel); c.circle(26, 12, 1.5f, red); break;
    case IS_HAND: c.rect(9, 14, 23, 26, a); for (int i = 0; i < 4; i++) c.rect(9 + i * 3.6f, 5 + (i == 0 || i == 3 ? 3 : 0), 12 + i * 3.6f, 15, a); c.line(9, 18, 4, 13, 3.5f, a); break;
    case IS_GEM: c.poly({{16, 3}, {27, 13}, {16, 29}, {5, 13}}, a); c.poly({{16, 3}, {16, 29}, {5, 13}}, mulc(a, 1.25f)); c.line(5, 13, 27, 13, 1, wht); break;
    case IS_PLUG: c.rect(10, 10, 22, 22, a); c.rect(12, 4, 14, 10, steel); c.rect(18, 4, 20, 10, steel); c.line(16, 22, 16, 30, 2.5f, dark); c.poly({{24, 12}, {29, 8}, {26, 14}, {31, 13}}, C(0xffe066)); break;
    case IS_CORE: c.circle(16, 16, 8, a); c.ring(16, 16, 10, 12, b); c.circle(14, 14, 3, wht); break;
    case IS_FEATHER: c.poly({{6, 27}, {12, 12}, {24, 3}, {22, 14}}, a); c.line(6, 27, 23, 5, 1.2f, wht); break;
    case IS_GOGGLES: c.rect(3, 13, 29, 16, dark); c.circle(10, 15, 6, a); c.circle(22, 15, 6, a); c.circle(10, 15, 4, b); c.circle(22, 15, 4, b); c.circle(8, 13, 1.5f, wht); c.circle(20, 13, 1.5f, wht); break;
    case IS_MASK: c.ellipse(16, 16, 11, 13, a); c.ellipse(11, 13, 3, 2, dark); c.ellipse(21, 13, 3, 2, dark); c.rect(12, 21, 20, 23, dark); c.line(5, 15, 1, 12, 1.5f, b); c.line(27, 15, 31, 12, 1.5f, b); break;
    case IS_GHOST: c.circle(16, 13, 10, a); c.rect(6, 13, 26, 25, a); for (int i = 0; i < 4; i++) c.circle(8.5f + i * 5, 25, 2.5f, a); c.circle(12, 13, 2, dark); c.circle(20, 13, 2, dark); break;
    case IS_FORK: c.line(16, 30, 16, 14, 3, a); c.rect(9, 10, 23, 14, a); for (int i = 0; i < 4; i++) c.rect(9 + i * 4, 2, 11 + i * 4, 11, a); break;
    case IS_GLOVE: c.rect(8, 12, 24, 26, a); for (int i = 0; i < 3; i++) c.rect(9 + i * 5, 5, 13 + i * 5, 13, a); c.circle(7, 16, 3.5f, a); c.rect(8, 25, 24, 29, b); break;
    case IS_COIN: c.circle(16, 16, 12, a); c.ring(16, 16, 8, 9.5f, b); c.rect(15, 10, 17, 22, b); break;
    case IS_SHOE: c.poly({{5, 10}, {14, 10}, {16, 17}, {28, 20}, {28, 25}, {5, 25}}, a); c.rect(5, 25, 28, 28, wht); c.line(9, 14, 13, 14, 1.2f, wht); c.line(9, 18, 15, 18, 1.2f, wht); break;
    case IS_CUBE: c.poly({{16, 4}, {27, 9}, {16, 14}, {5, 9}}, mulc(a, 1.2f)); c.poly({{5, 9}, {16, 14}, {16, 28}, {5, 22}}, a); c.poly({{27, 9}, {16, 14}, {16, 28}, {27, 22}}, mulc(a, 0.8f)); break;
    case IS_KEY: c.ring(10, 10, 3.5f, 7, a); c.line(14, 14, 27, 27, 3, a); c.line(22, 22, 25, 19, 2.5f, a); c.line(25, 25, 28, 22, 2.5f, a); break;
    case IS_LAMP: c.ellipse(15, 20, 10, 6, a); c.poly({{24, 18}, {31, 13}, {30, 17}, {25, 22}}, a); c.ring(5, 19, 2, 4, a); c.rect(12, 11, 18, 15, a); c.circle(15, 10, 2.5f, a); c.circle(29, 9, 2, Col(255, 255, 255, 160)); break;
    case IS_CHEESE: c.poly({{3, 24}, {29, 24}, {29, 13}, {3, 20}}, C(0xffd23f)); c.poly({{3, 20}, {29, 13}, {20, 8}}, C(0xffe68a)); c.circle(10, 21, 2, C(0xc9a227)); c.circle(21, 19, 1.5f, C(0xc9a227)); c.circle(25, 21, 2.5f, a); c.circle(14, 22, 1.5f, a); break;
    case IS_MIRROR: c.ellipse(16, 12, 9, 10, C(0xc0a060)); c.ellipse(16, 12, 7, 8, a); c.line(13, 8, 16, 6, 1.5f, wht); c.line(16, 21, 16, 30, 3.5f, C(0xc0a060)); break;
    case IS_SCARF: c.poly({{5, 8}, {27, 8}, {27, 14}, {5, 14}}, a); c.poly({{18, 13}, {24, 13}, {26, 29}, {20, 29}}, a); c.rect(5, 10, 27, 12, b); c.rect(20, 22, 26, 24, b); break;
    case IS_SKULL: c.circle(16, 13, 10, wht); c.rect(10, 19, 22, 27, wht); c.circle(12, 13, 3, dark); c.circle(20, 13, 3, dark); c.tri({16, 16}, {14, 20}, {18, 20}, dark); c.rect(12, 24, 13, 27, dark); c.rect(15.5f, 24, 16.5f, 27, dark); c.rect(19, 24, 20, 27, dark); break;
    case IS_MAGNET: c.arc(16, 14, 5, 11, -180, 0, a);
      c.rect(5, 14, 11, 24, a); c.rect(21, 14, 27, 24, a); c.rect(5, 22, 11, 27, steel); c.rect(21, 22, 27, 27, steel); break;
    case IS_GLASSES: c.rect(3, 12, 29, 14, dark); c.poly({{4, 13}, {14, 13}, {13, 20}, {6, 20}}, a); c.poly({{18, 13}, {28, 13}, {26, 20}, {19, 20}}, a); c.line(6, 15, 9, 15, 1, wht); break;
    case IS_BRACELET: c.ring(16, 16, 9, 12, a); c.circle(16, 6, 5, wht); c.line(16, 6, 16, 3.5f, 1, dark); c.line(16, 6, 18, 6, 1, dark); break;
    case IS_BARREL: c.rect(8, 5, 24, 28, a); c.rect(8, 9, 24, 11, mulc(a, 0.6f)); c.rect(8, 22, 24, 24, mulc(a, 0.6f)); c.circle(16, 16.5f, 4, C(0x1b1b1b)); c.circle(16, 16.5f, 1.5f, a); break;
    case IS_SKATE: c.poly({{6, 6}, {15, 6}, {15, 15}, {26, 17}, {26, 22}, {6, 22}}, a); c.rect(5, 22, 27, 24, dark); c.circle(9, 27, 3, b); c.circle(22, 27, 3, b); break;
    case IS_CLOCK: c.circle(16, 17, 11, a); c.circle(16, 17, 8.5f, wht); c.line(16, 17, 16, 11, 1.5f, dark); c.line(16, 17, 20, 19, 1.5f, dark); c.circle(8, 6, 3, a); c.circle(24, 6, 3, a); break;
    case IS_POT: c.rect(6, 12, 26, 26, a); c.ellipse(16, 12, 10, 3, mulc(a, 1.2f)); c.rect(2, 14, 6, 17, a); c.rect(26, 14, 30, 17, a); c.rect(9, 15, 11, 23, wht); break;
    case IS_HAT: c.tri({16, 2}, {7, 24}, {25, 24}, a); c.ellipse(16, 25, 13, 3.5f, a); c.rect(8, 20, 24, 23, b); drawSymbol(c, 18, 15, 13, gold); break;
    case IS_SNAKE: for (int i = 0; i < 26; i++) { float t = i / 25.f; float x = 6 + t * 20, y = 16 + sinf(t * TAU * 1.2f) * 7; c.circle(x, y, 2.8f, a); } c.circle(26, 9, 3.5f, a); c.circle(27, 8, 1, dark); break;
    case IS_PUMPKIN: c.ellipse(16, 18, 12, 10, a); c.ellipse(16, 18, 4, 10, mulc(a, 1.15f)); c.rect(14, 5, 18, 10, C(0x4caf50)); c.tri({10, 15}, {14, 15}, {12, 12}, dark); c.tri({18, 15}, {22, 15}, {20, 12}, dark); c.rect(10, 21, 22, 23, dark); break;
    case IS_CAMPFIRE: c.line(5, 27, 27, 21, 3.5f, wood); c.line(5, 21, 27, 27, 3.5f, wood); c.poly({{10, 22}, {16, 4}, {22, 22}}, C(0xff8c1a)); c.poly({{13, 22}, {16, 11}, {19, 22}}, C(0xffe066)); break;
    case IS_CACTUS: c.rect(13, 6, 19, 29, a); c.rect(6, 13, 10, 21, a); c.rect(6, 19, 13, 22, a); c.rect(22, 10, 26, 18, a); c.rect(19, 16, 26, 19, a); c.circle(16, 6, 3, a); c.circle(17, 4, 1.5f, C(0xff5fa2)); break;
    case IS_WRENCH: c.line(9, 23, 21, 11, 4, a); c.arc(23, 9, 2.5f, 6.5f, 0, 270, a); c.circle(8, 24, 3.5f, a); break;
    case IS_BEACON: c.poly({{12, 28}, {20, 28}, {18, 12}, {14, 12}}, a); c.rect(12, 8, 20, 12, gold); c.tri({12, 8}, {20, 8}, {16, 3}, red); c.line(4, 9, 10, 10, 1.5f, C(0xffe066)); c.line(22, 10, 28, 9, 1.5f, C(0xffe066)); break;
    case IS_ANVIL: c.poly({{3, 9}, {27, 9}, {27, 14}, {21, 16}, {21, 22}, {11, 22}, {11, 16}, {6, 14}}, a); c.rect(8, 22, 24, 27, a); c.line(4, 10, 26, 10, 1, C(0x9aa3b2)); break;
    case IS_HAMMER: c.line(8, 28, 18, 12, 3, wood); c.poly({{12, 4}, {28, 12}, {24, 20}, {8, 12}}, a); c.line(13, 6, 27, 13, 1.2f, wht); break;
    case IS_SOUL: c.circle(16, 20, 7, a); c.tri({9.5f, 18}, {22.5f, 18}, {18, 3}, a); c.circle(14, 19, 2, wht); c.circle(19, 19, 2, wht); break;
    case IS_LEG: c.circle(16, 10, 7, wht); c.circle(13, 10, 2, dark); c.circle(19, 10, 2, dark); c.rect(14.5f, 16, 17.5f, 26, wht); c.rect(14.5f, 25, 23, 28, wht); break;
    case IS_KNUCKLES: for (int i = 0; i < 4; i++) c.ring(7 + i * 6, 12, 2, 4, a); c.rect(4, 15, 28, 21, a); c.rect(6, 21, 26, 24, mulc(a, 0.8f)); break;
    case IS_CLEAVER: c.rect(9, 5, 27, 18, steel); c.rect(4, 7, 9, 11, wood); c.circle(23, 8, 1.5f, dark); c.circle(13, 18, 2, red); c.rect(12, 18, 14, 25, red); c.circle(20, 18, 1.5f, red); break;
    case IS_BOB: c.rect(9, 8, 23, 28, C(0x9aa3b2)); c.circle(16, 9, 7, C(0x9aa3b2)); c.rect(12, 12, 20, 13, dark); c.rect(15.5f, 10, 16.5f, 18, dark); c.rect(5, 27, 27, 30, C(0x6d4c2a)); break;
    case IS_HEART: drawSymbol(c, 6, 16, 15, a); c.circle(12, 12, 1.5f, wht); break;
    case IS_STAR: drawSymbol(c, 18, 16, 16, a); break;
    case IS_SHARD: c.poly({{14, 3}, {25, 18}, {17, 29}, {7, 15}}, a); c.line(14, 3, 17, 29, 1, wht); break;
    case IS_CHEST: c.rect(4, 13, 28, 27, wood); c.ellipse(16, 13, 12, 6, mulc(wood, 1.15f)); c.rect(4, 13, 28, 15, gold); c.rect(14, 12, 18, 19, gold); c.rect(4, 25, 28, 27, gold); break;
    case IS_OATS: c.poly({{8, 8}, {24, 8}, {26, 28}, {6, 28}}, C(0xd9b26f)); c.rect(9, 14, 23, 21, wht); c.circle(16, 17.5f, 2.5f, a); c.ellipse(16, 8, 8, 2.5f, C(0xb08a4a)); break;
    case IS_MEDKIT: c.rect(5, 9, 27, 27, wht); c.rect(12, 5, 20, 9, wht); c.rect(14, 12, 18, 24, red); c.rect(10, 16, 22, 20, red); break;
    case IS_CANNON: c.circle(10, 22, 6, wood); c.line(10, 20, 26, 10, 7, a); c.circle(27, 9, 2.5f, dark); c.circle(28, 5, 2, C(0xff5fa2)); break;
    case IS_SHROUD: c.poly({{9, 4}, {23, 4}, {28, 29}, {22, 25}, {16, 30}, {10, 25}, {4, 29}}, a); c.circle(13, 12, 1.8f, wht); c.circle(19, 12, 1.8f, wht); break;
    case IS_LIGHTORB: c.circle(16, 16, 11, a); c.poly({{18, 6}, {11, 17}, {16, 17}, {13, 27}, {22, 14}, {17, 14}, {21, 6}}, C(0xffe066)); break;
    case IS_BLADE: for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) { float d1 = sqrtf(sq(x - 6.f) + sq(y - 26.f)), d2 = sqrtf(sq(x - 3.f) + sq(y - 30.f)); if (d1 < 22 && d2 > 21 && x > 6 && y < 26) c.blend(x, y, a); } c.line(4, 28, 9, 23, 3.5f, dark); break;
    case IS_EAGLE: for (int i = 0; i < 3; i++) { c.line(10 + i * 6, 6, 9 + i * 6, 20, 3.5f, C(0xd9b26f)); c.tri({7 + i * 6.f, 20}, {12 + i * 6.f, 20}, {9 + i * 6.f, 28}, a); } break;
    case IS_TONIC: c.rect(9, 12, 23, 29, a); c.rect(13, 4, 19, 12, a); c.rect(12, 2, 20, 5, C(0xd8d0c0)); c.rect(10, 17, 22, 24, wht); c.line(12, 18, 20, 23, 1, dark); c.line(20, 18, 12, 23, 1, dark); break;
    case IS_SPIKYSHIELD: for (int i = 0; i < 8; i++) { float an = i * PI / 4; c.tri({16 + cosf(an - .25f) * 10, 16 + sinf(an - .25f) * 10}, {16 + cosf(an + .25f) * 10, 16 + sinf(an + .25f) * 10}, {16 + cosf(an) * 15, 16 + sinf(an) * 15}, steel); } c.circle(16, 16, 10, a); c.circle(16, 16, 3, gold); break;
    case IS_BAG: c.rect(8, 10, 24, 29, a); c.ellipse(16, 10, 8, 4, a); c.rect(10, 18, 22, 25, mulc(a, 0.75f)); c.ring(16, 7, 3, 4.5f, b); break;
    case IS_GOLDKEY: c.ring(10, 10, 3.5f, 7.5f, gold); c.circle(10, 10, 1.5f, red); c.line(14, 14, 28, 28, 3.5f, gold); c.line(22, 22, 25, 19, 3, gold); c.line(26, 26, 29, 23, 3, gold); break;
    case IS_NUKE: c.circle(15, 18, 10, dark); c.circle(12, 15, 2.5f, C(0x5a5470)); c.line(21, 10, 26, 5, 2, wood); c.circle(27, 4, 2.5f, C(0xffb020)); break;
    case IS_RAGE: c.circle(16, 16, 12, red); c.line(9, 10, 14, 13, 2, dark); c.line(23, 10, 18, 13, 2, dark); c.circle(11, 15, 1.5f, dark); c.circle(21, 15, 1.5f, dark); c.rect(10, 21, 22, 24, dark); break;
    case IS_SHIELDPU: c.circle(16, 16, 12, Col(80, 200, 255, 160)); c.ring(16, 16, 10.5f, 12.5f, C(0x9be7ff)); c.circle(11, 11, 2.5f, wht); break;
    case IS_SILVER: c.circle(16, 16, 11, C(0xd8dde6)); c.ring(16, 16, 7, 8.5f, C(0x8a94a6)); c.circle(13, 12, 1.5f, wht); break;
    case IS_PAUSE: c.rect(9, 7, 14, 25, a); c.rect(18, 7, 23, 25, a); break;
    case IS_MAP: c.poly({{4, 8}, {12, 5}, {20, 8}, {28, 5}, {28, 24}, {20, 27}, {12, 24}, {4, 27}}, C(0xe8d5a3)); c.line(12, 5, 12, 24, 1, C(0xb59a62)); c.line(20, 8, 20, 27, 1, C(0xb59a62)); c.line(7, 20, 16, 13, 1.2f, red); c.circle(23, 12, 2, red); break;
    case IS_JUMP: c.tri({16, 3}, {5, 15}, {27, 15}, a); c.rect(12, 15, 20, 24, a); c.rect(8, 26, 24, 29, a); break;
    case IS_SLIDE: c.tri({16, 29}, {5, 17}, {27, 17}, a); c.rect(12, 8, 20, 17, a); c.rect(8, 3, 24, 6, a); break;
    case IS_INTERACT: c.rect(10, 14, 22, 27, a); c.rect(14, 4, 17.5f, 15, a); for (int i = 0; i < 2; i++) c.rect(18 + i * 3.5f, 10, 21 + i * 3.5f, 16, a); c.line(10, 18, 6, 14, 3, a); break;
    case IS_GEAR: for (int i = 0; i < 8; i++) { float an = i * PI / 4; c.line(16, 16, 16 + cosf(an) * 13, 16 + sinf(an) * 13, 4.5f, a); } c.circle(16, 16, 9.5f, a); c.erase(13, 13, 19, 19); break;
    case IS_PORTAL: c.rect(5, 8, 10, 29, C(0x8a8a99)); c.rect(22, 8, 27, 29, C(0x8a8a99)); c.ellipse(16, 8, 11, 6, C(0x8a8a99)); c.ellipse(16, 17, 6, 11, a); c.ellipse(16, 17, 3, 7, b); break;
    case IS_SHRINE: c.rect(11, 12, 21, 29, C(0x9aa3b2)); c.rect(9, 26, 23, 30, C(0x7a8292)); c.poly({{16, 1}, {21, 7}, {16, 13}, {11, 7}}, a); c.circle(15, 6, 1, wht); break;
    case IS_CROWN: c.poly({{4, 24}, {4, 9}, {10, 16}, {16, 6}, {22, 16}, {28, 9}, {28, 24}}, gold); c.circle(16, 18, 2, red); break;
    case IS_ARROWUP: c.tri({16, 4}, {5, 17}, {27, 17}, a); c.rect(12, 17, 20, 28, a); break;
    case IS_REROLL: c.arc(16, 16, 7, 10.5f, -60, 240, a); c.tri({17, 1}, {17, 13}, {24, 7}, a); break;
    case IS_SKIP: c.tri({5, 7}, {5, 25}, {16, 16}, a); c.tri({16, 7}, {16, 25}, {27, 16}, a); break;
    case IS_BANISH: c.ring(16, 16, 9, 12.5f, red); c.line(8, 8, 24, 24, 3.5f, red); break;
    case IS_TROPHY: c.poly({{8, 4}, {24, 4}, {22, 15}, {16, 19}, {10, 15}}, gold); c.ring(7, 9, 2, 4, gold); c.ring(25, 9, 2, 4, gold); c.rect(14, 18, 18, 24, gold); c.rect(9, 24, 23, 28, wood); break;
    case IS_LOCK: c.ring(16, 12, 4.5f, 7.5f, steel); c.rect(7, 13, 25, 28, gold); c.circle(16, 19, 2, dark); c.rect(15, 19, 17, 24, dark); break;
    case IS_FOX: c.tri({6, 4}, {13, 12}, {6, 14}, C(0xf08a24)); c.tri({26, 4}, {19, 12}, {26, 14}, C(0xf08a24)); c.circle(16, 17, 10, C(0xf08a24)); c.poly({{8, 20}, {24, 20}, {16, 29}}, wht); c.circle(16, 26, 2, dark); c.circle(12, 16, 1.8f, dark); c.circle(20, 16, 1.8f, dark); break;
    case IS_NINJA: c.circle(16, 16, 12, C(0x23202b)); c.rect(5, 12, 27, 19, C(0xe8c39e)); c.rect(9, 14, 13, 16, dark); c.rect(19, 14, 23, 16, dark); c.rect(4, 8, 28, 11, C(0xd83a3a)); c.line(26, 9, 31, 14, 2, C(0xd83a3a)); break;
    case IS_SKELETON: artDrawIcon(c, IconRecipe{IS_SKULL, 0, 0, 0}); c.rect(4, 3, 28, 6, C(0x4fc3f7)); return;
    case IS_MONKE: c.circle(6, 15, 4.5f, C(0x8b5a2b)); c.circle(26, 15, 4.5f, C(0x8b5a2b)); c.circle(16, 15, 11, C(0x8b5a2b)); c.ellipse(16, 18, 8, 7, C(0xe6c08f)); c.circle(12, 13, 1.8f, dark); c.circle(20, 13, 1.8f, dark); c.rect(12, 21, 20, 22.5f, dark); break;
    case IS_OGRE: c.circle(16, 17, 12, C(0x7aa34a)); c.tri({9, 26}, {12, 26}, {10.5f, 21}, wht); c.tri({20, 26}, {23, 26}, {21.5f, 21}, wht); c.circle(11, 14, 2, dark); c.circle(21, 14, 2, dark); c.rect(8, 10, 24, 12, C(0x4a6b2a)); break;
    case IS_RACCOON: c.circle(8, 8, 4.5f, C(0x8a8f99)); c.circle(24, 8, 4.5f, C(0x8a8f99)); c.circle(8, 8, 2, dark); c.circle(24, 8, 2, dark);
      c.circle(16, 17, 11, C(0x9aa0aa)); c.ellipse(16, 15, 11, 4, C(0x2a2a33)); c.circle(11.5f, 15, 2, wht); c.circle(20.5f, 15, 2, wht); c.circle(11.5f, 15, 1, dark); c.circle(20.5f, 15, 1, dark);
      c.ellipse(16, 22.5f, 6, 4.5f, C(0xe8e4dc)); c.circle(16, 20.5f, 1.8f, dark); break;
    case IS_STUMP: c.rect(7, 12, 25, 29, C(0x8a5e38)); c.line(11, 14, 11, 28, 1, C(0x6a4428)); c.line(21, 14, 21, 28, 1, C(0x6a4428)); c.ellipse(16, 12, 9, 4, C(0xd9b07a)); c.ring(16, 12, 2.5f, 3.6f, C(0xb08a5a));
      c.ellipse(10, 10.5f, 5, 2.4f, C(0x5aa83a)); c.line(18, 10, 20, 3, 1.5f, C(0x4caf50)); c.ellipse(23, 4, 3.5f, 2, C(0x7bd23f)); c.circle(12, 19, 1.8f, dark); c.circle(20, 19, 1.8f, dark); c.rect(13, 23, 19, 24.5f, dark); break;
    case IS_GECKO: c.ellipse(16, 21, 12, 8, C(0x6cc04a)); c.circle(9, 17, 4.2f, C(0x6cc04a)); c.circle(23, 17, 4.2f, C(0x6cc04a)); c.circle(9, 17, 2.8f, C(0xffd23f)); c.circle(23, 17, 2.8f, C(0xffd23f));
      c.rect(8.4f, 15, 9.6f, 19, dark); c.rect(22.4f, 15, 23.6f, 19, dark); c.line(10, 25, 22, 25, 1.2f, C(0x2e6a22));
      c.rect(10, 8, 22, 12.5f, wht); c.circle(11.5f, 7, 3.5f, wht); c.circle(16, 5, 4.5f, wht); c.circle(20.5f, 7, 3.5f, wht); break;
    case IS_XP: c.poly({{16, 3}, {25, 16}, {16, 29}, {7, 16}}, a); c.poly({{16, 3}, {16, 29}, {7, 16}}, mulc(a, 1.3f)); break;
    case IS_HOURGLASS: c.rect(7, 3, 25, 6, wood); c.rect(7, 26, 25, 29, wood); c.tri({9, 6}, {23, 6}, {16, 16}, Col(220, 230, 255, 200)); c.tri({9, 26}, {23, 26}, {16, 16}, Col(220, 230, 255, 200)); c.tri({12, 26}, {20, 26}, {16, 20}, a); break;
    default: c.circle(16, 16, 10, a); break;
  }
  if (r.shape != IS_SKELETON) { c.shadeEdges(0.22f); c.outline(Col(18, 14, 24, 255)); }
}

// =====================================================================
//  Pixel font (5x8 body + 2 accent rows above). Rows top->bottom.
// =====================================================================
struct GlyphDef { int cp; const char* rows; };
// rows: 8 rows of 5 chars separated by '|'; '#' = ink
static const GlyphDef kFont[] = {
  {'0', ".###.|#...#|#..##|#.#.#|##..#|#...#|.###.|....."}, {'1', "..#..|.##..|..#..|..#..|..#..|..#..|.###.|....."},
  {'2', ".###.|#...#|....#|...#.|..#..|.#...|#####|....."}, {'3', "####.|....#|....#|.###.|....#|....#|####.|....."},
  {'4', "...#.|..##.|.#.#.|#..#.|#####|...#.|...#.|....."}, {'5', "#####|#....|####.|....#|....#|#...#|.###.|....."},
  {'6', "..##.|.#...|#....|####.|#...#|#...#|.###.|....."}, {'7', "#####|....#|...#.|..#..|.#...|.#...|.#...|....."},
  {'8', ".###.|#...#|#...#|.###.|#...#|#...#|.###.|....."}, {'9', ".###.|#...#|#...#|.####|....#|...#.|.##..|....."},
  {'A', ".###.|#...#|#...#|#####|#...#|#...#|#...#|....."}, {'B', "####.|#...#|#...#|####.|#...#|#...#|####.|....."},
  {'C', ".###.|#...#|#....|#....|#....|#...#|.###.|....."}, {'D', "####.|#...#|#...#|#...#|#...#|#...#|####.|....."},
  {'E', "#####|#....|#....|####.|#....|#....|#####|....."}, {'F', "#####|#....|#....|####.|#....|#....|#....|....."},
  {'G', ".###.|#...#|#....|#.###|#...#|#...#|.####|....."}, {'H', "#...#|#...#|#...#|#####|#...#|#...#|#...#|....."},
  {'I', ".###.|..#..|..#..|..#..|..#..|..#..|.###.|....."}, {'J', "..###|...#.|...#.|...#.|...#.|#..#.|.##..|....."},
  {'K', "#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#|....."}, {'L', "#....|#....|#....|#....|#....|#....|#####|....."},
  {'M', "#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#|....."}, {'N', "#...#|#...#|##..#|#.#.#|#..##|#...#|#...#|....."},
  {'O', ".###.|#...#|#...#|#...#|#...#|#...#|.###.|....."}, {'P', "####.|#...#|#...#|####.|#....|#....|#....|....."},
  {'Q', ".###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#|....."}, {'R', "####.|#...#|#...#|####.|#.#..|#..#.|#...#|....."},
  {'S', ".####|#....|#....|.###.|....#|....#|####.|....."}, {'T', "#####|..#..|..#..|..#..|..#..|..#..|..#..|....."},
  {'U', "#...#|#...#|#...#|#...#|#...#|#...#|.###.|....."}, {'V', "#...#|#...#|#...#|#...#|#...#|.#.#.|..#..|....."},
  {'W', "#...#|#...#|#...#|#.#.#|#.#.#|#.#.#|.#.#.|....."}, {'X', "#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#|....."},
  {'Y', "#...#|#...#|.#.#.|..#..|..#..|..#..|..#..|....."}, {'Z', "#####|....#|...#.|..#..|.#...|#....|#####|....."},
  {'a', ".....|.....|.###.|....#|.####|#...#|.####|....."}, {'b', "#....|#....|####.|#...#|#...#|#...#|####.|....."},
  {'c', ".....|.....|.###.|#....|#....|#...#|.###.|....."}, {'d', "....#|....#|.####|#...#|#...#|#...#|.####|....."},
  {'e', ".....|.....|.###.|#...#|#####|#....|.###.|....."}, {'f', "..##.|.#..#|.#...|###..|.#...|.#...|.#...|....."},
  {'g', ".....|.....|.####|#...#|#...#|.####|....#|.###."}, {'h', "#....|#....|#.##.|##..#|#...#|#...#|#...#|....."},
  {'i', "..#..|.....|.##..|..#..|..#..|..#..|.###.|....."}, {'j', "...#.|.....|..##.|...#.|...#.|...#.|#..#.|.##.."},
  {'k', "#....|#....|#..#.|#.#..|##...|#.#..|#..#.|....."}, {'l', ".##..|..#..|..#..|..#..|..#..|..#..|.###.|....."},
  {'m', ".....|.....|##.#.|#.#.#|#.#.#|#.#.#|#.#.#|....."}, {'n', ".....|.....|#.##.|##..#|#...#|#...#|#...#|....."},
  {'o', ".....|.....|.###.|#...#|#...#|#...#|.###.|....."}, {'p', ".....|.....|####.|#...#|#...#|####.|#....|#...."},
  {'q', ".....|.....|.####|#...#|#...#|.####|....#|....#"}, {'r', ".....|.....|#.##.|##..#|#....|#....|#....|....."},
  {'s', ".....|.....|.####|#....|.###.|....#|####.|....."}, {'t', ".#...|.#...|###..|.#...|.#...|.#..#|..##.|....."},
  {'u', ".....|.....|#...#|#...#|#...#|#..##|.##.#|....."}, {'v', ".....|.....|#...#|#...#|#...#|.#.#.|..#..|....."},
  {'w', ".....|.....|#...#|#...#|#.#.#|#.#.#|.#.#.|....."}, {'x', ".....|.....|#...#|.#.#.|..#..|.#.#.|#...#|....."},
  {'y', ".....|.....|#...#|#...#|#...#|.####|....#|.###."}, {'z', ".....|.....|#####|...#.|..#..|.#...|#####|....."},
  {' ', ".....|.....|.....|.....|.....|.....|.....|....."}, {'!', "..#..|..#..|..#..|..#..|..#..|.....|..#..|....."},
  {'"', ".#.#.|.#.#.|.....|.....|.....|.....|.....|....."}, {'#', ".#.#.|#####|.#.#.|.#.#.|#####|.#.#.|.....|....."},
  {'$', "..#..|.####|#.#..|.###.|..#.#|####.|..#..|....."}, {'%', "##...|##..#|...#.|..#..|.#...|#..##|...##|....."},
  {'&', ".##..|#..#.|#.#..|.#...|#.#.#|#..#.|.##.#|....."}, {'\'', "..#..|..#..|.....|.....|.....|.....|.....|....."},
  {'(', "...#.|..#..|.#...|.#...|.#...|..#..|...#.|....."}, {')', ".#...|..#..|...#.|...#.|...#.|..#..|.#...|....."},
  {'*', ".....|..#..|#.#.#|.###.|#.#.#|..#..|.....|....."}, {'+', ".....|..#..|..#..|#####|..#..|..#..|.....|....."},
  {',', ".....|.....|.....|.....|.....|.##..|..#..|.#..."}, {'-', ".....|.....|.....|.###.|.....|.....|.....|....."},
  {'.', ".....|.....|.....|.....|.....|.##..|.##..|....."}, {'/', "....#|...#.|...#.|..#..|.#...|.#...|#....|....."},
  {':', ".....|.##..|.##..|.....|.##..|.##..|.....|....."}, {';', ".....|.##..|.##..|.....|.##..|..#..|.#...|....."},
  {'<', "...#.|..#..|.#...|#....|.#...|..#..|...#.|....."}, {'=', ".....|.....|#####|.....|#####|.....|.....|....."},
  {'>', ".#...|..#..|...#.|....#|...#.|..#..|.#...|....."}, {'?', ".###.|#...#|....#|...#.|..#..|.....|..#..|....."},
  {'@', ".###.|#...#|#.###|#.#.#|#.###|#....|.###.|....."}, {'[', ".###.|.#...|.#...|.#...|.#...|.#...|.###.|....."},
  {'\\', "#....|.#...|.#...|..#..|...#.|...#.|....#|....."}, {']', ".###.|...#.|...#.|...#.|...#.|...#.|.###.|....."},
  {'^', "..#..|.#.#.|#...#|.....|.....|.....|.....|....."}, {'_', ".....|.....|.....|.....|.....|.....|#####|....."},
  {'`', ".#...|..#..|.....|.....|.....|.....|.....|....."}, {'{', "...##|..#..|..#..|.#...|..#..|..#..|...##|....."},
  {'|', "..#..|..#..|..#..|..#..|..#..|..#..|..#..|....."}, {'}', "##...|..#..|..#..|...#.|..#..|..#..|##...|....."},
  {'~', ".....|.....|.#..#|#.##.|.....|.....|.....|....."},
  {0xD7, ".....|#...#|.#.#.|..#..|.#.#.|#...#|.....|....."}, // ×
  {0xB0, ".##..|#..#.|.##..|.....|.....|.....|.....|....."}, // °
  {0x95, ".....|.....|.....|.##..|.##..|.....|.....|....."}, // bullet (mapped from U+2022)
  {0xDF, ".##..|#..#.|#.##.|#..#.|#..#.|#.##.|#....|#...."}, // ss (sharp s)
  {0xBF, "..#..|.....|..#..|.#...|#....|#...#|.###.|....."}, // inverted ?
  {0xA1, "..#..|.....|..#..|..#..|..#..|..#..|..#..|....."}, // inverted !
  {0xAB, ".....|..#.#|.#.#.|#.#..|.#.#.|..#.#|.....|....."}, // <<
  {0xBB, ".....|#.#..|.#.#.|..#.#|.#.#.|#.#..|.....|....."}, // >>
  // ---- Cyrillic (letters that look like Latin ones are aliased in cyrAlias below)
  {0x411, "#####|#....|#....|####.|#...#|#...#|####.|....."}, // B
  {0x413, "#####|#....|#....|#....|#....|#....|#....|....."}, // G
  {0x414, ".###.|.#.#.|.#.#.|.#.#.|.#.#.|#####|#...#|....."}, // D
  {0x416, "#.#.#|#.#.#|.###.|..#..|.###.|#.#.#|#.#.#|....."}, // Zh
  {0x417, "####.|....#|....#|.###.|....#|....#|####.|....."}, // Z
  {0x418, "#...#|#...#|#..##|#.#.#|##..#|#...#|#...#|....."}, // I
  {0x41B, "..###|.#..#|.#..#|.#..#|.#..#|.#..#|#...#|....."}, // L
  {0x41F, "#####|#...#|#...#|#...#|#...#|#...#|#...#|....."}, // P
  {0x423, "#...#|#...#|.#.#.|..#..|..#..|.#...|#....|....."}, // U
  {0x424, "..#..|.###.|#.#.#|#.#.#|.###.|..#..|..#..|....."}, // F
  {0x426, "#..#.|#..#.|#..#.|#..#.|#..#.|#####|....#|....."}, // Ts
  {0x427, "#...#|#...#|#...#|.####|....#|....#|....#|....."}, // Ch
  {0x428, "#.#.#|#.#.#|#.#.#|#.#.#|#.#.#|#.#.#|#####|....."}, // Sh
  {0x429, "#.#.#|#.#.#|#.#.#|#.#.#|#.#.#|#####|....#|....."}, // Shch
  {0x42A, "##...|.#...|.#...|.###.|.#..#|.#..#|.###.|....."}, // hard sign
  {0x42B, "#...#|#...#|#...#|##..#|#.#.#|#.#.#|##..#|....."}, // Y
  {0x42C, "#....|#....|#....|####.|#...#|#...#|####.|....."}, // soft sign
  {0x42D, ".###.|#...#|....#|..###|....#|#...#|.###.|....."}, // E
  {0x42E, "#..#.|#.#.#|#.#.#|###.#|#.#.#|#.#.#|#..#.|....."}, // Yu
  {0x42F, ".####|#...#|#...#|.####|..#.#|.#..#|#...#|....."}, // Ya
  {0x431, ".##..|#....|####.|#...#|#...#|#...#|.###.|....."}, // b
  {0x432, ".....|.....|####.|#...#|####.|#...#|####.|....."}, // v
  {0x433, ".....|.....|####.|#....|#....|#....|#....|....."}, // g
  {0x434, ".....|.....|.###.|.#.#.|.#.#.|#####|#...#|....."}, // d
  {0x436, ".....|.....|#.#.#|.###.|..#..|.###.|#.#.#|....."}, // zh
  {0x437, ".....|.....|####.|....#|.###.|....#|####.|....."}, // z
  {0x438, ".....|.....|#...#|#..##|#.#.#|##..#|#...#|....."}, // i
  {0x43A, ".....|.....|#..#.|#.#..|##...|#.#..|#..#.|....."}, // k
  {0x43B, ".....|.....|..###|.#..#|.#..#|.#..#|#...#|....."}, // l
  {0x43C, ".....|.....|#...#|##.##|#.#.#|#...#|#...#|....."}, // m
  {0x43D, ".....|.....|#...#|#...#|#####|#...#|#...#|....."}, // n
  {0x43F, ".....|.....|#####|#...#|#...#|#...#|#...#|....."}, // p
  {0x442, ".....|.....|#####|..#..|..#..|..#..|..#..|....."}, // t
  {0x444, "..#..|..#..|.###.|#.#.#|#.#.#|.###.|..#..|..#.."}, // f
  {0x446, ".....|.....|#..#.|#..#.|#..#.|#..#.|#####|....#"}, // ts
  {0x447, ".....|.....|#...#|#...#|.####|....#|....#|....."}, // ch
  {0x448, ".....|.....|#.#.#|#.#.#|#.#.#|#.#.#|#####|....."}, // sh
  {0x449, ".....|.....|#.#.#|#.#.#|#.#.#|#.#.#|#####|....#"}, // shch
  {0x44A, ".....|.....|##...|.#...|.###.|.#..#|.###.|....."}, // hard sign
  {0x44B, ".....|.....|#...#|#...#|##..#|#.#.#|##..#|....."}, // y
  {0x44C, ".....|.....|#....|#....|####.|#...#|####.|....."}, // soft sign
  {0x44D, ".....|.....|####.|....#|..###|....#|####.|....."}, // e
  {0x44E, ".....|.....|#..#.|#.#.#|###.#|#.#.#|#..#.|....."}, // yu
  {0x44F, ".....|.....|.####|#...#|.####|.#..#|#...#|....."}, // ya
};
// Cyrillic letters drawn with the matching Latin glyph
static int cyrAlias(int cp) {
  switch (cp) {
    case 0x410: return 'A'; case 0x412: return 'B'; case 0x415: return 'E'; case 0x41A: return 'K'; case 0x41C: return 'M';
    case 0x41D: return 'H'; case 0x41E: return 'O'; case 0x420: return 'P'; case 0x421: return 'C'; case 0x422: return 'T'; case 0x425: return 'X';
    case 0x430: return 'a'; case 0x435: return 'e'; case 0x43E: return 'o'; case 0x440: return 'p'; case 0x441: return 'c'; case 0x443: return 'y'; case 0x445: return 'x';
  }
  return cp;
}
static const char* kAccent[] = {
  "...#.|..#..", // acute
  ".#...|..#..", // grave
  "..#..|.#.#.", // circumflex
  ".##.#|#..#.", // tilde
  ".#.#.|.....", // diaeresis
  ".#.#.|..#..", // breve
};
struct AccentDef { int cp; int base; int acc; };
static const AccentDef kAccented[] = {
  {0xC0, 'A', 1}, {0xC1, 'A', 0}, {0xC2, 'A', 2}, {0xC3, 'A', 3}, {0xC9, 'E', 0}, {0xCA, 'E', 2}, {0xCD, 'I', 0},
  {0xD3, 'O', 0}, {0xD4, 'O', 2}, {0xD5, 'O', 3}, {0xDA, 'U', 0}, {0xDC, 'U', 4},
  {0xE0, 'a', 1}, {0xE1, 'a', 0}, {0xE2, 'a', 2}, {0xE3, 'a', 3}, {0xE9, 'e', 0}, {0xEA, 'e', 2}, {0xED, 'i', 0},
  {0xF3, 'o', 0}, {0xF4, 'o', 2}, {0xF5, 'o', 3}, {0xFA, 'u', 0}, {0xFC, 'u', 4},
  // German / Spanish / French extras
  {0xC4, 'A', 4}, {0xD6, 'O', 4}, {0xC8, 'E', 1}, {0xCB, 'E', 4}, {0xCC, 'I', 1}, {0xCE, 'I', 2}, {0xCF, 'I', 4}, {0xD2, 'O', 1},
  {0xD9, 'U', 1}, {0xDB, 'U', 2}, {0xD1, 'N', 3},
  {0xE4, 'a', 4}, {0xF6, 'o', 4}, {0xE8, 'e', 1}, {0xEB, 'e', 4}, {0xEC, 'i', 1}, {0xEE, 'i', 2}, {0xEF, 'i', 4}, {0xF2, 'o', 1},
  {0xF9, 'u', 1}, {0xFB, 'u', 2}, {0xF1, 'n', 3},
  // Cyrillic with marks
  {0x419, 0x418, 5}, {0x439, 0x438, 5}, {0x401, 'E', 4}, {0x451, 'e', 4},
};

int utf8Next(const std::string& s, size_t& i) {
  u8 c = (u8)s[i++];
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && i < s.size()) { int cp = ((c & 0x1F) << 6) | ((u8)s[i++] & 0x3F); return (cp < 256 || (cp >= 0x410 && cp <= 0x44F) || cp == 0x401 || cp == 0x451) ? cp : '?'; }
  if ((c & 0xF0) == 0xE0 && i + 1 < s.size()) { int cp = ((c & 0x0F) << 12) | (((u8)s[i] & 0x3F) << 6) | ((u8)s[i + 1] & 0x3F); i += 2; if (cp == 0x2022) return 0x95; if (cp == 0x2014 || cp == 0x2013 || cp == 0x2212) return '-'; if (cp == 0x2018 || cp == 0x2019) return '\''; if (cp == 0x201C || cp == 0x201D) return '"'; if (cp == 0x2026) return '.'; return '?'; }
  if ((c & 0xF8) == 0xF0 && i + 2 < s.size()) { i += 3; return '?'; }
  return '?';
}

// glyph bitmap 5 x 10 (rows 0-1 accent area, rows 2-9 body)
static void glyphBits(int cp, bool bits[10][5], int& width) {
  memset(bits, 0, sizeof(bool) * 50); width = 5;
  auto putRows = [&](const char* rows, int rowOff) {
    int r = rowOff, col = 0;
    for (const char* p = rows; *p; p++) {
      if (*p == '|') { r++; col = 0; continue; }
      if (r >= 0 && r < 10 && col < 5) bits[r][col] = (*p == '#');
      col++;
    }
  };
  cp = cyrAlias(cp);
  for (auto& g : kFont) if (g.cp == cp) { putRows(g.rows, 2); goto done; }
  for (auto& a : kAccented)
    if (a.cp == cp) {
      for (auto& g : kFont) if (g.cp == a.base) putRows(g.rows, 2);
      bool upper = a.base < 'a' || a.base == 0x418;
      if (a.base == 'i' && a.acc != 5) { for (int c2 = 0; c2 < 5; c2++) bits[2][c2] = false; }
      int ar = upper ? 0 : 2;
      if (!upper) { bits[2][0] = bits[2][1] = bits[2][2] = bits[2][3] = bits[2][4] = false; bits[3][0] = bits[3][1] = bits[3][2] = bits[3][3] = bits[3][4] = false; }
      putRows(kAccent[a.acc], ar);
      goto done;
    }
  if (cp == 0xC7 || cp == 0xE7) { // Ç ç
    for (auto& g : kFont) if (g.cp == (cp == 0xC7 ? 'C' : 'c')) putRows(g.rows, 2);
    bits[9][1] = bits[9][2] = true; goto done;
  }
  for (auto& g : kFont) if (g.cp == '?') putRows(g.rows, 2);
done:
  if (cp == ' ') width = 3;
  else {
    // proportional width trimming for narrow glyphs
    int minc = 5, maxc = -1;
    for (int r = 0; r < 10; r++) for (int c2 = 0; c2 < 5; c2++) if (bits[r][c2]) { minc = std::min(minc, c2); maxc = std::max(maxc, c2); }
    if (maxc >= 0 && (cp == 'i' || cp == 'l' || cp == '!' || cp == '.' || cp == ',' || cp == ':' || cp == ';' || cp == '\'' || cp == '|' || cp == 0xED)) {
      // shift left & shrink
      for (int r = 0; r < 10; r++) {
        bool tmp[5] = {0};
        for (int c2 = minc; c2 <= maxc; c2++) tmp[c2 - minc] = bits[r][c2];
        for (int c2 = 0; c2 < 5; c2++) bits[r][c2] = tmp[c2];
      }
      width = maxc - minc + 1;
    }
  }
}

// =====================================================================
//  Atlas
// =====================================================================
void artBuildAtlas(UIAtlas& A, const std::vector<IconRecipe>& icons) {
  A.px.assign((size_t)ATLAS_SIZE * ATLAS_SIZE * 4, 0);
  auto P = [&](int x, int y, Col c) { if (x < 0 || y < 0 || x >= ATLAS_SIZE || y >= ATLAS_SIZE) return; u8* d = &A.px[((size_t)y * ATLAS_SIZE + x) * 4]; d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = c.a; };
  // --- font: Latin-1 cells of 8x12 at (0,0), 32 per row; Cyrillic cells in the strip y=96..108
  for (int gi = 0; gi < GLYPH_COUNT; gi++) {
    int cp = glyphCp(gi);
    // Latin-1 cells: 32 per row from y=0; extended cells (Cyrillic): one wide row at y=96
    int cx = gi < 256 ? (gi % 32) * GLYPH_CW : ((gi - 256) % 128) * GLYPH_CW, cy = gi < 256 ? (gi / 32) * GLYPH_CH : 96 + ((gi - 256) / 128) * GLYPH_CH;
    A.glyph[gi] = {(u16)cx, (u16)cy, (u16)GLYPH_CW, (u16)GLYPH_CH};
    if (gi < 32) { A.glyphW[gi] = 0; continue; }
    bool bits[10][5]; int w;
    glyphBits(cp, bits, w);
    A.glyphW[gi] = w + 1;
    // outline first
    for (int r = 0; r < 10; r++)
      for (int c = 0; c < 5; c++) {
        if (!bits[r][c]) continue;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) P(cx + 1 + c + dx, cy + 1 + r + dy, Col(10, 8, 14, 255));
      }
    for (int r = 0; r < 10; r++) for (int c = 0; c < 5; c++) if (bits[r][c]) P(cx + 1 + c, cy + 1 + r, Col(255, 255, 255, 255));
  }
  // --- icons: 32x32 cells, region y=128.., 32 per row
  for (size_t i = 0; i < icons.size() && i < 512; i++) {
    int ix = (int)(i % 32) * 32, iy = 128 + (int)(i / 32) * 32;
    A.icon[i] = {(u16)ix, (u16)iy, 32, 32};
    if (icons[i].shape == IS_NONE) continue;
    Canvas c(32, 32);
    artDrawIcon(c, icons[i]);
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) if (c.px[y * 32 + x].a) P(ix + x, iy + y, c.px[y * 32 + x]);
  }
  // --- sprites from y=768
  int sx = 0, sy = 768, rowH = 0;
  auto alloc = [&](int w, int h) { if (sx + w > ATLAS_SIZE) { sx = 0; sy += rowH + 2; rowH = 0; } ARect r{(u16)sx, (u16)sy, (u16)w, (u16)h}; sx += w + 2; rowH = std::max(rowH, h); return r; };
  auto spr = [&](Sprite s, int w, int h, std::function<Col(float, float)> f) {
    ARect r = alloc(w, h); A.sprite[s] = r;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { float u = (x + .5f) / w * 2 - 1, v = (y + .5f) / h * 2 - 1; P(r.x + x, r.y + y, f(u, v)); }
  };
  spr(SP_WHITE, 4, 4, [](float, float) { return Col(255, 255, 255, 255); });
  spr(SP_SOFT, 64, 64, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = sat(1 - d); return Col(255, 255, 255, (u8)(a * a * 255)); });
  spr(SP_RING, 64, 64, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = sat(1 - fabsf(d - 0.8f) * 8); return Col(255, 255, 255, (u8)(a * 255)); });
  spr(SP_SPARK, 32, 32, [](float u, float v) { float a = sat(1 - fabsf(u) * 6) * sat(1 - fabsf(v)) + sat(1 - fabsf(v) * 6) * sat(1 - fabsf(u)); return Col(255, 255, 255, (u8)(sat(a) * 255)); });
  spr(SP_STAR, 32, 32, [](float u, float v) { float a = atan2f(v, u), d = sqrtf(u * u + v * v); float r = 0.45f + 0.5f * powf(fabsf(cosf(a * 2.5f)), 3); return Col(255, 255, 255, d < r ? 255 : 0); });
  spr(SP_SMOKE, 64, 64, [](float u, float v) { float d = sqrtf(u * u + v * v); float n = vnoise(u * 4 + 10, v * 4 + 10, 5); float a = sat(1 - d) * (0.6f + n * 0.6f); return Col(255, 255, 255, (u8)(sat(a) * 255)); });
  spr(SP_SHADOW, 64, 64, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = sat((1 - d) * 2.5f); return Col(0, 0, 0, (u8)(a * 255)); });
  spr(SP_DECAL_RING, 128, 128, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = d < 0.96f && d > 0.88f ? 1.f : (d < 0.88f ? 0.22f : 0.f); return Col(255, 255, 255, (u8)(a * 255)); });
  spr(SP_DECAL_DISC, 128, 128, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = d < 1 ? sat((1 - d) * 4) * 0.4f + 0.6f * (d > 0.9f) : 0; return Col(255, 255, 255, (u8)(sat(a) * 255)); });
  // pixel-art circle outline (joystick base & buttons) like the touch concept
  auto pixCircle = [&](Sprite s, int size, float th, bool fill) {
    spr(s, size, size, [size, th, fill](float u, float v) {
      int px = (int)((u * .5f + .5f) * size / 4), py = (int)((v * .5f + .5f) * size / 4); // 4x4 chunky pixels
      float cu = (px + .5f) * 4 / size * 2 - 1, cv = (py + .5f) * 4 / size * 2 - 1;
      float d = sqrtf(cu * cu + cv * cv);
      if (fill) return d < 0.92f ? Col(255, 255, 255, 220) : Col(0, 0, 0, 0);
      if (d < 1.0f && d > 1.0f - th) return Col(255, 255, 255, 235);
      if (d <= 1.0f - th) return Col(255, 255, 255, 40);
      return Col(0, 0, 0, 0);
    });
  };
  pixCircle(SP_JOY_BASE, 128, 0.09f, false);
  pixCircle(SP_JOY_KNOB, 64, 0, true);
  pixCircle(SP_BTN_BIG, 128, 0.085f, false);
  pixCircle(SP_BTN_SMALL, 64, 0.14f, false);
  spr(SP_PANEL, 16, 16, [](float u, float v) { bool edge = fabsf(u) > 0.76f || fabsf(v) > 0.76f; return edge ? Col(255, 255, 255, 255) : Col(255, 255, 255, 255); });
  spr(SP_GLOW, 64, 64, [](float u, float v) { float d = sqrtf(u * u + v * v); float a = sat(1 - d); return Col(255, 255, 255, (u8)(powf(a, 1.6f) * 255)); });
  spr(SP_ARROW, 32, 32, [](float u, float v) { bool in = (v > -0.9f && v < 0.9f && fabsf(u) < (0.9f - v) * 0.5f); return Col(255, 255, 255, in ? 255 : 0); });
  spr(SP_SLASH, 64, 32, [](float u, float v) { float a = sat(1 - fabsf(v) * (1.2f + 2 * fabsf(u))) * sat(1 - fabsf(u)); return Col(255, 255, 255, (u8)(a * 255)); });
  spr(SP_FLAME, 32, 32, [](float u, float v) { float w = (1 - (v * .5f + .5f)) * 0.9f; float a = sat(1 - fabsf(u) / (w + 0.15f)) * sat((v + 1) * 2) * sat(1 - (v * .5f + .5f) * 0.2f); return Col(255, 255, 255, (u8)(sat(a) * 255)); });
  spr(SP_BOLT, 16, 64, [](float u, float v) { float a = sat(1 - fabsf(u) * 2.5f); return Col(255, 255, 255, (u8)(a * 255)); });
  spr(SP_DROP, 32, 32, [](float u, float v) { float d = sqrtf(u * u + (v - 0.3f) * (v - 0.3f)); bool in = d < 0.6f || (v < 0.3f && fabsf(u) < (v + 1) * 0.45f); return Col(255, 255, 255, in ? 255 : 0); });
  spr(SP_SWIRL, 64, 64, [](float u, float v) { float a = atan2f(v, u), d = sqrtf(u * u + v * v); float s = fractf(a / TAU * 2 + d * 1.5f); float al = d < 1 ? (s < 0.5f ? 1.f : 0.25f) * sat((1 - d) * 3) : 0; return Col(255, 255, 255, (u8)(al * 255)); });
  spr(SP_CROSS, 32, 32, [](float u, float v) { bool in = fabsf(u) < 0.25f || fabsf(v) < 0.25f; return Col(255, 255, 255, in ? 255 : 0); });
}
