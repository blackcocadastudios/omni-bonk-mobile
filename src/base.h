// OmniBonk Mobile - base utilities, math, rng, noise
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "OmniBonk", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OmniBonk", __VA_ARGS__)
#else
#define LOGI(...) do{fprintf(stdout,__VA_ARGS__);fputc('\n',stdout);fflush(stdout);}while(0)
#define LOGE(...) do{fprintf(stderr,__VA_ARGS__);fputc('\n',stderr);}while(0)
#endif

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717959f;
constexpr float DEG = PI / 180.f;

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float sat(float x) { return clampf(x, 0.f, 1.f); }
inline float smooth01(float t) { t = sat(t); return t * t * (3 - 2 * t); }
inline float signf(float x) { return x < 0 ? -1.f : 1.f; }
inline float sq(float x) { return x * x; }
inline float approachf(float cur, float tgt, float d) { return cur < tgt ? std::min(cur + d, tgt) : std::max(cur - d, tgt); }
inline float wrapAngle(float a) { while (a > PI) a -= TAU; while (a < -PI) a += TAU; return a; }
inline float angleLerp(float a, float b, float t) { return a + wrapAngle(b - a) * t; }
inline float fractf(float x) { return x - floorf(x); }

struct v2 {
  float x = 0, y = 0;
  v2() {}
  v2(float a, float b) : x(a), y(b) {}
  v2 operator+(v2 o) const { return {x + o.x, y + o.y}; }
  v2 operator-(v2 o) const { return {x - o.x, y - o.y}; }
  v2 operator*(float s) const { return {x * s, y * s}; }
  v2 operator/(float s) const { return {x / s, y / s}; }
  v2& operator+=(v2 o) { x += o.x; y += o.y; return *this; }
  v2& operator-=(v2 o) { x -= o.x; y -= o.y; return *this; }
  v2& operator*=(float s) { x *= s; y *= s; return *this; }
};
inline float dot(v2 a, v2 b) { return a.x * b.x + a.y * b.y; }
inline float len(v2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
inline v2 norm(v2 a) { float l = len(a); return l > 1e-6f ? a / l : v2(0, 0); }

struct v3 {
  float x = 0, y = 0, z = 0;
  v3() {}
  v3(float a, float b, float c) : x(a), y(b), z(c) {}
  v3 operator+(v3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  v3 operator-(v3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  v3 operator-() const { return {-x, -y, -z}; }
  v3 operator*(float s) const { return {x * s, y * s, z * s}; }
  v3 operator*(v3 o) const { return {x * o.x, y * o.y, z * o.z}; }
  v3 operator/(float s) const { return {x / s, y / s, z / s}; }
  v3& operator+=(v3 o) { x += o.x; y += o.y; z += o.z; return *this; }
  v3& operator-=(v3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  v3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  float& operator[](int i) { return (&x)[i]; }
  float operator[](int i) const { return (&x)[i]; }
};
inline float dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline v3 cross(v3 a, v3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float len(v3 a) { return sqrtf(dot(a, a)); }
inline float len2(v3 a) { return dot(a, a); }
inline v3 norm(v3 a) { float l = len(a); return l > 1e-6f ? a / l : v3(0, 0, 0); }
inline v3 lerp3(v3 a, v3 b, float t) { return a + (b - a) * t; }
inline float distXZ(v3 a, v3 b) { return sqrtf(sq(a.x - b.x) + sq(a.z - b.z)); }
inline float dist2XZ(v3 a, v3 b) { return sq(a.x - b.x) + sq(a.z - b.z); }
inline v3 flatXZ(v3 a) { return {a.x, 0, a.z}; }

struct v4 {
  float x = 0, y = 0, z = 0, w = 0;
  v4() {}
  v4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
  v4(v3 v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
  v3 xyz() const { return {x, y, z}; }
};

// quaternion (x,y,z,w)
struct q4 {
  float x = 0, y = 0, z = 0, w = 1;
  q4() {}
  q4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
  static q4 axis(v3 ax, float ang) { ax = norm(ax); float s = sinf(ang * .5f); return {ax.x * s, ax.y * s, ax.z * s, cosf(ang * .5f)}; }
  static q4 yaw(float a) { return {0, sinf(a * .5f), 0, cosf(a * .5f)}; }
  q4 operator*(q4 b) const {
    return {w * b.x + x * b.w + y * b.z - z * b.y, w * b.y - x * b.z + y * b.w + z * b.x,
            w * b.z + x * b.y - y * b.x + z * b.w, w * b.w - x * b.x - y * b.y - z * b.z};
  }
  v3 rot(v3 v) const { v3 q(x, y, z); v3 t = cross(q, v) * 2.f; return v + t * w + cross(q, t); }
};
// yaw (Y) then pitch (X) then roll (Z), applied in that hierarchical order
inline q4 qEuler(float yaw, float pitch, float roll) { return q4::yaw(yaw) * q4::axis({1, 0, 0}, pitch) * q4::axis({0, 0, 1}, roll); }

// column-major 4x4
struct m4 {
  float m[16];
  static m4 I() { m4 r; memset(r.m, 0, sizeof(r.m)); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }
  m4 operator*(const m4& b) const {
    m4 r;
    for (int c = 0; c < 4; c++)
      for (int rr = 0; rr < 4; rr++) {
        float s = 0;
        for (int k = 0; k < 4; k++) s += at(rr, k) * b.at(k, c);
        r.m[c * 4 + rr] = s;
      }
    return r;
  }
  v3 pt(v3 p) const { return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3), at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3), at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3)}; }
  v3 dir(v3 p) const { return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z, at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z, at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z}; }
  v4 mul4(v4 p) const {
    return {at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2) * p.z + at(0, 3) * p.w, at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2) * p.z + at(1, 3) * p.w,
            at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2) * p.z + at(2, 3) * p.w, at(3, 0) * p.x + at(3, 1) * p.y + at(3, 2) * p.z + at(3, 3) * p.w};
  }
  static m4 T(v3 t) { m4 r = I(); r.at(0, 3) = t.x; r.at(1, 3) = t.y; r.at(2, 3) = t.z; return r; }
  static m4 S(v3 s) { m4 r = I(); r.at(0, 0) = s.x; r.at(1, 1) = s.y; r.at(2, 2) = s.z; return r; }
  static m4 RX(float a) { m4 r = I(); float c = cosf(a), s = sinf(a); r.at(1, 1) = c; r.at(1, 2) = -s; r.at(2, 1) = s; r.at(2, 2) = c; return r; }
  static m4 RY(float a) { m4 r = I(); float c = cosf(a), s = sinf(a); r.at(0, 0) = c; r.at(0, 2) = s; r.at(2, 0) = -s; r.at(2, 2) = c; return r; }
  static m4 RZ(float a) { m4 r = I(); float c = cosf(a), s = sinf(a); r.at(0, 0) = c; r.at(0, 1) = -s; r.at(1, 0) = s; r.at(1, 1) = c; return r; }
  static m4 Q(q4 q) {
    m4 r = I();
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.at(0, 0) = 1 - 2 * (yy + zz); r.at(0, 1) = 2 * (xy - wz); r.at(0, 2) = 2 * (xz + wy);
    r.at(1, 0) = 2 * (xy + wz); r.at(1, 1) = 1 - 2 * (xx + zz); r.at(1, 2) = 2 * (yz - wx);
    r.at(2, 0) = 2 * (xz - wy); r.at(2, 1) = 2 * (yz + wx); r.at(2, 2) = 1 - 2 * (xx + yy);
    return r;
  }
  static m4 persp(float fovy, float asp, float n, float f) {
    m4 r; memset(r.m, 0, sizeof(r.m));
    float t = 1.f / tanf(fovy * .5f);
    r.at(0, 0) = t / asp; r.at(1, 1) = t; r.at(2, 2) = (f + n) / (n - f); r.at(2, 3) = 2 * f * n / (n - f); r.at(3, 2) = -1;
    return r;
  }
  static m4 ortho(float l, float rr, float b, float t, float n, float f) {
    m4 r = I();
    r.at(0, 0) = 2 / (rr - l); r.at(1, 1) = 2 / (t - b); r.at(2, 2) = -2 / (f - n);
    r.at(0, 3) = -(rr + l) / (rr - l); r.at(1, 3) = -(t + b) / (t - b); r.at(2, 3) = -(f + n) / (f - n);
    return r;
  }
  static m4 lookAt(v3 eye, v3 at, v3 up) {
    v3 f = norm(at - eye), s = norm(cross(f, up)), u = cross(s, f);
    m4 r = I();
    r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z; r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z; r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
  }
  m4 inverse() const {
    const float* a = m; float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    m4 r; det = fabsf(det) > 1e-12f ? 1.f / det : 0.f;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
  }
};

// ---------- RNG ----------
struct Rng {
  u64 s;
  explicit Rng(u64 seed = 0x9E3779B97F4A7C15ULL) : s(seed ? seed : 1) {}
  u32 next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (u32)(s >> 11); }
  float f() { return (next() & 0xFFFFFF) / 16777216.f; }
  float range(float a, float b) { return a + (b - a) * f(); }
  int irange(int n) { return n <= 0 ? 0 : (int)(next() % (u32)n); }
  int irange(int a, int b) { return a + irange(b - a + 1); }
  bool chance(float p) { return f() < p; }
  float sign() { return (next() & 1) ? 1.f : -1.f; }
  v3 inDisk(float r) { float a = f() * TAU, d = sqrtf(f()) * r; return {cosf(a) * d, 0, sinf(a) * d}; }
  v3 onSphere() { float z = range(-1, 1), a = f() * TAU, r = sqrtf(1 - z * z); return {r * cosf(a), z, r * sinf(a)}; }
};

inline u32 hash32(u32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline u32 hash2i(int x, int y, u32 seed) { return hash32((u32)x * 0x8da6b343U ^ (u32)y * 0xd8163841U ^ seed * 0xcb1ab31fU); }
inline float hashf(int x, int y, u32 seed) { return (hash2i(x, y, seed) & 0xFFFFFF) / 16777216.f; }
inline float vnoise(float x, float y, u32 seed) {
  int ix = (int)floorf(x), iy = (int)floorf(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  float a = hashf(ix, iy, seed), b = hashf(ix + 1, iy, seed), c = hashf(ix, iy + 1, seed), d = hashf(ix + 1, iy + 1, seed);
  return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
inline float fbm(float x, float y, u32 seed, int oct = 4) {
  float s = 0, a = 0.5f, n = 0;
  for (int i = 0; i < oct; i++) { s += a * vnoise(x, y, seed + i * 131); n += a; x *= 2.03f; y *= 2.03f; a *= 0.5f; }
  return s / n;
}
// tileable value noise (period p)
inline float vnoiseTile(float x, float y, int p, u32 seed) {
  int ix = (int)floorf(x), iy = (int)floorf(y);
  float fx = x - ix, fy = y - iy;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  auto H = [&](int a, int b) { return hashf(((a % p) + p) % p, ((b % p) + p) % p, seed); };
  return lerpf(lerpf(H(ix, iy), H(ix + 1, iy), fx), lerpf(H(ix, iy + 1), H(ix + 1, iy + 1), fx), fy);
}

// ---------- color ----------
struct Col {
  u8 r = 255, g = 255, b = 255, a = 255;
  Col() {}
  Col(u8 R, u8 G, u8 B, u8 A = 255) : r(R), g(G), b(B), a(A) {}
};
inline Col hexc(u32 h, u8 a = 255) { return Col((h >> 16) & 255, (h >> 8) & 255, h & 255, a); }
inline Col mixc(Col a, Col b, float t) {
  t = sat(t);
  return Col((u8)lerpf(a.r, b.r, t), (u8)lerpf(a.g, b.g, t), (u8)lerpf(a.b, b.b, t), (u8)lerpf(a.a, b.a, t));
}
inline Col mulc(Col a, float k) { return Col((u8)clampf(a.r * k, 0, 255), (u8)clampf(a.g * k, 0, 255), (u8)clampf(a.b * k, 0, 255), a.a); }
inline v3 c2v(Col c) { return {c.r / 255.f, c.g / 255.f, c.b / 255.f}; }
inline Col v2c(v3 v, float a = 1) { return Col((u8)(sat(v.x) * 255), (u8)(sat(v.y) * 255), (u8)(sat(v.z) * 255), (u8)(sat(a) * 255)); }

// half float conversion
inline u16 f2h(float f) {
  u32 x; memcpy(&x, &f, 4);
  u32 sign = (x >> 16) & 0x8000; int e = ((x >> 23) & 0xff) - 127 + 15; u32 m = x & 0x7fffff;
  if (e <= 0) return (u16)sign;
  if (e >= 31) return (u16)(sign | 0x7c00);
  return (u16)(sign | (e << 10) | (m >> 13));
}

inline std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
#include <cstdarg>
inline std::string fmt(const char* f, ...) {
  char buf[1024]; va_list ap; va_start(ap, f); vsnprintf(buf, sizeof(buf), f, ap); va_end(ap); return buf;
}
inline std::string fmtTime(float sec) {
  if (sec < 0) sec = 0;
  int s = (int)sec; return fmt("%d:%02d", s / 60, s % 60);
}
inline std::string fmtNum(double v) {
  // 12,345 style
  long long n = (long long)llround(v); bool neg = n < 0; if (neg) n = -n;
  std::string s = std::to_string(n), o;
  int c = 0;
  for (int i = (int)s.size() - 1; i >= 0; i--) { o.push_back(s[i]); if (++c % 3 == 0 && i > 0) o.push_back(','); }
  if (neg) o.push_back('-');
  std::reverse(o.begin(), o.end());
  return o;
}
inline std::string fmtShort(double v) {
  if (v >= 1e9) return fmt("%.1fB", v / 1e9);
  if (v >= 1e6) return fmt("%.1fM", v / 1e6);
  if (v >= 1e4) return fmt("%.1fK", v / 1e3);
  return fmtNum(v);
}
