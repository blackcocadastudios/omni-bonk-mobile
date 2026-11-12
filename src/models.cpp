// Procedural models. Every model is a small rig of rigid parts; each animation
// clip is sampled at BAKE_FPS and baked into a separate vertex frame (flip-book),
// exactly like the "each frame is a separate mesh" trick of the original.
#include "models.h"

ModelLibrary gModels;

// ---------------------------------------------------------------- mesh building
struct MV { v3 p, n; Col c; v2 uv; u8 layer, emis, flag; };
struct Part { std::vector<MV> v; std::vector<u16> idx; int parent = -1; v3 pivot; };
enum { P_HIPS, P_TORSO, P_HEAD, P_ARM_L, P_ARM_R, P_FORE_L, P_FORE_R, P_THIGH_L, P_THIGH_R, P_SHIN_L, P_SHIN_R, P_PROP, P_TAIL, P_EXTRA, P_X1, P_X2, MAXP };
struct Rig { std::vector<Part> parts; Rig(int n = 1) : parts(n) {} };

struct MB {
  Part* P = nullptr; m4 xf = m4::I(); Col col = Col(255, 255, 255); u8 layer = L_PLAIN, emis = 0, flag = 0; float uvs = 1.f; float jit = 0.08f;
  MB& to(Part& p) { P = &p; return *this; }
  MB& c(u32 h) { col = hexc(h); return *this; }
  MB& cc(Col k) { col = k; return *this; }
  MB& l(u8 L) { layer = L; return *this; }
  MB& e(u8 E) { emis = E; return *this; }
  MB& f(u8 F) { flag = F; return *this; }
  MB& uv(float s) { uvs = s; return *this; }
  MB& X(const m4& m) { xf = m; return *this; }
  MB& noX() { xf = m4::I(); return *this; }
  u16 vert(v3 p, v3 n, v2 t) {
    MV m; m.p = xf.pt(p); m.n = norm(xf.dir(n)); m.uv = t; m.layer = layer; m.emis = emis; m.flag = flag;
    float j = 1 + (hashf((int)(m.p.x * 37), (int)(m.p.y * 41 + m.p.z * 29), 7) - 0.5f) * jit * 2;
    m.c = mulc(col, j);
    P->v.push_back(m); return (u16)(P->v.size() - 1);
  }
  void tri(u16 a, u16 b, u16 c) { P->idx.push_back(a); P->idx.push_back(b); P->idx.push_back(c); }
  void quadIdx(u16 a, u16 b, u16 c, u16 d) { tri(a, b, c); tri(a, c, d); }
  // flat quad given CCW (from outside) corners
  MB& quad(v3 a, v3 b, v3 c2, v3 d, float su = 1, float sv = 1) {
    v3 n = norm(cross(b - a, d - a));
    u16 i0 = vert(a, n, {0, 0}), i1 = vert(b, n, {su * uvs, 0}), i2 = vert(c2, n, {su * uvs, sv * uvs}), i3 = vert(d, n, {0, sv * uvs});
    quadIdx(i0, i1, i2, i3);
   return *this; }
  MB& dquad(v3 a, v3 b, v3 c2, v3 d, float su = 1, float sv = 1) { quad(a, b, c2, d, su, sv); quad(d, c2, b, a, su, sv);  return *this; }
  MB& box(v3 c0, v3 h) { boxT(c0, h, h);  return *this; }
  // tapered box: bottom half-extents hb, top half-extents ht (y extent from hb.y)
  MB& boxT(v3 c0, v3 hb, v3 ht) {
    float y0 = c0.y - hb.y, y1 = c0.y + hb.y;
    v3 b[4] = {{c0.x - hb.x, y0, c0.z - hb.z}, {c0.x + hb.x, y0, c0.z - hb.z}, {c0.x + hb.x, y0, c0.z + hb.z}, {c0.x - hb.x, y0, c0.z + hb.z}};
    v3 t[4] = {{c0.x - ht.x, y1, c0.z - ht.z}, {c0.x + ht.x, y1, c0.z - ht.z}, {c0.x + ht.x, y1, c0.z + ht.z}, {c0.x - ht.x, y1, c0.z + ht.z}};
    float sx = hb.x * 2, sy = hb.y * 2, sz = hb.z * 2;
    quad(t[3], t[2], t[1], t[0], sx, sz);           // top
    quad(b[0], b[1], b[2], b[3], sx, sz);           // bottom
    quad(b[3], b[2], t[2], t[3], sx, sy);           // +z
    quad(b[1], b[0], t[0], t[1], sx, sy);           // -z
    quad(b[2], b[1], t[1], t[2], sz, sy);           // +x
    quad(b[0], b[3], t[3], t[0], sz, sy);           // -x
   return *this; }
  MB& limb(v3 a, v3 b, float ra, float rb, int seg = 6, bool caps = true) {
    v3 ax = norm(b - a); v3 up = fabsf(ax.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    v3 u = norm(cross(ax, up)), w = cross(ax, u);
    float L = len(b - a);
    u16 base = (u16)P->v.size();
    for (int i = 0; i <= seg; i++) {
      float an = i * TAU / seg; v3 d = u * cosf(an) + w * sinf(an);
      vert(a + d * ra, d, {i / (float)seg * uvs * (ra + rb) * 3, 0});
      vert(b + d * rb, d, {i / (float)seg * uvs * (ra + rb) * 3, L * uvs});
    }
    for (int i = 0; i < seg; i++) { u16 a0 = base + 2 * i, b0 = a0 + 1, a1 = a0 + 2, b1 = a0 + 3; tri(a0, a1, b1); tri(a0, b1, b0); }
    if (caps) {
      u16 ct = vert(b, ax, {.5f, .5f}); u16 cb = vert(a, -ax, {.5f, .5f});
      u16 rt = (u16)P->v.size();
      for (int i = 0; i <= seg; i++) { float an = i * TAU / seg; v3 d = u * cosf(an) + w * sinf(an); vert(b + d * rb, ax, {cosf(an) * .5f + .5f, sinf(an) * .5f + .5f}); }
      u16 rb0 = (u16)P->v.size();
      for (int i = 0; i <= seg; i++) { float an = i * TAU / seg; v3 d = u * cosf(an) + w * sinf(an); vert(a + d * ra, -ax, {cosf(an) * .5f + .5f, sinf(an) * .5f + .5f}); }
      for (int i = 0; i < seg; i++) { tri(ct, rt + i, rt + i + 1); tri(cb, rb0 + i + 1, rb0 + i); }
    }
   return *this; }
  MB& cyl(v3 base, float r0, float r1, float h, int seg = 8, bool caps = true) { limb(base, base + v3(0, h, 0), r0, r1, seg, caps);  return *this; }
  MB& cone(v3 base, float r, float h, int seg = 8) { limb(base, base + v3(0, h, 0), r, 0.001f, seg, true);  return *this; }
  MB& sphere(v3 c0, v3 r, int seg = 8, int rings = 6, float cutBottom = 0) {
    u16 base = (u16)P->v.size();
    for (int j = 0; j <= rings; j++) {
      float ph = PI * j / rings; if (cutBottom > 0) ph = std::min(ph, PI * (1 - cutBottom));
      for (int i = 0; i <= seg; i++) {
        float th = TAU * i / seg;
        v3 d(sinf(ph) * cosf(th), cosf(ph), sinf(ph) * sinf(th));
        vert(c0 + d * r, norm(v3(d.x / r.x, d.y / r.y, d.z / r.z)), {i / (float)seg * uvs * 2, j / (float)rings * uvs});
      }
    }
    for (int j = 0; j < rings; j++)
      for (int i = 0; i < seg; i++) {
        u16 a = base + j * (seg + 1) + i, b = a + 1, c2 = a + (seg + 1) + 1, d = a + (seg + 1);
        tri(a, b, c2); tri(a, c2, d);
      }
   return *this; }
  // flat-shaded displaced icosphere (rocks, canopies)
  MB& ico(v3 c0, v3 r, int sub, float noise, u32 seed) {
    const float t = 1.618034f;
    std::vector<v3> V = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    std::vector<int> F = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
    for (auto& p : V) p = norm(p);
    for (int s = 0; s < sub; s++) {
      std::vector<int> F2;
      auto mid = [&](int a, int b) { V.push_back(norm((V[a] + V[b]) * .5f)); return (int)V.size() - 1; };
      for (size_t i = 0; i < F.size(); i += 3) {
        int a = F[i], b = F[i + 1], c3 = F[i + 2];
        int ab = mid(a, b), bc = mid(b, c3), ca = mid(c3, a);
        int nf[] = {a, ab, ca, b, bc, ab, c3, ca, bc, ab, bc, ca};
        F2.insert(F2.end(), nf, nf + 12);
      }
      F = F2;
    }
    // displacement keyed on direction so shared verts move together
    std::vector<v3> P2(V.size());
    for (size_t i = 0; i < V.size(); i++) {
      v3 d = V[i];
      float n = vnoise(d.x * 2.1f + d.z * 1.3f + 10, d.y * 2.3f + d.x * 0.7f + 10, seed) * 2 - 1;
      P2[i] = c0 + d * r * (1 + n * noise);
    }
    for (size_t i = 0; i < F.size(); i += 3) {
      v3 a = P2[F[i]], b = P2[F[i + 1]], c3 = P2[F[i + 2]];
      v3 n = norm(cross(b - a, c3 - a));
      v3 ctr = (a + b + c3) / 3.f;
      if (dot(n, ctr - c0) < 0) { std::swap(b, c3); n = -n; }
      // planar uv
      v3 ux = norm(b - a), uy = cross(n, ux);
      u16 i0 = vert(a, n, {0, 0}), i1 = vert(b, n, {dot(b - a, ux) * uvs, dot(b - a, uy) * uvs}), i2 = vert(c3, n, {dot(c3 - a, ux) * uvs, dot(c3 - a, uy) * uvs});
      tri(i0, i1, i2);
    }
   return *this; }
  // lathe: profile (radius, y) revolved around Y
  MB& lathe(v3 c0, const std::vector<v2>& prof, int seg = 10) {
    u16 base = (u16)P->v.size();
    for (size_t j = 0; j < prof.size(); j++)
      for (int i = 0; i <= seg; i++) {
        float th = TAU * i / seg;
        v2 pp = prof[j];
        v2 tg = (j + 1 < prof.size() ? prof[j + 1] : prof[j]) - (j > 0 ? prof[j - 1] : prof[j]);
        v3 n(cosf(th) * tg.y, -tg.x, sinf(th) * tg.y);
        vert(c0 + v3(cosf(th) * pp.x, pp.y, sinf(th) * pp.x), norm(n), {i / (float)seg * 2 * uvs, pp.y * uvs});
      }
    for (size_t j = 0; j + 1 < prof.size(); j++)
      for (int i = 0; i < seg; i++) {
        u16 a = base + j * (seg + 1) + i, b = a + 1, d = a + (seg + 1), c3 = d + 1;
        tri(a, d, c3); tri(a, c3, b);
      }
   return *this; }
  // flat ring in XZ (for fx)
  MB& ringXZ(v3 c0, float r0, float r1, int seg) {
    for (int i = 0; i < seg; i++) {
      float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg;
      v3 p0(cosf(a0) * r0, 0, sinf(a0) * r0), p1(cosf(a1) * r0, 0, sinf(a1) * r0), q0(cosf(a0) * r1, 0, sinf(a0) * r1), q1(cosf(a1) * r1, 0, sinf(a1) * r1);
      dquad(c0 + p0, c0 + q0, c0 + q1, c0 + p1);
    }
   return *this; }
};

// ---------------------------------------------------------------- posing
struct PP { v3 r, t, s{1, 1, 1}; };
struct Pose { PP p[MAXP]; v3 rootT, rootR, rootS{1, 1, 1}; };
typedef std::function<void(float, Pose&)> PoseFn;
struct ClipDef { int id; float dur; bool loop; PoseFn fn; };

static m4 rotM(v3 r) { return m4::RY(r.y) * m4::RX(r.x) * m4::RZ(r.z); }

static void bakeModel(int id, Rig& rig, const std::vector<ClipDef>& clips, bool transparent = false) {
  ModelLibrary& L = gModels;
  ModelInfo& mi = L.info[id];
  mi.transparent = transparent;
  // flatten parts
  std::vector<MV> verts; std::vector<int> partOf; std::vector<u16> idx;
  for (size_t pi = 0; pi < rig.parts.size(); pi++) {
    Part& p = rig.parts[pi];
    u16 base = (u16)verts.size();
    for (auto& v : p.v) { verts.push_back(v); partOf.push_back((int)pi); }
    for (auto i : p.idx) idx.push_back(base + i);
  }
  if (verts.size() > 65000) LOGE("model %d too many verts %zu", id, verts.size());
  mi.vtx = (int)verts.size(); mi.idx = (int)idx.size();
  mi.statOff = L.stat.size() * sizeof(StatVtx);
  mi.idxOff = L.idx.size() * sizeof(u16);
  float maxR = 0.1f, maxH = 0.1f, minY = 0;
  for (auto& v : verts) {
    StatVtx s; s.r = v.c.r; s.g = v.c.g; s.b = v.c.b; s.emis = v.emis; s.u = v.uv.x; s.v = v.uv.y; s.layer = v.layer; s.f0 = v.flag; s.f1 = s.f2 = 0;
    L.stat.push_back(s);
    maxR = std::max(maxR, sqrtf(v.p.x * v.p.x + v.p.z * v.p.z)); maxH = std::max(maxH, v.p.y); minY = std::min(minY, v.p.y);
  }
  mi.radius = maxR; mi.height = maxH; mi.minY = minY;
  L.idx.insert(L.idx.end(), idx.begin(), idx.end());
  mi.animOff = L.anim.size() * sizeof(AnimVtx);
  int frame = 0;
  auto emitFrame = [&](const Pose& pose) {
    std::vector<m4> W(rig.parts.size()); std::vector<m4> N(rig.parts.size());
    m4 root = m4::T(pose.rootT) * rotM(pose.rootR) * m4::S(pose.rootS);
    for (size_t pi = 0; pi < rig.parts.size(); pi++) {
      Part& p = rig.parts[pi]; const PP& pp = pose.p[pi];
      m4 local = m4::T(p.pivot + pp.t) * rotM(pp.r) * m4::S(pp.s) * m4::T(-p.pivot);
      W[pi] = (p.parent >= 0 ? W[p.parent] : root) * local;
      // normal matrix = inverse transpose (upper 3x3)
      m4 inv = W[pi].inverse(); m4 nt = m4::I();
      for (int r = 0; r < 3; r++) for (int c2 = 0; c2 < 3; c2++) nt.at(r, c2) = inv.at(c2, r);
      N[pi] = nt;
    }
    for (size_t i = 0; i < verts.size(); i++) {
      v3 p = W[partOf[i]].pt(verts[i].p); v3 n = norm(N[partOf[i]].dir(verts[i].n));
      AnimVtx a; a.x = p.x; a.y = p.y; a.z = p.z; a.nx = (i8)clampf(n.x * 127, -127, 127); a.ny = (i8)clampf(n.y * 127, -127, 127); a.nz = (i8)clampf(n.z * 127, -127, 127); a.pad = 0;
      L.anim.push_back(a);
    }
    frame++;
  };
  for (int c2 = 0; c2 < CL_COUNT; c2++) mi.clip[c2] = ModelClip{0, 0, true};
  if (clips.empty()) { Pose p; emitFrame(p); mi.clip[CL_IDLE] = {0, 1, true}; }
  for (auto& cd : clips) {
    int n = std::max(1, (int)lroundf(cd.dur * BAKE_FPS));
    ModelClip mc; mc.first = frame; mc.frames = n; mc.loop = cd.loop;
    for (int f = 0; f < n; f++) {
      float t = cd.loop ? f / (float)n : (n > 1 ? f / (float)(n - 1) : 0);
      Pose p; cd.fn(t, p); emitFrame(p);
    }
    mi.clip[cd.id] = mc;
  }
  // fallbacks
  for (int c2 = 0; c2 < CL_COUNT; c2++) if (mi.clip[c2].frames == 0) mi.clip[c2] = mi.clip[CL_MOVE].frames ? mi.clip[CL_MOVE] : mi.clip[CL_IDLE];
  if (mi.clip[CL_IDLE].frames == 0) mi.clip[CL_IDLE] = {0, 1, true};
  for (int c2 = 0; c2 < CL_COUNT; c2++) if (mi.clip[c2].frames == 0) mi.clip[c2] = mi.clip[CL_IDLE];
  mi.frames = frame;
}
static void bakeStatic(int id, Rig& rig, bool transparent = false) { bakeModel(id, rig, {}, transparent); }

int modelFrame(int model, int clip, float t, int animFps, bool clampEnd) { return modelFrameRef(model, clip, t, animFps, clampEnd).frame; }
FrameRef modelFrameRef(int model, int clip, float t, int animFps, bool clampEnd) {
  const ModelInfo& mi = gModels.info[model];
  const ModelClip& c = mi.clip[clip];
  FrameRef r; r.frame = r.next = c.first; r.blend = 0;
  if (c.frames <= 1) return r;
  if (animFps <= 0) animFps = 12;
  float f;
  if (animFps >= 24) f = t * BAKE_FPS;                                  // smooth: interpolate
  else f = floorf(t * animFps) / animFps * BAKE_FPS;                    // stepped
  int fi = (int)floorf(f); float bl = animFps >= 24 ? f - fi : 0.f;
  int ni = fi + 1;
  if (c.loop && !clampEnd) { fi = ((fi % c.frames) + c.frames) % c.frames; ni = ni % c.frames; }
  else { if (fi >= c.frames - 1) { fi = c.frames - 1; bl = 0; } ni = std::min(ni, c.frames - 1); if (fi < 0) fi = 0; }
  r.frame = c.first + fi; r.next = c.first + ni; r.blend = bl;
  return r;
}
float clipDuration(int model, int clip) { return gModels.info[model].clip[clip].frames / (float)BAKE_FPS; }

// ---------------------------------------------------------------- humanoid rig
struct HSpec {
  float leg = 0.8f, torso = 0.55f, head = 0.3f, shoulder = 0.44f, hip = 0.26f, arm = 0.66f, limbR = 0.075f, torsoW = 0.36f, torsoD = 0.22f;
  float hunch = 0;      // forward bend of torso (rad)
  u32 skin = 0xc8a080, top = 0x445566, bottom = 0x333333, feet = 0x2a2a2a, hands = 0;
  u8 skinL = L_SKIN, topL = L_CLOTH, botL = L_CLOTH;
  bool thinLimbs = false; float bulk = 1.f; bool armsForward = false;
};
struct HJ { v3 hip, neck, headC, shL, shR, elL, elR, haL, haR, hpL, hpR, knL, knR, anL, anR; };
static HJ joints(const HSpec& s) {
  HJ j; float hy = s.leg;
  j.hip = {0, hy, 0};
  j.neck = {0, hy + s.torso, sinf(s.hunch) * s.torso};
  j.headC = j.neck + v3(0, s.head * 0.95f, s.head * 0.1f + sinf(s.hunch) * 0.1f);
  float shy = j.neck.y - 0.05f;
  j.shL = {s.shoulder * .5f, shy, j.neck.z * 0.8f}; j.shR = {-s.shoulder * .5f, shy, j.neck.z * 0.8f};
  j.elL = j.shL + v3(0.03f, -s.arm * .5f, 0); j.elR = j.shR + v3(-0.03f, -s.arm * .5f, 0);
  j.haL = j.elL + v3(0, -s.arm * .5f, 0.02f); j.haR = j.elR + v3(0, -s.arm * .5f, 0.02f);
  j.hpL = {s.hip * .5f, hy, 0}; j.hpR = {-s.hip * .5f, hy, 0};
  j.knL = j.hpL + v3(0, -s.leg * .5f, 0.02f); j.knR = j.hpR + v3(0, -s.leg * .5f, 0.02f);
  j.anL = {s.hip * .5f, 0.07f, 0}; j.anR = {-s.hip * .5f, 0.07f, 0};
  return j;
}
// builds body; head geometry is left to the caller (part P_HEAD)
static Rig humanoid(const HSpec& s, HJ& J, MB& b) {
  Rig r(MAXP); J = joints(s);
  int par[MAXP] = {-1, P_HIPS, P_TORSO, P_TORSO, P_TORSO, P_ARM_L, P_ARM_R, P_HIPS, P_HIPS, P_THIGH_L, P_THIGH_R, P_FORE_R, P_HIPS, P_TORSO, P_HEAD, P_FORE_L};
  v3 piv[MAXP] = {J.hip, J.hip, J.neck, J.shL, J.shR, J.elL, J.elR, J.hpL, J.hpR, J.knL, J.knR, J.haR, J.hip, J.neck, J.headC, J.haL};
  for (int i = 0; i < MAXP; i++) { r.parts[i].parent = par[i]; r.parts[i].pivot = piv[i]; }
  float lr = s.limbR * s.bulk;
  // hips
  b.to(r.parts[P_HIPS]).c(s.bottom).l(s.botL).box(J.hip + v3(0, -0.02f, 0), v3(s.hip * .5f + lr, 0.1f, s.torsoD * .45f));
  // torso (tapered, bent by hunch)
  b.to(r.parts[P_TORSO]).c(s.top).l(s.topL);
  {
    m4 m = m4::T(J.hip) * m4::RX(s.hunch) * m4::T(-J.hip);
    b.X(m).boxT(J.hip + v3(0, s.torso * .5f, 0), v3(s.torsoW * .42f * s.bulk, s.torso * .5f, s.torsoD * .5f * s.bulk), v3(s.torsoW * .5f * s.bulk, s.torso * .5f, s.torsoD * .55f * s.bulk));
    b.noX();
  }
  u32 hand = s.hands ? s.hands : s.skin;
  // arms
  b.to(r.parts[P_ARM_L]).c(s.top).l(s.topL).limb(J.shL, J.elL, lr * 1.1f, lr, 6);
  b.to(r.parts[P_ARM_R]).c(s.top).l(s.topL).limb(J.shR, J.elR, lr * 1.1f, lr, 6);
  b.to(r.parts[P_FORE_L]).c(s.skin).l(s.skinL).limb(J.elL, J.haL, lr, lr * .85f, 6);
  b.to(r.parts[P_FORE_R]).c(s.skin).l(s.skinL).limb(J.elR, J.haR, lr, lr * .85f, 6);
  b.to(r.parts[P_FORE_L]).c(hand).box(J.haL + v3(0, -0.05f, 0), v3(lr * 1.1f, 0.06f, lr * 1.1f));
  b.to(r.parts[P_FORE_R]).c(hand).box(J.haR + v3(0, -0.05f, 0), v3(lr * 1.1f, 0.06f, lr * 1.1f));
  // legs
  b.to(r.parts[P_THIGH_L]).c(s.bottom).l(s.botL).limb(J.hpL, J.knL, lr * 1.3f, lr * 1.05f, 6);
  b.to(r.parts[P_THIGH_R]).c(s.bottom).l(s.botL).limb(J.hpR, J.knR, lr * 1.3f, lr * 1.05f, 6);
  b.to(r.parts[P_SHIN_L]).c(s.bottom).l(s.botL).limb(J.knL, J.anL, lr * 1.05f, lr * .85f, 6);
  b.to(r.parts[P_SHIN_R]).c(s.bottom).l(s.botL).limb(J.knR, J.anR, lr * 1.05f, lr * .85f, 6);
  b.to(r.parts[P_SHIN_L]).c(s.feet).l(L_NOISE).box(J.anL + v3(0, -0.02f, 0.06f), v3(lr * 1.2f, 0.05f, lr * 1.2f + 0.06f));
  b.to(r.parts[P_SHIN_R]).c(s.feet).l(L_NOISE).box(J.anR + v3(0, -0.02f, 0.06f), v3(lr * 1.2f, 0.05f, lr * 1.2f + 0.06f));
  return r;
}

// generic humanoid clips. amp scales motion; kind: 0 normal, 1 heavy, 2 player run, 3 zombie (arms forward)
static std::vector<ClipDef> humanClips(float walkDur, float amp, int kind, bool withAttack = true) {
  std::vector<ClipDef> v;
  bool fwdArms = kind == 3;
  v.push_back({CL_IDLE, 0.67f, true, [=](float t, Pose& p) {
    float s = sinf(t * TAU);
    p.p[P_TORSO].r.x = 0.02f + s * 0.025f; p.p[P_HEAD].r.x = -s * 0.03f;
    p.p[P_ARM_L].r.z = 0.08f + s * 0.03f; p.p[P_ARM_R].r.z = -0.08f - s * 0.03f;
    if (fwdArms) { p.p[P_ARM_L].r.x = p.p[P_ARM_R].r.x = -1.35f; }
    p.p[P_FORE_L].r.x = p.p[P_FORE_R].r.x = -0.15f;
    p.rootT.y = s * 0.01f;
  }});
  v.push_back({CL_MOVE, walkDur, true, [=](float t, Pose& p) {
    float ph = t * TAU, s = sinf(ph), c = cosf(ph);
    float a = (kind == 2 ? 0.95f : kind == 1 ? 0.45f : 0.62f) * amp;
    p.p[P_THIGH_L].r.x = -s * a; p.p[P_THIGH_R].r.x = s * a;
    p.p[P_SHIN_L].r.x = std::max(0.f, -c) * a * 1.4f + 0.1f; p.p[P_SHIN_R].r.x = std::max(0.f, c) * a * 1.4f + 0.1f;
    if (fwdArms) { p.p[P_ARM_L].r.x = -1.35f + s * 0.15f; p.p[P_ARM_R].r.x = -1.35f - s * 0.15f; }
    else { p.p[P_ARM_L].r.x = s * a * 0.8f; p.p[P_ARM_R].r.x = -s * a * 0.8f; }
    p.p[P_FORE_L].r.x = p.p[P_FORE_R].r.x = kind == 2 ? -1.1f : -0.35f;
    p.p[P_ARM_L].r.z = 0.1f; p.p[P_ARM_R].r.z = -0.1f;
    p.p[P_TORSO].r.x = (kind == 2 ? 0.28f : 0.1f); p.p[P_TORSO].r.y = s * 0.12f * amp;
    p.p[P_HEAD].r.x = -(kind == 2 ? 0.2f : 0.06f);
    p.rootT.y = fabsf(c) * 0.06f * amp * (kind == 1 ? 0.6f : 1.f);
    p.rootR.z = s * 0.04f * (kind == 1 ? 2.f : 1.f);
  }});
  if (withAttack)
    v.push_back({CL_ATTACK, 0.55f, false, [=](float t, Pose& p) {
      float w = t < 0.4f ? smooth01(t / 0.4f) : 1 - smooth01((t - 0.4f) / 0.25f);
      float strike = t < 0.4f ? 0 : smooth01((t - 0.4f) / 0.15f) * (1 - smooth01((t - 0.65f) / 0.35f));
      p.p[P_ARM_R].r.x = -2.5f * w - 0.6f * strike; p.p[P_ARM_L].r.x = -2.3f * w - 0.5f * strike;
      p.p[P_FORE_R].r.x = p.p[P_FORE_L].r.x = -0.4f * w;
      p.p[P_TORSO].r.x = -0.15f * w + 0.4f * strike; p.p[P_HEAD].r.x = 0.1f * w;
      p.p[P_THIGH_L].r.x = -0.3f * strike; p.p[P_SHIN_L].r.x = 0.3f * strike;
    }});
  if (kind == 2) {
    v.push_back({CL_JUMP, 0.5f, true, [=](float t, Pose& p) {
      float s = sinf(t * TAU) * 0.08f;
      p.p[P_THIGH_L].r.x = -0.9f + s; p.p[P_SHIN_L].r.x = 1.3f; p.p[P_THIGH_R].r.x = -0.2f - s; p.p[P_SHIN_R].r.x = 0.9f;
      p.p[P_ARM_L].r.x = -2.2f + s; p.p[P_ARM_R].r.x = -0.6f - s; p.p[P_ARM_L].r.z = 0.5f; p.p[P_ARM_R].r.z = -0.6f;
      p.p[P_TORSO].r.x = 0.15f;
    }});
    v.push_back({CL_SLIDE, 0.5f, true, [=](float t, Pose& p) {
      float s = sinf(t * TAU) * 0.04f;
      p.rootR.x = -0.55f; p.rootT.y = -0.25f; p.rootT.z = -0.2f;
      p.p[P_THIGH_L].r.x = -1.5f; p.p[P_SHIN_L].r.x = 0.1f; p.p[P_THIGH_R].r.x = -0.6f; p.p[P_SHIN_R].r.x = 1.6f;
      p.p[P_ARM_L].r.x = 0.6f + s; p.p[P_ARM_R].r.x = 0.9f - s; p.p[P_ARM_L].r.z = 0.7f; p.p[P_ARM_R].r.z = -0.7f;
      p.p[P_TORSO].r.x = 0.35f; p.p[P_HEAD].r.x = 0.2f;
    }});
  }
  return v;
}

static void addEyes(MB& b, Part& head, v3 c0, float sep, float r, u32 col, bool glow = false) {
  b.to(head).c(col).l(L_PLAIN).e(glow ? 255 : 0);
  b.box(c0 + v3(sep, 0, 0), v3(r, r * 1.2f, r * 0.5f)); b.box(c0 + v3(-sep, 0, 0), v3(r, r * 1.2f, r * 0.5f));
  b.e(0);
}

// ---------------------------------------------------------------- characters
static void buildFox() {
  MB b; HSpec s; s.leg = 0.62f; s.torso = 0.46f; s.head = 0.3f; s.shoulder = 0.38f; s.hip = 0.24f; s.arm = 0.52f; s.limbR = 0.07f;
  s.skin = 0xe8822a; s.top = 0x6a3fa8; s.bottom = 0xe8822a; s.feet = 0x2a2026; s.hands = 0x2a2026; s.topL = L_CLOTH; s.botL = L_FUR; s.skinL = L_FUR;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0xe8822a).l(L_FUR).sphere(J.headC, {0.27f, 0.25f, 0.25f}, 8, 6);
  b.c(0xf6efe6).sphere(J.headC + v3(0, -0.07f, 0.17f), {0.13f, 0.1f, 0.16f}, 6, 4);           // muzzle
  b.c(0x1a1416).box(J.headC + v3(0, -0.02f, 0.33f), v3(0.04f, 0.03f, 0.03f));                    // nose
  b.c(0xe8822a).l(L_FUR); b.limb(J.headC + v3(0.15f, 0.16f, 0), J.headC + v3(0.24f, 0.45f, -0.02f), 0.09f, 0.01f, 4);
  b.limb(J.headC + v3(-0.15f, 0.16f, 0), J.headC + v3(-0.24f, 0.45f, -0.02f), 0.09f, 0.01f, 4);
  addEyes(b, H, J.headC + v3(0, 0.04f, 0.22f), 0.1f, 0.035f, 0x161018);
  // white chest + purple mantle
  b.to(r.parts[P_TORSO]).c(0xf6efe6).l(L_FUR).box(J.hip + v3(0, s.torso * .45f, 0.1f), v3(0.11f, 0.16f, 0.03f));
  b.c(0x4a2a80).l(L_CLOTH).boxT(J.neck + v3(0, -0.12f, -0.02f), v3(0.24f, 0.08f, 0.15f), v3(0.15f, 0.08f, 0.12f));
  // tail
  Part& T = r.parts[P_TAIL]; T.pivot = J.hip + v3(0, 0, -0.1f);
  b.to(T).c(0xe8822a).l(L_FUR).sphere(J.hip + v3(0, 0.05f, -0.38f), {0.13f, 0.13f, 0.3f}, 7, 5);
  b.c(0xf6efe6).sphere(J.hip + v3(0, 0.08f, -0.64f), {0.09f, 0.09f, 0.1f}, 6, 4);
  // fire staff in right hand
  Part& W = r.parts[P_PROP];
  b.to(W).c(0x7a4a2a).l(L_WOOD).limb(J.haR + v3(0, -0.45f, 0.05f), J.haR + v3(0, 0.55f, 0.05f), 0.035f, 0.035f, 5);
  b.c(0xff8a1a).e(255).sphere(J.haR + v3(0, 0.66f, 0.05f), {0.1f, 0.1f, 0.1f}, 6, 4); b.e(0);
  auto clips = humanClips(0.5f, 1.0f, 2);
  for (auto& c : clips) {
    auto f0 = c.fn;
    c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_TAIL].r.y = sinf(t * TAU) * 0.35f; p.p[P_TAIL].r.x = 0.2f; p.p[P_PROP].r.x = -(p.p[P_TORSO].r.x + p.p[P_ARM_R].r.x + p.p[P_FORE_R].r.x) * 0.9f; };
  }
  bakeModel(M_FOX, r, clips);
}
static void buildNinja() {
  MB b; HSpec s; s.leg = 0.78f; s.torso = 0.52f; s.head = 0.24f; s.shoulder = 0.44f; s.arm = 0.62f; s.limbR = 0.07f;
  s.skin = 0x26222e; s.top = 0x2a2633; s.bottom = 0x221f2a; s.feet = 0x141218; s.hands = 0x141218;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x26222e).l(L_CLOTH).boxT(J.headC, v3(0.17f, 0.2f, 0.18f), v3(0.15f, 0.2f, 0.16f));
  b.c(0xe0b48a).l(L_SKIN).box(J.headC + v3(0, 0.03f, 0.16f), v3(0.15f, 0.05f, 0.03f));           // eye slit skin
  addEyes(b, H, J.headC + v3(0, 0.03f, 0.19f), 0.06f, 0.025f, 0x101014);
  b.to(H).c(0xd8323a).l(L_CLOTH).box(J.headC + v3(0, 0.12f, 0), v3(0.18f, 0.035f, 0.19f));          // red headband
  b.limb(J.headC + v3(0.02f, 0.12f, -0.18f), J.headC + v3(0.12f, 0.02f, -0.48f), 0.03f, 0.015f, 4);
  b.limb(J.headC + v3(-0.03f, 0.12f, -0.18f), J.headC + v3(-0.08f, -0.04f, -0.44f), 0.03f, 0.015f, 4);
  b.to(r.parts[P_TORSO]).c(0xd8323a).l(L_CLOTH).box(J.hip + v3(0, 0.06f, 0), v3(0.2f, 0.04f, 0.13f)); // belt
  b.c(0xd8323a).boxT(J.neck + v3(0, -0.05f, 0), v3(0.15f, 0.05f, 0.13f), v3(0.12f, 0.05f, 0.11f));  // scarf
  // katana in hand + sheath on back
  b.to(r.parts[P_PROP]).c(0xd6dde6).l(L_METAL).e(40).box(J.haR + v3(0, -0.07f, 0.42f), v3(0.012f, 0.03f, 0.4f)); b.e(0);
  b.c(0xd4a43a).box(J.haR + v3(0, -0.07f, 0.02f), v3(0.05f, 0.05f, 0.012f));
  b.c(0x2b2533).box(J.haR + v3(0, -0.07f, -0.08f), v3(0.022f, 0.022f, 0.09f));
  b.to(r.parts[P_EXTRA]).c(0x1a1620).l(L_WOOD).X(m4::T(J.neck + v3(0, -0.25f, -0.15f)) * m4::RZ(0.7f)).box({0, 0, 0}, v3(0.025f, 0.38f, 0.025f)); b.noX();
  auto clips = humanClips(0.48f, 1.0f, 2);
  for (auto& c : clips) {
    auto f0 = c.fn;
    c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -0.9f; };
  }
  bakeModel(M_NINJA, r, clips);
}
static void buildSkeletonBase(int id, u32 bone, u32 accent, int variant) {
  // variant: 0 player Calcium, 1 skeleton, 2 armored, 3 dusty, 4 dusty armored, 5 gold, 6 xp, 7 king (big)
  MB b; HSpec s; s.leg = 0.78f; s.torso = 0.5f; s.head = 0.26f; s.shoulder = 0.42f; s.arm = 0.64f; s.limbR = 0.04f;
  s.skin = bone; s.top = bone; s.bottom = bone; s.feet = bone; s.skinL = L_BONE; s.topL = L_BONE; s.botL = L_BONE; s.hunch = variant == 0 ? 0 : 0.12f;
  u8 emG = (variant == 5 || variant == 6) ? 60 : 0;
  HJ J; Rig r = humanoid(s, J, b);
  // replace torso with ribcage + spine
  r.parts[P_TORSO].v.clear(); r.parts[P_TORSO].idx.clear();
  b.to(r.parts[P_TORSO]).c(bone).l(L_BONE).e(emG);
  b.limb(J.hip, J.neck, 0.04f, 0.04f, 5);
  for (int i = 0; i < 4; i++) { float y = J.hip.y + 0.18f + i * 0.08f; b.box(v3(0, y, 0.02f + s.hunch * 0.2f), v3(0.16f - i * 0.005f, 0.018f, 0.1f)); }
  b.to(r.parts[P_HIPS]).c(bone).l(L_BONE).e(emG);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(bone).l(L_BONE).e(emG).sphere(J.headC, {0.2f, 0.19f, 0.21f}, 7, 5);
  b.box(J.headC + v3(0, -0.15f, 0.06f), v3(0.12f, 0.05f, 0.1f));                                  // jaw
  bool glow = variant == 0 || variant == 7 || variant == 6;
  addEyes(b, H, J.headC + v3(0, 0.01f, 0.18f), 0.075f, 0.045f, glow ? (variant == 6 ? 0x40c4ff : variant == 7 ? 0xff3030 : 0x40e0ff) : 0x140c10, glow);
  b.to(H).e(emG);
  if (variant == 0) { // Calcium: blue bandana + hoodie scarf
    b.to(H).c(0x2f7de1).l(L_CLOTH).box(J.headC + v3(0, 0.1f, 0), v3(0.21f, 0.04f, 0.22f));
    b.limb(J.headC + v3(0, 0.1f, -0.2f), J.headC + v3(0.06f, -0.05f, -0.42f), 0.03f, 0.015f, 4);
    b.to(r.parts[P_TORSO]).c(0x2f7de1).l(L_CLOTH).boxT(J.neck + v3(0, -0.06f, 0), v3(0.16f, 0.06f, 0.13f), v3(0.12f, 0.06f, 0.1f));
  }
  if (variant == 2 || variant == 4 || variant == 7) {
    u32 met = variant == 4 ? 0x9a8a6a : (variant == 7 ? 0x3a3448 : 0x7a8492);
    b.to(H).c(met).l(L_METAL).e(0).sphere(J.headC + v3(0, 0.05f, 0), {0.23f, 0.17f, 0.23f}, 7, 4, 0.45f);
    b.to(r.parts[P_TORSO]).c(met).l(L_METAL).boxT(J.hip + v3(0, 0.3f, 0.02f), v3(0.19f, 0.17f, 0.14f), v3(0.22f, 0.17f, 0.15f));
    b.to(r.parts[P_FORE_L]).c(met).l(L_METAL).X(m4::T(J.haL + v3(0.08f, 0, 0.08f)) * m4::RZ(0.1f)).sphere({0, 0, 0}, {0.04f, 0.22f, 0.2f}, 6, 4); b.noX();
  }
  if (variant == 7) { // king: crown + cape
    b.to(H).c(0xffcc33).l(L_GOLD).e(60);
    for (int i = 0; i < 5; i++) { float a = i * TAU / 5; b.cone(J.headC + v3(cosf(a) * 0.16f, 0.2f, sinf(a) * 0.16f), 0.05f, 0.14f, 4); }
    b.cyl(J.headC + v3(0, 0.15f, 0), 0.19f, 0.19f, 0.07f, 10, false); b.e(0);
    b.to(r.parts[P_EXTRA]).c(0x8a1a2a).l(L_CLOTH).boxT(J.neck + v3(0, -0.4f, -0.18f), v3(0.32f, 0.42f, 0.03f), v3(0.24f, 0.42f, 0.03f));
  }
  if (variant == 6) { b.to(r.parts[P_TORSO]).c(0x40c4ff).e(255).box(J.hip + v3(0, 0.32f, 0.03f), v3(0.06f, 0.06f, 0.06f)); b.e(0); }
  // weapon: rusty sword (enemies), bone (Calcium), big sword (king)
  Part& W = r.parts[P_PROP];
  if (variant == 0) { b.to(W).c(0xf4ecd8).l(L_BONE).limb(J.haR + v3(0, -0.07f, -0.15f), J.haR + v3(0, -0.07f, 0.25f), 0.03f, 0.03f, 5); b.sphere(J.haR + v3(0.03f, -0.07f, 0.27f), {.05f, .05f, .05f}, 5, 3); b.sphere(J.haR + v3(-0.03f, -0.07f, 0.27f), {.05f, .05f, .05f}, 5, 3); }
  else {
    float L = variant == 7 ? 0.95f : 0.5f;
    b.to(W).c(variant == 7 ? 0x9aa4b4 : 0x8a7a6a).l(L_METAL).box(J.haR + v3(0, -0.07f, L * .5f + 0.05f), v3(0.015f, 0.045f, L * .5f));
    b.c(0x5a4a3a).box(J.haR + v3(0, -0.07f, 0.02f), v3(0.07f, 0.02f, 0.02f));
  }
  auto clips = humanClips(variant == 0 ? 0.42f : 0.7f, variant == 7 ? 0.8f : 1.0f, variant == 0 ? 2 : (variant == 7 ? 1 : 0));
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -1.1f; }; }
  bakeModel(id, r, clips);
}
static void buildMonke() {
  MB b; HSpec s; s.leg = 0.5f; s.torso = 0.5f; s.head = 0.28f; s.shoulder = 0.46f; s.arm = 0.8f; s.limbR = 0.075f; s.hunch = 0.2f;
  s.skin = 0x7a4a26; s.top = 0x7a4a26; s.bottom = 0x7a4a26; s.feet = 0xd9b07a; s.hands = 0xd9b07a; s.skinL = L_FUR; s.topL = L_FUR; s.botL = L_FUR;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x7a4a26).l(L_FUR).sphere(J.headC, {0.25f, 0.24f, 0.24f}, 8, 6);
  b.c(0xe0b888).l(L_SKIN).sphere(J.headC + v3(0, -0.05f, 0.14f), {0.17f, 0.14f, 0.13f}, 7, 5);
  b.c(0x7a4a26).l(L_FUR).sphere(J.headC + v3(0.25f, 0.02f, 0), {0.08f, 0.09f, 0.04f}, 6, 4).sphere(J.headC + v3(-0.25f, 0.02f, 0), {0.08f, 0.09f, 0.04f}, 6, 4);
  addEyes(b, H, J.headC + v3(0, 0.05f, 0.21f), 0.08f, 0.03f, 0x161010);
  b.to(r.parts[P_TORSO]).c(0xe0b888).l(L_SKIN).box(J.hip + v3(0, 0.25f, 0.12f), v3(0.13f, 0.18f, 0.03f));
  Part& T = r.parts[P_TAIL]; T.pivot = J.hip + v3(0, 0, -0.1f);
  b.to(T).c(0x7a4a26).l(L_FUR).limb(J.hip + v3(0, 0, -0.1f), J.hip + v3(0, 0.15f, -0.45f), 0.04f, 0.035f, 5).limb(J.hip + v3(0, 0.15f, -0.45f), J.hip + v3(0, 0.45f, -0.5f), 0.035f, 0.03f, 5);
  b.to(r.parts[P_PROP]).c(0xffe14d).l(L_PLAIN);
  for (int i = 0; i < 5; i++) { float a = -0.6f + i * 0.3f; b.sphere(J.haR + v3(0, -0.08f + sinf(a) * 0.12f, 0.12f + cosf(a) * 0.12f), {0.05f, 0.05f, 0.05f}, 5, 3); }
  auto clips = humanClips(0.46f, 1.0f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_TAIL].r.z = sinf(t * TAU) * 0.3f; }; }
  bakeModel(M_MONKE, r, clips);
}
static void buildOgre() {
  MB b; HSpec s; s.leg = 0.72f; s.torso = 0.7f; s.head = 0.3f; s.shoulder = 0.7f; s.hip = 0.38f; s.arm = 0.78f; s.limbR = 0.12f; s.torsoW = 0.55f; s.torsoD = 0.4f;
  s.skin = 0x7aa34a; s.top = 0x6a4a2a; s.bottom = 0x5a3a22; s.feet = 0x3a2a1a; s.topL = L_CLOTH; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  b.to(r.parts[P_TORSO]).c(0x7aa34a).l(L_SKIN).sphere(J.hip + v3(0, 0.28f, 0.06f), {0.3f, 0.3f, 0.26f}, 8, 6);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x7aa34a).l(L_SKIN).sphere(J.headC + v3(0, -0.02f, 0), {0.26f, 0.24f, 0.24f}, 8, 6);
  b.c(0x5f8a3a).box(J.headC + v3(0, 0.09f, 0.18f), v3(0.2f, 0.04f, 0.06f));
  b.c(0xf0ead8).l(L_BONE).cone(J.headC + v3(0.1f, -0.15f, 0.18f), 0.035f, 0.12f, 4).cone(J.headC + v3(-0.1f, -0.15f, 0.18f), 0.035f, 0.12f, 4);
  addEyes(b, H, J.headC + v3(0, 0.03f, 0.22f), 0.09f, 0.03f, 0x1a1010);
  b.to(H).c(0x7aa34a).l(L_SKIN).sphere(J.headC + v3(0.25f, 0.02f, 0), {0.06f, 0.09f, 0.05f}, 5, 4).sphere(J.headC + v3(-0.25f, 0.02f, 0), {0.06f, 0.09f, 0.05f}, 5, 4);
  b.to(r.parts[P_PROP]).c(0x6a4a2a).l(L_WOOD).limb(J.haR + v3(0, -0.07f, -0.2f), J.haR + v3(0, -0.07f, 0.65f), 0.04f, 0.04f, 5);
  b.c(0xb8c0cc).l(L_METAL).boxT(J.haR + v3(0, 0.08f, 0.55f), v3(0.02f, 0.1f, 0.14f), v3(0.02f, 0.2f, 0.2f));
  auto clips = humanClips(0.62f, 0.9f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -0.8f; }; }
  bakeModel(M_OGRE, r, clips);
}

static void buildRocco() {   // raccoon tinkerer: mask, goggles, tool vest, ringed tail, wrench
  MB b; HSpec s; s.leg = 0.6f; s.torso = 0.46f; s.head = 0.3f; s.shoulder = 0.4f; s.hip = 0.26f; s.arm = 0.52f; s.limbR = 0.075f; s.torsoW = 0.4f;
  s.skin = 0x8a8f99; s.top = 0x6a4a2a; s.bottom = 0x7d828c; s.feet = 0x2a2a33; s.hands = 0x2a2a33; s.skinL = L_FUR; s.topL = L_CLOTH; s.botL = L_FUR;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x8a8f99).l(L_FUR).sphere(J.headC, {0.27f, 0.24f, 0.25f}, 8, 6);
  b.c(0xe8e4dc).sphere(J.headC + v3(0, -0.08f, 0.16f), {0.14f, 0.1f, 0.15f}, 6, 4);                // muzzle
  b.c(0x1a1416).box(J.headC + v3(0, -0.04f, 0.31f), v3(0.045f, 0.035f, 0.03f));                     // nose
  b.c(0x26262e).l(L_PLAIN).boxT(J.headC + v3(0, 0.03f, 0.2f), v3(0.24f, 0.05f, 0.06f), v3(0.21f, 0.05f, 0.05f)); // mask band
  addEyes(b, H, J.headC + v3(0, 0.035f, 0.255f), 0.1f, 0.03f, 0xf4f1ea);
  addEyes(b, H, J.headC + v3(0, 0.035f, 0.27f), 0.1f, 0.016f, 0x101014);
  b.to(H).c(0x8a8f99).l(L_FUR).sphere(J.headC + v3(0.17f, 0.2f, -0.02f), {0.08f, 0.08f, 0.04f}, 5, 4).sphere(J.headC + v3(-0.17f, 0.2f, -0.02f), {0.08f, 0.08f, 0.04f}, 5, 4);
  b.c(0x26262e).l(L_PLAIN).box(J.headC + v3(0.17f, 0.2f, 0.01f), v3(0.04f, 0.04f, 0.015f)).box(J.headC + v3(-0.17f, 0.2f, 0.01f), v3(0.04f, 0.04f, 0.015f));
  b.c(0xc9a227).l(L_METAL).cyl(J.headC + v3(0.07f, 0.13f, 0.17f), 0.05f, 0.05f, 0.05f, 5).cyl(J.headC + v3(-0.07f, 0.13f, 0.17f), 0.05f, 0.05f, 0.05f, 5); // goggles up
  b.c(0x4a3020).l(L_CLOTH).box(J.headC + v3(0, 0.15f, 0.02f), v3(0.26f, 0.025f, 0.24f));
  b.to(r.parts[P_TORSO]).c(0xd8d4cc).l(L_FUR).box(J.hip + v3(0, s.torso * .45f, 0.13f), v3(0.09f, 0.15f, 0.03f));   // belly through the open vest
  b.c(0x3a2a1a).l(L_CLOTH).box(J.hip + v3(0, 0.06f, 0), v3(0.22f, 0.04f, 0.14f));                               // tool belt
  Part& T = r.parts[P_TAIL]; T.pivot = J.hip + v3(0, 0, -0.1f);
  for (int i = 0; i < 4; i++) {
    float k = i / 3.f;
    b.to(T).c(i % 2 ? 0x2a2a33 : 0x9aa0aa).l(L_FUR).sphere(J.hip + v3(0, 0.02f + k * 0.12f, -0.2f - k * 0.38f), {0.11f - k * 0.02f, 0.11f - k * 0.02f, 0.12f}, 5, 3);
  }
  b.to(r.parts[P_PROP]).c(0xa8b0bc).l(L_METAL).box(J.haR + v3(0, -0.07f, 0.2f), v3(0.02f, 0.025f, 0.2f));
  b.box(J.haR + v3(0, -0.07f, 0.42f), v3(0.025f, 0.07f, 0.04f));
  b.c(0x1a1416).box(J.haR + v3(0, -0.07f, 0.47f), v3(0.03f, 0.03f, 0.03f));
  auto clips = humanClips(0.48f, 1.0f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_TAIL].r.y = sinf(t * TAU) * 0.3f; p.p[P_TAIL].r.x = 0.15f; p.p[P_PROP].r.x = -0.9f; }; }
  bakeModel(M_ROCCO, r, clips);
}
static void buildBramble() {   // walking mossy stump: ring-cut top, sprout, mushroom, twig arms, poison flask
  MB b; HSpec s; s.leg = 0.5f; s.torso = 0.62f; s.head = 0.28f; s.shoulder = 0.62f; s.hip = 0.34f; s.arm = 0.58f; s.limbR = 0.08f; s.torsoW = 0.56f; s.torsoD = 0.5f;
  u32 bark = 0x7a5232;
  s.skin = 0x6a4428; s.top = bark; s.bottom = 0x5e3e24; s.feet = 0x4a3020; s.hands = 0x5aa83a; s.skinL = L_BARK; s.topL = L_BARK; s.botL = L_BARK;
  HJ J; Rig r = humanoid(s, J, b);
  r.parts[P_TORSO].v.clear(); r.parts[P_TORSO].idx.clear();
  b.to(r.parts[P_TORSO]).c(bark).l(L_BARK).cyl(J.hip + v3(0, -0.08f, 0), 0.33f, 0.3f, s.torso + 0.1f, 9, false);
  for (int i = 0; i < 4; i++) { float a = i * TAU / 4 + 0.4f; b.c(0x5e3e24).limb(J.hip + v3(cosf(a) * 0.3f, 0.02f, sinf(a) * 0.3f), J.hip + v3(cosf(a) * 0.42f, -0.14f, sinf(a) * 0.42f), 0.06f, 0.02f, 4); }
  Part& H = r.parts[P_HEAD];
  b.to(H).c(bark).l(L_BARK).cyl(J.neck + v3(0, -0.03f, 0), 0.3f, 0.29f, 0.32f, 9, false);
  b.c(0xd9b07a).l(L_WOOD).cyl(J.neck + v3(0, 0.29f, 0), 0.29f, 0.29f, 0.02f, 9, true);               // cut top
  b.c(0xb08a5a).cyl(J.neck + v3(0, 0.305f, 0), 0.17f, 0.17f, 0.012f, 9, true);
  b.c(0x5aa83a).l(L_LEAVES).sphere(J.neck + v3(-0.1f, 0.31f, -0.06f), {0.17f, 0.05f, 0.14f}, 6, 3);  // moss
  b.c(0x4caf50).l(L_PLAIN).limb(J.neck + v3(0.08f, 0.3f, 0.02f), J.neck + v3(0.12f, 0.55f, 0), 0.015f, 0.01f, 4);
  b.c(0x7bd23f).l(L_LEAVES).sphere(J.neck + v3(0.19f, 0.56f, 0), {0.08f, 0.03f, 0.05f}, 5, 3).sphere(J.neck + v3(0.06f, 0.53f, 0), {0.06f, 0.025f, 0.04f}, 5, 3);
  b.c(0xd83a3a).l(L_PLAIN).sphere(J.neck + v3(0.27f, 0.1f, 0.1f), {0.09f, 0.06f, 0.09f}, 6, 3, 0.45f);   // mushroom
  b.c(0xf4f1ea).cyl(J.neck + v3(0.25f, 0.02f, 0.09f), 0.03f, 0.03f, 0.08f, 5, false);
  addEyes(b, H, J.neck + v3(0, 0.17f, 0.29f), 0.1f, 0.04f, 0xd8ff70, true);
  b.to(H).c(0x2a1a0e).l(L_PLAIN).box(J.neck + v3(0, 0.05f, 0.29f), v3(0.08f, 0.02f, 0.02f));
  for (int side : {-1, 1}) b.to(r.parts[side < 0 ? P_FORE_R : P_FORE_L]).c(0x5aa83a).l(L_LEAVES).sphere((side < 0 ? J.haR : J.haL) + v3(0, -0.06f, 0), {0.08f, 0.07f, 0.08f}, 5, 3);
  b.to(r.parts[P_PROP]).c(0x7bd23f).l(L_CRYSTAL).e(120).sphere(J.haR + v3(0, -0.1f, 0.13f), {0.09f, 0.1f, 0.09f}, 6, 4);
  b.e(0).c(0xd8e8f0).l(L_PLAIN).cyl(J.haR + v3(0, -0.01f, 0.13f), 0.035f, 0.03f, 0.08f, 5);
  b.c(0x8b5a2b).l(L_WOOD).cyl(J.haR + v3(0, 0.07f, 0.13f), 0.035f, 0.035f, 0.03f, 5);
  auto clips = humanClips(0.62f, 0.85f, 2);
  bakeModel(M_BRAMBLE, r, clips);
}
static void buildSizzle() {   // gecko line cook: big side eyes, chef hat, white jacket, long tail, frying pan
  MB b; HSpec s; s.leg = 0.62f; s.torso = 0.47f; s.head = 0.26f; s.shoulder = 0.4f; s.hip = 0.24f; s.arm = 0.55f; s.limbR = 0.065f;
  u32 skin = 0x6cc04a;
  s.skin = skin; s.top = 0xf4f1ea; s.bottom = 0x3a3a44; s.feet = skin; s.hands = skin; s.skinL = L_SKIN; s.topL = L_CLOTH; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(skin).l(L_SKIN).sphere(J.headC + v3(0, -0.02f, 0.06f), {0.22f, 0.17f, 0.3f}, 8, 6);
  b.c(0x5aa83a).sphere(J.headC + v3(0, -0.09f, 0.14f), {0.17f, 0.07f, 0.22f}, 7, 4);                  // jaw
  b.c(0x2e6a22).box(J.headC + v3(0, -0.07f, 0.33f), v3(0.11f, 0.008f, 0.02f));                        // smile
  for (int side : {-1, 1}) {
    v3 e = J.headC + v3(side * 0.16f, 0.08f, 0.12f);
    b.c(skin).l(L_SKIN).sphere(e, {0.08f, 0.08f, 0.08f}, 5, 3);
    b.c(0xffd23f).l(L_PLAIN).sphere(e + v3(side * 0.03f, 0.01f, 0.03f), {0.06f, 0.06f, 0.06f}, 5, 3);
    b.c(0x101014).box(e + v3(side * 0.07f, 0.01f, 0.06f), v3(0.012f, 0.04f, 0.012f));
  }
  b.c(0xf4f1ea).l(L_CLOTH).cyl(J.headC + v3(0, 0.1f, -0.01f), 0.17f, 0.19f, 0.18f, 8, false);         // chef hat
  b.sphere(J.headC + v3(0, 0.33f, -0.01f), {0.22f, 0.12f, 0.22f}, 8, 4);
  b.to(r.parts[P_TORSO]).c(0xd83a3a).l(L_CLOTH).boxT(J.neck + v3(0, -0.04f, 0.01f), v3(0.14f, 0.04f, 0.12f), v3(0.11f, 0.04f, 0.1f)); // neckerchief
  b.c(0x2a2a33).l(L_PLAIN);
  for (int i = 0; i < 3; i++) b.box(v3(0, J.hip.y + 0.12f + i * 0.11f, 0.11f), v3(0.016f, 0.016f, 0.01f));
  Part& T = r.parts[P_TAIL]; T.pivot = J.hip + v3(0, -0.05f, -0.08f);
  b.to(T).c(skin).l(L_SKIN).limb(J.hip + v3(0, -0.05f, -0.08f), J.hip + v3(0, -0.3f, -0.45f), 0.07f, 0.045f, 6).limb(J.hip + v3(0, -0.3f, -0.45f), J.hip + v3(0.1f, -0.42f, -0.78f), 0.045f, 0.01f, 5);
  b.to(r.parts[P_PROP]).c(0x2b2533).l(L_METAL).cyl(J.haR + v3(0, -0.09f, 0.42f), 0.15f, 0.15f, 0.03f, 10, true); // frying pan
  b.c(0x4a3020).l(L_WOOD).box(J.haR + v3(0, -0.075f, 0.13f), v3(0.022f, 0.02f, 0.15f));
  auto clips = humanClips(0.48f, 1.0f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_TAIL].r.y = sinf(t * TAU) * 0.4f; p.p[P_PROP].r.x = -0.7f; }; }
  bakeModel(M_SIZZLE, r, clips);
}
static void buildDigby() {   // mole miner: hard hat with lamp, pink nose, overalls, big digging paws, pickaxe
  MB b; HSpec s; s.leg = 0.5f; s.torso = 0.55f; s.head = 0.28f; s.shoulder = 0.56f; s.hip = 0.32f; s.arm = 0.58f; s.limbR = 0.1f; s.torsoW = 0.5f; s.torsoD = 0.38f; s.hunch = 0.12f;
  u32 fur = 0x5a4038;
  s.skin = fur; s.top = 0x3a5a8a; s.bottom = 0x3a5a8a; s.feet = 0x3a2a22; s.hands = 0xe0a8a0; s.skinL = L_FUR; s.topL = L_CLOTH; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(fur).l(L_FUR).sphere(J.headC, {0.27f, 0.25f, 0.26f}, 8, 6);
  b.c(0x7a5a50).sphere(J.headC + v3(0, -0.06f, 0.24f), {0.1f, 0.08f, 0.14f}, 6, 4);                    // snout
  b.c(0xff8fa8).l(L_PLAIN).sphere(J.headC + v3(0, -0.05f, 0.37f), {0.06f, 0.05f, 0.04f}, 6, 4);       // nose
  addEyes(b, H, J.headC + v3(0, 0.05f, 0.24f), 0.1f, 0.018f, 0x101014);
  b.to(H).c(0xffc61a).l(L_PLAIN).sphere(J.headC + v3(0, 0.1f, 0), {0.29f, 0.2f, 0.29f}, 8, 4, 0.5f); // hard hat
  b.c(0xe0a800).cyl(J.headC + v3(0, 0.08f, 0.02f), 0.34f, 0.34f, 0.025f, 10, true);
  b.c(0xfff6b0).e(255).box(J.headC + v3(0, 0.2f, 0.27f), v3(0.06f, 0.05f, 0.03f)); b.e(0);           // lamp
  b.to(r.parts[P_TORSO]).c(0x7a5a50).l(L_FUR).box(J.neck + v3(0, -0.1f, 0.12f), v3(0.13f, 0.08f, 0.03f));
  b.c(0xffc61a).l(L_PLAIN).box(J.hip + v3(0.12f, 0.36f, 0.16f), v3(0.025f, 0.025f, 0.012f)).box(J.hip + v3(-0.12f, 0.36f, 0.16f), v3(0.025f, 0.025f, 0.012f)); // buttons
  for (int side : {-1, 1}) {
    v3 hnd = side < 0 ? J.haR : J.haL; Part& F = r.parts[side < 0 ? P_FORE_R : P_FORE_L];
    b.to(F).c(0xe0a8a0).l(L_SKIN).box(hnd + v3(0, -0.06f, 0.02f), v3(0.11f, 0.07f, 0.09f));             // broad paw
    b.c(0xf4f1ea).l(L_BONE);
    for (int k : {-1, 1}) b.limb(hnd + v3(k * 0.045f, -0.1f, 0.08f), hnd + v3(k * 0.055f, -0.2f, 0.14f), 0.026f, 0.004f, 4);
  }
  b.to(r.parts[P_PROP]).c(0x7a4a2a).l(L_WOOD).limb(J.haR + v3(0, -0.07f, -0.15f), J.haR + v3(0, -0.07f, 0.6f), 0.035f, 0.035f, 5);
  b.c(0x9aa4b4).l(L_METAL).X(m4::T(J.haR + v3(0, 0.0f, 0.58f)) * m4::RX(0.15f)).boxT({0, 0, 0}, v3(0.03f, 0.04f, 0.04f), v3(0.025f, 0.28f, 0.03f)); b.noX();
  auto clips = humanClips(0.6f, 0.9f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -0.8f; }; }
  bakeModel(M_DIGBY, r, clips);
}
static void buildHoot() {   // owl scholar: facial discs, round glasses, ear tufts, mortarboard, wing arms, lightning staff
  MB b; HSpec s; s.leg = 0.42f; s.torso = 0.55f; s.head = 0.3f; s.shoulder = 0.5f; s.hip = 0.28f; s.arm = 0.5f; s.limbR = 0.07f; s.torsoW = 0.5f; s.torsoD = 0.44f;
  u32 feather = 0x8a6a48;
  s.skin = feather; s.top = feather; s.bottom = 0xe0a030; s.feet = 0xe0a030; s.hands = 0x6a5038; s.skinL = L_FEATHER; s.topL = L_FEATHER; s.botL = L_PLAIN;
  HJ J; Rig r = humanoid(s, J, b);
  b.to(r.parts[P_TORSO]).c(feather).l(L_FEATHER).sphere(J.hip + v3(0, 0.3f, 0), {0.3f, 0.34f, 0.27f}, 8, 6);
  b.c(0xf0e2c4).sphere(J.hip + v3(0, 0.26f, 0.12f), {0.2f, 0.25f, 0.16f}, 7, 5);                      // belly
  Part& H = r.parts[P_HEAD];
  b.to(H).c(feather).l(L_FEATHER).sphere(J.headC, {0.3f, 0.27f, 0.27f}, 8, 6);
  for (int side : {-1, 1}) {
    v3 e = J.headC + v3(side * 0.12f, 0.02f, 0.2f);
    b.to(H).c(0xf0e2c4).l(L_PLAIN).sphere(e, {0.12f, 0.12f, 0.06f}, 6, 4);                             // facial disc
    b.c(0x3a3a44).l(L_METAL).sphere(e + v3(0, 0, 0.035f), {0.085f, 0.085f, 0.035f}, 7, 3);           // glasses rim
    b.c(0xffd23f).l(L_PLAIN).sphere(e + v3(0, 0, 0.05f), {0.065f, 0.065f, 0.04f}, 6, 3);
    b.c(0x101014).box(e + v3(0, 0, 0.09f), v3(0.03f, 0.03f, 0.01f));
    b.c(feather).l(L_FEATHER).limb(J.headC + v3(side * 0.18f, 0.18f, 0.02f), J.headC + v3(side * 0.27f, 0.36f, -0.02f), 0.06f, 0.01f, 4); // ear tuft
  }
  b.to(H).c(0xffa726).l(L_PLAIN).X(m4::T(J.headC + v3(0, -0.08f, 0.27f)) * m4::RX(2.3f)).cone({0, 0, 0}, 0.045f, 0.12f, 4); b.noX();
  b.c(0x26222e).l(L_CLOTH).cyl(J.headC + v3(0, 0.2f, -0.02f), 0.16f, 0.16f, 0.07f, 8, false);             // mortarboard
  b.box(J.headC + v3(0, 0.29f, -0.02f), v3(0.27f, 0.02f, 0.27f));
  b.c(0xf5c542).l(L_GOLD).limb(J.headC + v3(0, 0.31f, -0.02f), J.headC + v3(0.24f, 0.3f, 0.18f), 0.01f, 0.01f, 3).limb(J.headC + v3(0.24f, 0.3f, 0.18f), J.headC + v3(0.25f, 0.18f, 0.19f), 0.012f, 0.02f, 4);
  for (int side : {-1, 1}) {
    Part& A = r.parts[side < 0 ? P_ARM_R : P_ARM_L]; v3 sh = side < 0 ? J.shR : J.shL;
    b.to(A).c(0x7a5a3c).l(L_FEATHER).X(m4::T(sh + v3(side * 0.05f, -0.22f, -0.04f)) * m4::RZ(side * 0.12f)).boxT({0, 0, 0}, v3(0.03f, 0.22f, 0.12f), v3(0.04f, 0.22f, 0.14f)); b.noX();
  }
  Part& W = r.parts[P_PROP];
  b.to(W).c(0x7a4a2a).l(L_WOOD).limb(J.haR + v3(0, -0.4f, 0.05f), J.haR + v3(0, 0.55f, 0.05f), 0.03f, 0.03f, 5);
  b.c(0x80e8ff).l(L_CRYSTAL).e(255).X(m4::T(J.haR + v3(0, 0.66f, 0.05f)) * m4::RZ(0.6f)).box({0, 0, 0}, v3(0.06f, 0.1f, 0.06f)); b.noX(); b.e(0);
  auto clips = humanClips(0.44f, 0.9f, 2);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -(p.p[P_TORSO].r.x + p.p[P_ARM_R].r.x + p.p[P_FORE_R].r.x) * 0.9f; }; }
  bakeModel(M_HOOT, r, clips);
}

// ---------------------------------------------------------------- enemies
static void buildGoblin(int id, float scale, int variant) {
  // variant 0 weak, 1 strong (club), 2 tank (armor + big club)
  MB b; HSpec s; float k = scale;
  s.leg = 0.5f * k; s.torso = 0.42f * k; s.head = 0.26f * k; s.shoulder = 0.38f * k * (variant == 2 ? 1.4f : 1); s.hip = 0.22f * k; s.arm = 0.6f * k;
  s.limbR = 0.06f * k * (variant ? 1.3f : 1); s.torsoW = 0.32f * k * (variant == 2 ? 1.5f : 1); s.torsoD = 0.24f * k * (variant == 2 ? 1.4f : 1); s.hunch = 0.35f;
  u32 skin = variant == 0 ? 0x5fbf4a : variant == 1 ? 0x4aa83a : 0x3f8f34;
  s.skin = skin; s.top = skin; s.bottom = 0x6a4428; s.feet = skin; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(skin).l(L_SKIN).sphere(J.headC, v3(0.24f, 0.21f, 0.23f) * k, 8, 6);
  b.c(skin).limb(J.headC + v3(0.16f, 0.05f, 0) * k, J.headC + v3(0.46f, 0.18f, -0.08f) * k, 0.07f * k, 0.01f, 4);   // ears
  b.limb(J.headC + v3(-0.16f, 0.05f, 0) * k, J.headC + v3(-0.46f, 0.18f, -0.08f) * k, 0.07f * k, 0.01f, 4);
  b.c(mulc(hexc(skin), 0.85f).r << 16 | 0).cc(mulc(hexc(skin), 0.85f)).cone(J.headC + v3(0, -0.03f, 0.18f) * k, 0.05f * k, 0.14f * k, 4); // nose (pointing up, fine)
  addEyes(b, H, J.headC + v3(0, 0.05f, 0.2f) * k, 0.08f * k, 0.035f * k, variant == 2 ? 0xffd040 : 0xf0e040, true);
  b.to(r.parts[P_HIPS]).c(0x6a4428).l(L_CLOTH).boxT(J.hip + v3(0, -0.12f * k, 0.02f), v3(0.18f, 0.12f, 0.15f) * k, v3(0.15f, 0.12f, 0.14f) * k);
  if (variant >= 1) {
    b.to(r.parts[P_PROP]).c(0x6a4a2a).l(L_WOOD).limb(J.haR + v3(0, -0.07f, -0.05f) * k, J.haR + v3(0, -0.07f, 0.55f) * k, 0.04f * k, 0.09f * k, 5);
  }
  if (variant == 2) {
    b.to(r.parts[P_TORSO]).c(0x6a6a72).l(L_METAL).boxT(J.hip + v3(0, 0.26f, 0.06f) * k, v3(0.24f, 0.2f, 0.18f) * k, v3(0.28f, 0.2f, 0.2f) * k);
    b.to(H).c(0x6a6a72).l(L_METAL).sphere(J.headC + v3(0, 0.06f, 0) * k, v3(0.26f, 0.18f, 0.25f) * k, 7, 4, 0.45f);
    b.to(r.parts[P_PROP]).c(0x7a7a80).l(L_METAL).ico(J.haR + v3(0, -0.07f, 0.6f) * k, v3(0.13f, 0.13f, 0.13f) * k, 0, 0.2f, 3);
  }
  auto clips = humanClips(variant == 2 ? 0.75f : 0.6f, 1.0f, variant == 2 ? 1 : 0);
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -0.7f; }; }
  bakeModel(id, r, clips);
}
static void buildSlime() {
  Rig r(2); MB b;
  r.parts[0].pivot = {0, 0, 0}; r.parts[1].parent = 0; r.parts[1].pivot = {0, 0.3f, 0};
  b.to(r.parts[0]).c(0x5ad0ff).l(L_SLIME).e(30).sphere({0, 0.3f, 0}, {0.45f, 0.36f, 0.45f}, 10, 7); b.e(0);
  b.c(0xb8f0ff).e(80).sphere({0.14f, 0.5f, 0.2f}, {0.08f, 0.06f, 0.06f}, 5, 3); b.e(0);
  addEyes(b, r.parts[1], {0, 0.38f, 0.38f}, 0.12f, 0.05f, 0x0a1a2a);
  b.to(r.parts[1]).c(0x0a1a2a).box({0, 0.24f, 0.4f}, v3(0.08f, 0.02f, 0.02f));
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [](float t, Pose& p) { float s = sinf(t * TAU); p.rootS = {1 + s * 0.05f, 1 - s * 0.06f, 1 + s * 0.05f}; }},
    {CL_MOVE, 0.6f, true, [](float t, Pose& p) {
      float h = sinf(t * PI); float sq_ = t < 0.15f ? 1 - t / 0.15f : (t > 0.85f ? (t - 0.85f) / 0.15f : 0);
      p.rootT.y = h * 0.45f; p.rootS = {1 + sq_ * 0.25f - h * 0.08f, 1 - sq_ * 0.3f + h * 0.15f, 1 + sq_ * 0.25f - h * 0.08f};
    }},
    {CL_ATTACK, 0.5f, false, [](float t, Pose& p) { float k = sinf(t * PI); p.rootS = {1 + k * 0.3f, 1 - k * 0.2f, 1 + k * 0.4f}; p.rootT.z = k * 0.3f; }},
  };
  bakeModel(M_SLIME, r, cl);
}
static void buildBee() {
  Rig r(4); MB b;
  for (int i = 1; i < 4; i++) r.parts[i].parent = 0;
  r.parts[1].pivot = {0.12f, 0.75f, 0}; r.parts[2].pivot = {-0.12f, 0.75f, 0};
  Part& B = r.parts[0];
  b.to(B).c(0xffc61a).l(L_PLAIN).sphere({0, 0.6f, -0.05f}, {0.22f, 0.22f, 0.3f}, 8, 6);
  b.c(0x1a1410).box({0, 0.6f, -0.05f}, v3(0.225f, 0.06f, 0.06f)).box({0, 0.6f, -0.22f}, v3(0.19f, 0.05f, 0.05f));
  b.c(0x1a1410).sphere({0, 0.68f, 0.26f}, {0.16f, 0.15f, 0.15f}, 7, 5);
  b.c(0x1a1410).cone({0, 0.6f, -0.4f}, 0.06f, 0.15f, 4);
  addEyes(b, B, {0, 0.71f, 0.39f}, 0.07f, 0.04f, 0xff3030, true);
  b.to(B).c(0x1a1410).limb({0.05f, 0.8f, 0.32f}, {0.12f, 0.98f, 0.36f}, 0.012f, 0.01f, 3).limb({-0.05f, 0.8f, 0.32f}, {-0.12f, 0.98f, 0.36f}, 0.012f, 0.01f, 3);
  b.to(r.parts[1]).c(0xe8f4ff).e(40).dquad({0.12f, 0.76f, 0.1f}, {0.55f, 0.82f, 0.05f}, {0.5f, 0.84f, -0.2f}, {0.12f, 0.78f, -0.1f});
  b.to(r.parts[2]).c(0xe8f4ff).e(40).dquad({-0.12f, 0.78f, -0.1f}, {-0.5f, 0.84f, -0.2f}, {-0.55f, 0.82f, 0.05f}, {-0.12f, 0.76f, 0.1f});
  auto fly = [](float t, Pose& p, float amp) { float w = sinf(t * TAU * 4); p.p[1].r.z = w * 0.9f; p.p[2].r.z = -w * 0.9f; p.rootT.y = sinf(t * TAU) * 0.08f * amp; };
  std::vector<ClipDef> cl = {
    {CL_IDLE, 0.5f, true, [=](float t, Pose& p) { fly(t, p, 1); }},
    {CL_MOVE, 0.5f, true, [=](float t, Pose& p) { fly(t, p, 1); p.rootR.x = 0.2f; }},
    {CL_ATTACK, 0.5f, false, [=](float t, Pose& p) { fly(t, p, 0); float k = sinf(t * PI); p.rootR.x = 0.2f + k * 0.9f; p.rootT.z = k * 0.4f; }},
  };
  bakeModel(M_BEE, r, cl);
}
static void buildBoomer() {
  MB b; HSpec s; s.leg = 0.4f; s.torso = 0.5f; s.head = 0.2f; s.shoulder = 0.7f; s.hip = 0.4f; s.arm = 0.4f; s.limbR = 0.07f; s.torsoW = 0.5f; s.torsoD = 0.5f;
  s.skin = 0xa0b45a; s.top = 0xa0b45a; s.bottom = 0x6a5a3a; s.feet = 0x5a4a2a; s.skinL = L_SKIN; s.topL = L_SKIN;
  HJ J; Rig r = humanoid(s, J, b);
  r.parts[P_TORSO].v.clear(); r.parts[P_TORSO].idx.clear();
  b.to(r.parts[P_TORSO]).c(0xa8bc5a).l(L_SKIN).sphere(J.hip + v3(0, 0.3f, 0.05f), {0.52f, 0.48f, 0.5f}, 10, 8);
  b.c(0xff6a20).e(255);
  for (int i = 0; i < 6; i++) { float a = i * 1.05f; b.box(J.hip + v3(cosf(a) * 0.4f, 0.3f + sinf(a * 2) * 0.2f, sinf(a) * 0.4f + 0.05f), v3(0.07f, 0.03f, 0.07f)); }
  b.e(0);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0xa0b45a).l(L_SKIN).sphere(J.headC + v3(0, -0.05f, 0.05f), {0.2f, 0.18f, 0.2f}, 7, 5);
  addEyes(b, H, J.headC + v3(0, -0.02f, 0.22f), 0.07f, 0.03f, 0xff4020, true);
  std::vector<ClipDef> cl = humanClips(0.6f, 0.7f, 1, false);
  cl.push_back({CL_ATTACK, 0.8f, false, [](float t, Pose& p) { float k = smooth01(t); float jit = sinf(t * 60) * 0.03f * k; p.p[P_TORSO].s = {1 + k * 0.45f + jit, 1 + k * 0.35f, 1 + k * 0.45f - jit}; p.p[P_ARM_L].r.z = 1.0f * k; p.p[P_ARM_R].r.z = -1.0f * k; }});
  bakeModel(M_BOOMER, r, cl);
}
static void buildGolem(int id, float k, u32 col, u32 moss, int variant) {
  // variant 0 small golem, 1 stone golem miniboss, 2 sand golem miniboss
  MB b; HSpec s; s.leg = 0.55f * k; s.torso = 0.6f * k; s.head = 0.22f * k; s.shoulder = 0.85f * k; s.hip = 0.4f * k; s.arm = 0.85f * k; s.limbR = 0.13f * k; s.torsoW = 0.7f * k; s.torsoD = 0.45f * k; s.hunch = 0.25f;
  s.skin = col; s.top = col; s.bottom = col; s.feet = col; s.skinL = L_ROCK; s.topL = L_ROCK; s.botL = L_ROCK;
  HJ J; Rig r = humanoid(s, J, b);
  b.to(r.parts[P_TORSO]).c(col).l(L_ROCK).ico(J.hip + v3(0, 0.38f, 0.1f) * k, v3(0.5f, 0.42f, 0.38f) * k, 0, 0.15f, id);
  b.c(moss).l(L_LEAVES).ico(J.hip + v3(0.1f, 0.7f, 0.0f) * k, v3(0.3f, 0.16f, 0.26f) * k, 0, 0.2f, id + 3);
  b.to(r.parts[P_FORE_L]).c(col).l(L_ROCK).ico(J.haL + v3(0, -0.05f, 0) * k, v3(0.22f, 0.22f, 0.22f) * k, 0, 0.2f, id + 5);
  b.to(r.parts[P_FORE_R]).c(col).l(L_ROCK).ico(J.haR + v3(0, -0.05f, 0) * k, v3(0.22f, 0.22f, 0.22f) * k, 0, 0.2f, id + 6);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(col).l(L_ROCK).ico(J.headC, v3(0.22f, 0.2f, 0.22f) * k, 0, 0.15f, id + 7);
  addEyes(b, H, J.headC + v3(0, 0.02f, 0.2f) * k, 0.08f * k, 0.035f * k, variant == 2 ? 0xffb030 : 0x60f0ff, true);
  if (variant == 2) { b.to(r.parts[P_TORSO]).c(0xd0a050).l(L_SAND).ico(J.hip + v3(0, 0.95f, -0.1f) * k, v3(0.25f, 0.12f, 0.2f) * k, 0, 0.3f, 9); }
  bakeModel(id, r, humanClips(0.8f, 0.7f, 1));
}
static void buildGhoul() {
  MB b; HSpec s; s.leg = 0.62f; s.torso = 0.46f; s.head = 0.22f; s.shoulder = 0.4f; s.arm = 0.85f; s.limbR = 0.05f; s.hunch = 0.55f;
  s.skin = 0x8a8aa8; s.top = 0x8a8aa8; s.bottom = 0x3a3448; s.feet = 0x6a6a88; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x8a8aa8).l(L_SKIN).sphere(J.headC, {0.18f, 0.2f, 0.2f}, 7, 5);
  b.c(0x2a1a2a).box(J.headC + v3(0, -0.1f, 0.16f), v3(0.1f, 0.04f, 0.03f));
  addEyes(b, H, J.headC + v3(0, 0.03f, 0.17f), 0.07f, 0.03f, 0xff5050, true);
  b.to(r.parts[P_FORE_L]).c(0xd8d0c0).l(L_BONE).cone(J.haL + v3(0, -0.12f, 0.03f), 0.03f, -0.1f, 3);
  bakeModel(M_GHOUL, r, humanClips(0.42f, 1.2f, 0));
}
static void buildMummy(int id, float k, int variant) {
  MB b; HSpec s; s.leg = 0.78f * k; s.torso = 0.52f * k; s.head = 0.24f * k; s.shoulder = 0.44f * k * (variant == 1 ? 1.35f : 1); s.arm = 0.64f * k; s.limbR = 0.07f * k * (variant == 1 ? 1.4f : 1);
  s.torsoW = 0.36f * k * (variant == 1 ? 1.4f : 1); s.torsoD = 0.22f * k * (variant == 1 ? 1.5f : 1); s.hunch = 0.1f;
  u32 wrap = variant == 2 ? 0xb8a070 : 0xe6dcc0;
  s.skin = wrap; s.top = wrap; s.bottom = wrap; s.feet = wrap; s.skinL = L_WRAPS; s.topL = L_WRAPS; s.botL = L_WRAPS;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(wrap).l(L_WRAPS).sphere(J.headC, v3(0.2f, 0.22f, 0.2f) * k, 8, 6);
  addEyes(b, H, J.headC + v3(0, 0.02f, 0.18f) * k, 0.07f * k, 0.03f * k, variant == 2 ? 0x40ff80 : 0xffd040, true);
  if (variant == 2) { // ancient: gold headdress
    b.to(H).c(0xf0c040).l(L_GOLD).e(40).boxT(J.headC + v3(0, 0.08f, -0.05f) * k, v3(0.24f, 0.18f, 0.2f) * k, v3(0.2f, 0.18f, 0.12f) * k);
    b.c(0x2050c0).box(J.headC + v3(0, 0.08f, 0.15f) * k, v3(0.2f, 0.03f, 0.02f) * k); b.e(0);
  }
  b.to(r.parts[P_TORSO]).c(mulc(hexc(wrap), 0.75f).r << 16).cc(mulc(hexc(wrap), 0.75f)).box(J.hip + v3(0.05f, 0.3f, 0.12f) * k, v3(0.1f, 0.02f, 0.02f) * k);
  bakeModel(id, r, humanClips(0.8f, 0.8f, 3));
}
static void buildZombie() {
  MB b; HSpec s; s.leg = 0.76f; s.torso = 0.52f; s.head = 0.25f; s.shoulder = 0.44f; s.arm = 0.66f; s.limbR = 0.075f; s.hunch = 0.15f;
  s.skin = 0x8aa07a; s.top = 0x5a4a6a; s.bottom = 0x3a3a4a; s.feet = 0x2a2a2a;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x8aa07a).l(L_SKIN).boxT(J.headC, v3(0.18f, 0.2f, 0.18f), v3(0.19f, 0.2f, 0.19f));
  b.c(0x3a1a1a).box(J.headC + v3(0.02f, -0.1f, 0.18f), v3(0.1f, 0.03f, 0.02f));
  addEyes(b, H, J.headC + v3(0, 0.04f, 0.18f), 0.08f, 0.035f, 0xfff080, true);
  b.to(r.parts[P_TORSO]).c(0x8aa07a).l(L_SKIN).box(J.hip + v3(0.08f, 0.2f, 0.12f), v3(0.06f, 0.08f, 0.02f));
  bakeModel(M_ZOMBIE, r, humanClips(0.85f, 0.8f, 3));
}
static void buildTumblebone() {
  Rig r(1); MB b; b.to(r.parts[0]).c(0xece2c8).l(L_BONE);
  Rng rg(55);
  for (int i = 0; i < 14; i++) { v3 d = rg.onSphere(); v3 c0 = v3(0, 0.55f, 0) + d * 0.32f; v3 e = norm(cross(d, rg.onSphere())); b.limb(c0 - e * 0.28f, c0 + e * 0.28f, 0.045f, 0.045f, 4); }
  b.c(0xece2c8).sphere({0, 0.55f, 0}, {0.3f, 0.3f, 0.3f}, 7, 5);
  b.c(0xf0e8d0).sphere({0, 0.55f, 0.3f}, {0.15f, 0.14f, 0.12f}, 6, 4);
  addEyes(b, r.parts[0], {0, 0.58f, 0.42f}, 0.06f, 0.03f, 0x200a0a);
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [](float t, Pose& p) { p.rootT.y = 0; }},
    {CL_MOVE, 0.5f, true, [](float t, Pose& p) { p.rootT = {0, 0.55f, 0}; p.rootR.x = t * TAU; p.rootT = p.rootT - v3(0, 0, 0); p.rootT.y = 0; }},
  };
  // roll around the ball centre
  cl[1].fn = [](float t, Pose& p) { p.p[0].r.x = t * TAU; };
  r.parts[0].pivot = {0, 0.55f, 0};
  bakeModel(M_TUMBLEBONE, r, cl);
}
static void buildWisp() {
  Rig r(1); MB b; r.parts[0].pivot = {0, 0.9f, 0};
  b.to(r.parts[0]).c(0x40d8ff).l(L_PLAIN).e(255).sphere({0, 0.85f, 0}, {0.25f, 0.25f, 0.25f}, 8, 6);
  b.c(0x9af0ff).cone({0, 0.95f, 0}, 0.2f, 0.5f, 6);
  b.c(0x2080ff).e(200).cone({0, 0.55f, 0}, 0.12f, 0.35f, 5);
  b.e(0); addEyes(b, r.parts[0], {0, 0.88f, 0.23f}, 0.07f, 0.035f, 0x0a1830);
  std::vector<ClipDef> cl = {
    {CL_IDLE, 0.6f, true, [](float t, Pose& p) { float s = sinf(t * TAU * 2); p.p[0].s = {1 + s * 0.08f, 1 - s * 0.1f, 1 + s * 0.08f}; p.rootT.y = sinf(t * TAU) * 0.1f; }},
    {CL_MOVE, 0.6f, true, [](float t, Pose& p) { float s = sinf(t * TAU * 2); p.p[0].s = {1 + s * 0.08f, 1 - s * 0.1f, 1 + s * 0.08f}; p.rootR.x = 0.25f; p.rootT.y = sinf(t * TAU) * 0.1f; }},
    {CL_ATTACK, 0.5f, false, [](float t, Pose& p) { float k = sinf(t * PI); p.p[0].s = {1 + k * 0.4f, 1 + k * 0.4f, 1 + k * 0.4f}; }},
  };
  bakeModel(M_WISP, r, cl);
}
static void buildCactusShooter() {
  Rig r(4); MB b;
  r.parts[1].parent = 0; r.parts[1].pivot = {0.3f, 0.75f, 0}; r.parts[2].parent = 0; r.parts[2].pivot = {-0.3f, 0.6f, 0}; r.parts[3].parent = 0; r.parts[3].pivot = {0, 0.1f, 0};
  b.to(r.parts[0]).c(0x4a9a3a).l(L_CACTUS).cyl({0, 0.05f, 0}, 0.3f, 0.28f, 1.1f, 8, false).sphere({0, 1.15f, 0}, {0.28f, 0.2f, 0.28f}, 8, 4);
  b.c(0xff5fa2).e(60).sphere({0.05f, 1.33f, 0}, {0.08f, 0.06f, 0.08f}, 5, 3); b.e(0);
  addEyes(b, r.parts[0], {0, 0.9f, 0.29f}, 0.1f, 0.045f, 0x101010);
  b.to(r.parts[0]).c(0x2a1010).box({0, 0.7f, 0.29f}, v3(0.09f, 0.05f, 0.02f));
  b.to(r.parts[1]).c(0x4a9a3a).l(L_CACTUS).limb({0.28f, 0.7f, 0}, {0.55f, 0.75f, 0}, 0.11f, 0.1f, 6).limb({0.55f, 0.7f, 0}, {0.55f, 1.05f, 0}, 0.1f, 0.09f, 6);
  b.to(r.parts[2]).c(0x4a9a3a).l(L_CACTUS).limb({-0.28f, 0.55f, 0}, {-0.52f, 0.6f, 0}, 0.1f, 0.09f, 6).limb({-0.52f, 0.55f, 0}, {-0.52f, 0.85f, 0}, 0.09f, 0.08f, 6);
  b.to(r.parts[3]).c(0xb88a50).l(L_DIRT).cyl({0, 0, 0}, 0.38f, 0.32f, 0.12f, 8, true);
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [](float t, Pose& p) { float s = sinf(t * TAU); p.p[1].r.z = s * 0.1f; p.p[2].r.z = -s * 0.1f; p.rootS.y = 1 + s * 0.02f; }},
    {CL_MOVE, 0.6f, true, [](float t, Pose& p) { float s = sinf(t * TAU); p.rootR.z = s * 0.12f; p.rootT.y = fabsf(s) * 0.12f; p.p[1].r.z = s * 0.3f; p.p[2].r.z = s * 0.3f; }},
    {CL_ATTACK, 0.6f, false, [](float t, Pose& p) { float k = t < 0.5f ? smooth01(t * 2) : 1 - smooth01((t - 0.5f) * 2); p.rootR.x = -0.35f * k; p.rootS = {1 + k * 0.1f, 1 - k * 0.08f, 1 + k * 0.1f}; p.p[1].r.z = -0.6f * k; p.p[2].r.z = 0.6f * k; }},
  };
  bakeModel(M_CACTUS_SHOOTER, r, cl);
}
static void buildScorpion(int id, float k, u32 col, u32 col2, bool crown) {
  // parts: 0 body, 1-6 legs, 7 claw L, 8 claw R, 9 tail
  Rig r(10); MB b;
  for (int i = 1; i < 10; i++) r.parts[i].parent = 0;
  r.parts[0].pivot = v3(0, 0.3f, 0) * k;
  b.to(r.parts[0]).c(col).l(L_CHITIN).sphere(v3(0, 0.32f, 0) * k, v3(0.32f, 0.16f, 0.45f) * k, 8, 5);
  b.sphere(v3(0, 0.34f, 0.42f) * k, v3(0.2f, 0.12f, 0.16f) * k, 7, 4);
  addEyes(b, r.parts[0], v3(0, 0.43f, 0.52f) * k, 0.07f * k, 0.03f * k, 0xff2020, true);
  for (int i = 0; i < 6; i++) {
    int side = i < 3 ? 1 : -1; float z = (0.2f - (i % 3) * 0.2f) * k;
    Part& L = r.parts[1 + i]; L.pivot = v3(side * 0.25f * k, 0.3f * k, z);
    b.to(L).c(col2).l(L_CHITIN).limb(v3(side * 0.25f * k, 0.3f * k, z), v3(side * 0.55f * k, 0.38f * k, z * 1.2f), 0.04f * k, 0.035f * k, 4).limb(v3(side * 0.55f * k, 0.38f * k, z * 1.2f), v3(side * 0.7f * k, 0.02f, z * 1.3f), 0.035f * k, 0.02f * k, 4);
  }
  for (int side = -1; side <= 1; side += 2) {
    Part& C = r.parts[side < 0 ? 7 : 8]; C.pivot = v3(side * 0.2f * k, 0.32f * k, 0.5f * k);
    b.to(C).c(col2).l(L_CHITIN).limb(v3(side * 0.2f * k, 0.32f * k, 0.5f * k), v3(side * 0.42f * k, 0.32f * k, 0.8f * k), 0.05f * k, 0.05f * k, 5);
    b.c(col).sphere(v3(side * 0.42f * k, 0.32f * k, 0.95f * k), v3(0.12f, 0.08f, 0.16f) * k, 6, 4);
  }
  Part& T = r.parts[9]; T.pivot = v3(0, 0.35f, -0.4f) * k;
  v3 p0 = v3(0, 0.35f, -0.4f) * k;
  v3 segs[5] = {v3(0, 0.45f, -0.7f) * k, v3(0, 0.75f, -0.85f) * k, v3(0, 1.05f, -0.7f) * k, v3(0, 1.15f, -0.45f) * k, v3(0, 1.05f, -0.25f) * k};
  b.to(T).c(col).l(L_CHITIN);
  v3 prev = p0; for (int i = 0; i < 4; i++) { b.limb(prev, segs[i], (0.1f - i * 0.012f) * k, (0.09f - i * 0.012f) * k, 6); b.sphere(segs[i], v3(0.09f, 0.09f, 0.09f) * k * (1 - i * 0.1f), 5, 3); prev = segs[i]; }
  b.c(0xff4060).e(120).limb(segs[3], segs[4], 0.06f * k, 0.005f, 4); b.e(0);
  if (crown) { b.to(r.parts[0]).c(0xffcc33).l(L_GOLD).e(60); for (int i = 0; i < 4; i++) b.cone(v3((i - 1.5f) * 0.09f, 0.42f, 0.42f) * k, 0.04f * k, 0.16f * k, 4); b.e(0); }
  auto walk = [](float t, Pose& p) {
    for (int i = 0; i < 6; i++) { float ph = t * TAU + (i % 2) * PI + (i >= 3 ? PI : 0); p.p[1 + i].r.y = sinf(ph) * 0.35f; p.p[1 + i].r.z = std::max(0.f, cosf(ph)) * 0.25f * (i < 3 ? -1 : 1); }
    p.p[9].r.x = sinf(t * TAU) * 0.08f; p.p[7].r.y = sinf(t * TAU) * 0.15f; p.p[8].r.y = -sinf(t * TAU) * 0.15f;
  };
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [](float t, Pose& p) { p.p[9].r.x = sinf(t * TAU) * 0.06f; p.p[7].r.y = sinf(t * TAU) * 0.1f; p.p[8].r.y = -sinf(t * TAU) * 0.1f; }},
    {CL_MOVE, 0.45f, true, walk},
    {CL_ATTACK, 0.6f, false, [](float t, Pose& p) { float k2 = t < 0.45f ? smooth01(t / 0.45f) : 1 - smooth01((t - 0.45f) / 0.55f); float st = t > 0.4f && t < 0.7f ? sinf((t - 0.4f) / 0.3f * PI) : 0; p.p[9].r.x = -0.3f * k2 + 0.9f * st; p.p[7].r.y = -0.5f * k2; p.p[8].r.y = 0.5f * k2; }},
  };
  bakeModel(id, r, cl);
}
static void buildGhost(int id, float k, u32 col, int variant) {
  // variant 0 ghost, 1 greater, 2 purple, 3 red, 4 swarm, 5 ghostham(miniboss: hat), 6 ghost king (crown+cape)
  Rig r(4); MB b;
  r.parts[0].pivot = v3(0, 1.0f, 0) * k; r.parts[1].parent = 0; r.parts[1].pivot = v3(0.3f, 1.05f, 0) * k; r.parts[2].parent = 0; r.parts[2].pivot = v3(-0.3f, 1.05f, 0) * k; r.parts[3].parent = 0; r.parts[3].pivot = v3(0, 0.75f, 0) * k;
  u8 em = variant >= 2 ? 90 : 60;
  b.to(r.parts[0]).c(col).l(L_PLAIN).e(em).sphere(v3(0, 1.05f, 0) * k, v3(0.34f, 0.36f, 0.34f) * k, 9, 6);
  b.to(r.parts[3]).c(col).l(L_PLAIN).e(em).limb(v3(0, 0.95f, 0) * k, v3(0, 0.3f, -0.15f) * k, 0.33f * k, 0.03f, 8, false);
  b.to(r.parts[1]).c(col).e(em).limb(v3(0.28f, 1.05f, 0.05f) * k, v3(0.48f, 0.85f, 0.2f) * k, 0.07f * k, 0.04f * k, 4);
  b.to(r.parts[2]).c(col).e(em).limb(v3(-0.28f, 1.05f, 0.05f) * k, v3(-0.48f, 0.85f, 0.2f) * k, 0.07f * k, 0.04f * k, 4);
  u32 eye = variant == 3 ? 0xffe040 : variant == 6 ? 0xffe040 : 0x101018;
  addEyes(b, r.parts[0], v3(0, 1.1f, 0.31f) * k, 0.11f * k, 0.05f * k, eye, variant >= 3);
  b.to(r.parts[0]).c(0x101018).e(0).sphere(v3(0, 0.95f, 0.31f) * k, v3(0.07f, 0.09f, 0.04f) * k, 5, 3);
  if (variant == 5) { b.to(r.parts[0]).c(0x2a2030).l(L_CLOTH).cyl(v3(0, 1.32f, 0) * k, 0.42f * k, 0.42f * k, 0.04f * k, 10, true).cyl(v3(0, 1.36f, 0) * k, 0.22f * k, 0.2f * k, 0.35f * k, 8, true); b.c(0xc04040).cyl(v3(0, 1.38f, 0) * k, 0.225f * k, 0.225f * k, 0.06f * k, 8, false); }
  if (variant == 6) {
    b.to(r.parts[0]).c(0xffcc33).l(L_GOLD).e(120);
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6; b.cone(v3(cosf(a) * 0.22f, 1.38f, sinf(a) * 0.22f) * k, 0.06f * k, 0.18f * k, 4); }
    b.cyl(v3(0, 1.33f, 0) * k, 0.25f * k, 0.25f * k, 0.08f * k, 12, false);
    b.c(0x6a1a8a).l(L_CLOTH).e(30).boxT(v3(0, 0.8f, -0.3f) * k, v3(0.5f, 0.45f, 0.04f) * k, v3(0.35f, 0.45f, 0.04f) * k);
    b.e(0);
  }
  auto fl = [](float t, Pose& p, float lunge) { float s = sinf(t * TAU); p.rootT.y = s * 0.1f; p.p[3].r.x = sinf(t * TAU + 1) * 0.25f; p.p[3].r.z = cosf(t * TAU) * 0.15f; p.p[1].r.z = -0.2f + s * 0.25f; p.p[2].r.z = 0.2f - s * 0.25f; };
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [=](float t, Pose& p) { fl(t, p, 0); }},
    {CL_MOVE, 0.8f, true, [=](float t, Pose& p) { fl(t, p, 0); p.rootR.x = 0.25f; }},
    {CL_ATTACK, 0.5f, false, [=](float t, Pose& p) { fl(t, p, 1); float kk = sinf(t * PI); p.rootR.x = 0.25f + kk * 0.4f; p.rootT.z = kk * 0.4f; p.p[1].r.x = -1.4f * kk; p.p[2].r.x = -1.4f * kk; }},
  };
  bakeModel(id, r, cl);
}
static void buildHead() {
  Rig r(1); MB b; r.parts[0].pivot = {0, 1.0f, 0};
  b.to(r.parts[0]).c(0xc8c0a8).l(L_SKIN).sphere({0, 1.0f, 0}, {0.33f, 0.36f, 0.33f}, 9, 7);
  b.c(0x2a1a1a).box({0, 0.82f, 0.29f}, v3(0.14f, 0.06f, 0.04f));
  addEyes(b, r.parts[0], {0, 1.06f, 0.29f}, 0.12f, 0.06f, 0x60ff80, true);
  b.to(r.parts[0]).c(0x60ff80).e(200).cone({0, 0.55f, -0.1f}, 0.15f, 0.3f, 6); b.e(0);
  b.c(0x3a3020).l(L_FUR).sphere({0, 1.2f, -0.05f}, {0.3f, 0.2f, 0.3f}, 7, 4, 0.5f);
  std::vector<ClipDef> cl = {
    {CL_IDLE, 1.0f, true, [](float t, Pose& p) { p.rootT.y = sinf(t * TAU) * 0.12f; p.p[0].r.z = sinf(t * TAU) * 0.15f; }},
    {CL_MOVE, 0.7f, true, [](float t, Pose& p) { p.rootT.y = sinf(t * TAU) * 0.12f; p.p[0].r.y = sinf(t * TAU) * 0.3f; p.rootR.x = 0.2f; }},
    {CL_ATTACK, 0.5f, false, [](float t, Pose& p) { float k = sinf(t * PI); p.rootT.z = k * 0.5f; p.p[0].s = {1 + k * 0.2f, 1 + k * 0.2f, 1 + k * 0.2f}; }},
  };
  bakeModel(M_HEAD, r, cl);
}
static void buildChunkham() {
  // big pig brute
  MB b; HSpec s; float k = 1.8f; s.leg = 0.5f * k; s.torso = 0.55f * k; s.head = 0.3f * k; s.shoulder = 0.85f * k; s.hip = 0.45f * k; s.arm = 0.6f * k; s.limbR = 0.12f * k; s.torsoW = 0.7f * k; s.torsoD = 0.55f * k; s.hunch = 0.3f;
  s.skin = 0xe89aa0; s.top = 0x7a3a2a; s.bottom = 0x5a3020; s.feet = 0x3a2020; s.skinL = L_SKIN;
  HJ J; Rig r = humanoid(s, J, b);
  b.to(r.parts[P_TORSO]).c(0xe89aa0).l(L_SKIN).sphere(J.hip + v3(0, 0.3f, 0.12f) * k, v3(0.42f, 0.36f, 0.36f) * k, 9, 7);
  b.c(0xd02020).l(L_PLAIN).e(80).box(J.hip + v3(0, 0.35f, 0.45f) * k, v3(0.25f, 0.03f, 0.02f) * k).box(J.hip + v3(0, 0.25f, 0.46f) * k, v3(0.2f, 0.03f, 0.02f) * k); b.e(0);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0xe89aa0).l(L_SKIN).sphere(J.headC, v3(0.3f, 0.27f, 0.3f) * k, 8, 6);
  b.c(0xd88088).cyl(J.headC + v3(0, -0.04f, 0.25f) * k, 0.12f * k, 0.12f * k, 0.01f, 8, true);
  b.X(m4::T(J.headC + v3(0, -0.04f, 0.27f) * k) * m4::RX(PI / 2)).cyl({0, 0, 0}, 0.12f * k, 0.11f * k, 0.1f * k, 8, true).noX();
  b.c(0x501018).box(J.headC + v3(0.04f, -0.04f, 0.38f) * k, v3(0.02f, 0.03f, 0.01f) * k).box(J.headC + v3(-0.04f, -0.04f, 0.38f) * k, v3(0.02f, 0.03f, 0.01f) * k);
  b.c(0xf0ead8).l(L_BONE).limb(J.headC + v3(0.1f, -0.12f, 0.25f) * k, J.headC + v3(0.17f, 0.05f, 0.35f) * k, 0.03f * k, 0.005f, 4).limb(J.headC + v3(-0.1f, -0.12f, 0.25f) * k, J.headC + v3(-0.17f, 0.05f, 0.35f) * k, 0.03f * k, 0.005f, 4);
  addEyes(b, H, J.headC + v3(0, 0.07f, 0.27f) * k, 0.1f * k, 0.03f * k, 0xff2020, true);
  b.to(H).c(0xe89aa0).l(L_SKIN).cone(J.headC + v3(0.2f, 0.18f, 0) * k, 0.08f * k, 0.15f * k, 4).cone(J.headC + v3(-0.2f, 0.18f, 0) * k, 0.08f * k, 0.15f * k, 4);
  b.to(r.parts[P_PROP]).c(0x5a4030).l(L_WOOD).limb(J.haR + v3(0, -0.07f, -0.1f) * k, J.haR + v3(0, -0.07f, 0.6f) * k, 0.06f * k, 0.12f * k, 6);
  auto clips = humanClips(0.75f, 0.8f, 1);
  clips.push_back({CL_SPECIAL, 0.6f, true, [](float t, Pose& p) { float s = sinf(t * TAU); p.p[P_THIGH_L].r.x = -s * 1.0f; p.p[P_THIGH_R].r.x = s * 1.0f; p.p[P_SHIN_L].r.x = p.p[P_SHIN_R].r.x = 0.6f; p.p[P_TORSO].r.x = 0.6f; p.p[P_ARM_L].r.x = p.p[P_ARM_R].r.x = 0.8f; p.rootT.y = fabsf(cosf(t * TAU)) * 0.15f; }});
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -0.6f; }; }
  bakeModel(M_MB_CHUNKHAM, r, clips);
}
static void buildEnt(int id, float k, u32 bark, u32 leaf, int variant) {
  // variant 0 Lil Bark (stump, stationary), 1 Chadbark (buff tree), 2 Bark Vader (dark, helmet)
  MB b; HSpec s; s.leg = 0.6f * k; s.torso = 0.9f * k; s.head = 0.35f * k; s.shoulder = 1.0f * k; s.hip = 0.5f * k; s.arm = 0.95f * k; s.limbR = 0.14f * k; s.torsoW = 0.8f * k; s.torsoD = 0.6f * k;
  s.skin = bark; s.top = bark; s.bottom = bark; s.feet = bark; s.skinL = L_BARK; s.topL = L_BARK; s.botL = L_BARK;
  HJ J; Rig r = humanoid(s, J, b);
  u32 eye = variant == 2 ? 0xff2020 : 0xfff060;
  Part& H = r.parts[P_HEAD];
  if (variant == 0) {
    for (int i : {P_THIGH_L, P_THIGH_R, P_SHIN_L, P_SHIN_R}) { r.parts[i].v.clear(); r.parts[i].idx.clear(); }
    b.to(r.parts[P_HIPS]).c(bark).l(L_BARK);
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6; b.limb(v3(cosf(a) * 0.3f, 0.5f, sinf(a) * 0.3f) * k, v3(cosf(a) * 0.9f, 0.02f, sinf(a) * 0.9f) * k, 0.14f * k, 0.05f * k, 5); }
    b.cyl(v3(0, 0, 0), 0.6f * k, 0.5f * k, s.leg + 0.1f * k, 10, false);
  }
  b.to(r.parts[P_TORSO]).c(bark).l(L_BARK).cyl(J.hip + v3(0, -0.05f, 0), 0.45f * k, 0.5f * k, s.torso, 10, false);
  b.to(H).c(bark).l(L_BARK).cyl(J.neck, 0.48f * k, 0.42f * k, 0.5f * k, 10, true);
  addEyes(b, H, J.neck + v3(0, 0.32f * k, 0.44f * k), 0.16f * k, 0.06f * k, eye, true);
  b.to(H).c(0x1a1008).box(J.neck + v3(0, 0.12f * k, 0.45f * k), v3(0.18f, 0.05f, 0.03f) * k);
  if (variant == 1) { b.to(H).c(bark).l(L_BARK).boxT(J.neck + v3(0, 0.02f, 0.38f) * k, v3(0.3f, 0.1f, 0.12f) * k, v3(0.25f, 0.1f, 0.1f) * k); } // chad jaw
  if (variant == 2) {
    b.to(H).c(0x1a1620).l(L_METAL).sphere(J.neck + v3(0, 0.45f, 0) * k, v3(0.52f, 0.4f, 0.52f) * k, 9, 5, 0.42f);
    b.c(0x1a1620).boxT(J.neck + v3(0, 0.1f, 0.4f) * k, v3(0.3f, 0.12f, 0.1f) * k, v3(0.36f, 0.12f, 0.12f) * k);
    b.to(r.parts[P_EXTRA]).c(0x1a1018).l(L_CLOTH).boxT(J.neck + v3(0, -0.6f, -0.45f) * k, v3(0.75f, 0.65f, 0.04f) * k, v3(0.55f, 0.65f, 0.04f) * k);
    b.to(r.parts[P_PROP]).c(0xff3030).l(L_PLAIN).e(255).limb(J.haR + v3(0, -0.1f, 0) * k, J.haR + v3(0, -0.1f, 1.3f) * k, 0.05f * k, 0.05f * k, 6); b.e(0);
    b.c(0x2a2a2a).l(L_METAL).limb(J.haR + v3(0, -0.1f, -0.2f) * k, J.haR + v3(0, -0.1f, 0.05f) * k, 0.06f * k, 0.06f * k, 6);
  }
  // canopy
  b.to(H).c(leaf).l(L_LEAVES);
  int nb = variant == 0 ? 3 : 4;
  for (int i = 0; i < nb; i++) { float a = i * TAU / nb + 0.4f; b.ico(J.neck + v3(cosf(a) * 0.45f, 0.7f + (i & 1) * 0.15f, sinf(a) * 0.45f - 0.1f) * k, v3(0.5f, 0.4f, 0.5f) * k, 1, 0.18f, id + i); }
  b.ico(J.neck + v3(0, 1.0f, -0.1f) * k, v3(0.6f, 0.45f, 0.6f) * k, 1, 0.18f, id + 9);
  // twig fingers
  b.to(r.parts[P_FORE_L]).c(bark).l(L_BARK).limb(J.haL, J.haL + v3(0.15f, -0.35f, 0.1f) * k, 0.06f * k, 0.01f, 4).limb(J.haL, J.haL + v3(-0.05f, -0.4f, 0.15f) * k, 0.06f * k, 0.01f, 4);
  b.to(r.parts[P_FORE_R]).c(bark).l(L_BARK).limb(J.haR, J.haR + v3(-0.15f, -0.35f, 0.1f) * k, 0.06f * k, 0.01f, 4).limb(J.haR, J.haR + v3(0.05f, -0.4f, 0.15f) * k, 0.06f * k, 0.01f, 4);
  auto clips = humanClips(0.9f, variant == 0 ? 0.0f : 0.7f, 1);
  clips.push_back({CL_SPECIAL, 0.9f, false, [](float t, Pose& p) { float k2 = t < 0.5f ? smooth01(t * 2) : 1 - smooth01((t - 0.5f) * 2); p.p[P_ARM_L].r.z = 1.4f * k2; p.p[P_ARM_R].r.z = -1.4f * k2; p.p[P_TORSO].r.x = -0.2f * k2; p.p[P_HEAD].r.x = -0.3f * k2; }});
  bakeModel(id, r, clips);
}
static void buildAnubis(int id, float k, u32 skin, u32 gold, u32 accent, int variant) {
  MB b; HSpec s; s.leg = 0.85f * k; s.torso = 0.62f * k; s.head = 0.26f * k; s.shoulder = 0.58f * k * (variant ? 1.25f : 1); s.arm = 0.72f * k; s.limbR = 0.09f * k * (variant ? 1.25f : 1); s.torsoW = 0.45f * k * (variant ? 1.3f : 1); s.torsoD = 0.28f * k;
  s.skin = skin; s.top = skin; s.bottom = 0xf0e8d8; s.feet = gold; s.botL = L_CLOTH;
  HJ J; Rig r = humanoid(s, J, b);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(skin).l(L_SKIN).sphere(J.headC, v3(0.2f, 0.2f, 0.22f) * k, 8, 6);
  b.X(m4::T(J.headC + v3(0, -0.04f, 0.18f) * k) * m4::RX(PI / 2 - 0.15f)).cyl({0, 0, 0}, 0.1f * k, 0.05f * k, 0.28f * k, 6, true).noX();
  b.c(skin).limb(J.headC + v3(0.1f, 0.12f, -0.02f) * k, J.headC + v3(0.14f, 0.5f, -0.06f) * k, 0.06f * k, 0.01f, 4).limb(J.headC + v3(-0.1f, 0.12f, -0.02f) * k, J.headC + v3(-0.14f, 0.5f, -0.06f) * k, 0.06f * k, 0.01f, 4);
  addEyes(b, H, J.headC + v3(0, 0.05f, 0.18f) * k, 0.08f * k, 0.03f * k, accent, true);
  b.to(H).c(gold).l(L_GOLD).e(30).boxT(J.headC + v3(0, -0.12f, -0.1f) * k, v3(0.26f, 0.2f, 0.12f) * k, v3(0.2f, 0.2f, 0.08f) * k);
  b.c(accent).box(J.headC + v3(0, -0.02f, -0.1f) * k, v3(0.27f, 0.025f, 0.13f) * k); b.e(0);
  b.to(r.parts[P_TORSO]).c(gold).l(L_GOLD).e(30).boxT(J.neck + v3(0, -0.08f, 0) * k, v3(0.32f, 0.07f, 0.18f) * k, v3(0.26f, 0.07f, 0.16f) * k); b.e(0);
  b.to(r.parts[P_HIPS]).c(0xf0e8d8).l(L_CLOTH).boxT(J.hip + v3(0, -0.18f, 0.02f) * k, v3(0.24f, 0.18f, 0.16f) * k, v3(0.2f, 0.18f, 0.14f) * k);
  b.c(gold).l(L_GOLD).box(J.hip + v3(0, 0.0f, 0.0f) * k, v3(0.22f, 0.04f, 0.15f) * k);
  // staff (variant 0), khopesh (1), scales of justice staff (2)
  b.to(r.parts[P_PROP]).c(gold).l(L_GOLD).e(40).limb(J.haR + v3(0, -0.07f, -0.6f) * k, J.haR + v3(0, -0.07f, 0.9f) * k, 0.03f * k, 0.03f * k, 5);
  if (variant == 2) { b.c(accent).e(200).sphere(J.haR + v3(0, -0.07f, 0.95f) * k, v3(0.1f, 0.1f, 0.1f) * k, 6, 4); b.c(gold).e(40).box(J.haR + v3(0, -0.07f, 0.82f) * k, v3(0.3f, 0.015f, 0.015f) * k); }
  else b.c(accent).e(200).ico(J.haR + v3(0, -0.07f, 0.95f) * k, v3(0.09f, 0.09f, 0.09f) * k, 0, 0.1f, 4);
  b.e(0);
  auto clips = humanClips(0.75f, 0.8f, 0);
  clips.push_back({CL_SPECIAL, 1.0f, false, [](float t, Pose& p) { float k2 = t < 0.5f ? smooth01(t * 2) : 1 - smooth01((t - 0.5f) * 2); p.p[P_ARM_R].r.x = -2.8f * k2; p.p[P_ARM_L].r.x = -2.6f * k2; p.p[P_HEAD].r.x = -0.3f * k2; }});
  for (auto& c : clips) { auto f0 = c.fn; c.fn = [f0](float t, Pose& p) { f0(t, p); p.p[P_PROP].r.x = -1.4f; }; }
  bakeModel(id, r, clips);
}
static void buildBigBob() {
  MB b; HSpec s; float k = 2.4f; s.leg = 0.45f * k; s.torso = 0.55f * k; s.head = 0.25f * k; s.shoulder = 0.8f * k; s.hip = 0.5f * k; s.arm = 0.6f * k; s.limbR = 0.12f * k; s.torsoW = 0.8f * k; s.torsoD = 0.7f * k;
  s.skin = 0x8aa47a; s.top = 0x8aa47a; s.bottom = 0x4a3a5a; s.feet = 0x2a2a2a;
  HJ J; Rig r = humanoid(s, J, b);
  b.to(r.parts[P_TORSO]).c(0x8aa47a).l(L_SKIN).sphere(J.hip + v3(0, 0.3f, 0.1f) * k, v3(0.55f, 0.48f, 0.5f) * k, 10, 8);
  b.c(0x2a2a2a).l(L_PLAIN);
  for (int i = 0; i < 5; i++) b.box(J.hip + v3(0.0f, 0.12f + i * 0.08f, 0.58f) * k, v3(0.09f, 0.008f, 0.01f) * k);
  b.box(J.hip + v3(0, 0.28f, 0.585f) * k, v3(0.008f, 0.2f, 0.01f) * k);
  Part& H = r.parts[P_HEAD];
  b.to(H).c(0x8aa47a).l(L_SKIN).sphere(J.headC + v3(0, -0.05f, 0.05f) * k, v3(0.27f, 0.24f, 0.26f) * k, 8, 6);
  addEyes(b, H, J.headC + v3(0, 0.0f, 0.28f) * k, 0.1f * k, 0.04f * k, 0xff4040, true);
  b.to(H).c(0x3a1a1a).box(J.headC + v3(0, -0.15f, 0.28f) * k, v3(0.12f, 0.03f, 0.02f) * k);
  b.c(0x5a5068).l(L_CLOTH).cyl(J.headC + v3(0, 0.12f, 0) * k, 0.22f * k, 0.2f * k, 0.12f * k, 8, true);
  bakeModel(M_B_BIGBOB, r, humanClips(0.9f, 0.7f, 3));
}

// ---------------------------------------------------------------- props
static void buildProps() {
  MB b;
  // flags: 1 = tint A (foliage), 2 = tint B (trunk/rock/wood); base gray 128 => 1.0
  const u32 G = 0x808080;
  { // blob tree
    for (int v = 0; v < 2; v++) {
      Rig r(1); b.to(r.parts[0]); Rng rg(100 + v);
      float h = v == 0 ? 3.2f : 4.2f;
      b.c(G).l(L_BARK).f(2).uv(0.7f);
      b.limb({0, -0.2f, 0}, {0.2f, h * 0.6f, 0.1f}, 0.32f, 0.2f, 7).limb({0.2f, h * 0.6f, 0.1f}, {-0.1f, h, 0}, 0.2f, 0.14f, 6);
      b.limb({0.15f, h * 0.55f, 0.1f}, {1.0f, h * 0.85f, 0.4f}, 0.12f, 0.06f, 5);
      b.c(G).l(L_LEAVES).f(1).uv(0.5f);
      for (int i = 0; i < 4 + v; i++) {
        float a = i * TAU / (4 + v) + rg.f(); float rr = 1.0f + rg.f() * 0.4f;
        b.ico({cosf(a) * 0.9f, h + rg.range(-0.3f, 0.5f), sinf(a) * 0.9f}, {rr * 1.1f, rr * 0.9f, rr * 1.1f}, 1, 0.22f, 200 + i + v * 10);
      }
      b.ico({0, h + 0.9f, 0}, {1.4f, 1.1f, 1.4f}, 1, 0.2f, 300 + v);
      b.f(0).uv(1);
      bakeStatic(v == 0 ? M_TREE_BLOB : M_TREE_BLOB2, r);
    }
  }
  { // pines
    for (int v = 0; v < 2; v++) {
      Rig r(1); b.to(r.parts[0]); float h = v == 0 ? 5.0f : 7.0f;
      b.c(G).l(L_BARK).f(2).cyl({0, -0.2f, 0}, 0.25f, 0.15f, h * 0.5f, 6, false);
      b.c(G).l(L_PINE).f(1).uv(0.6f);
      int tiers = v == 0 ? 3 : 4;
      for (int i = 0; i < tiers; i++) { float y = h * 0.25f + i * h * 0.19f; float rr = (1.7f - i * 0.32f) * (v ? 1.15f : 1); b.cone({0, y, 0}, rr, h * 0.32f, 7); }
      b.f(0).uv(1);
      bakeStatic(v == 0 ? M_PINE : M_PINE2, r);
    }
  }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_ROCK).f(2).uv(0.35f).ico({0, 1.4f, 0}, {3.0f, 2.2f, 2.6f}, 1, 0.28f, 401); b.f(0).uv(1); bakeStatic(M_ROCK_BIG, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_ROCK).f(2).uv(0.5f).ico({0, 0.6f, 0}, {1.3f, 1.0f, 1.1f}, 1, 0.25f, 402); b.f(0).uv(1); bakeStatic(M_ROCK_MED, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_ROCK).f(2).ico({0, 0.2f, 0}, {0.5f, 0.35f, 0.45f}, 0, 0.25f, 403); b.f(0); bakeStatic(M_ROCK_SMALL, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BARK).f(2).limb({-1.6f, 0.35f, 0}, {1.6f, 0.3f, 0.1f}, 0.35f, 0.32f, 7); b.f(0).c(0xc8a878).l(L_WOOD); bakeStatic(M_LOG, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BARK).f(2).cyl({0, -0.1f, 0}, 0.5f, 0.42f, 0.6f, 8, true); b.f(0); bakeStatic(M_STUMP, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xf0e8d8).l(L_PLAIN).cyl({0, 0, 0}, 0.1f, 0.08f, 0.35f, 6, false); b.c(0xd83a3a).sphere({0, 0.38f, 0}, {0.3f, 0.18f, 0.3f}, 8, 4, 0.48f); b.c(0xffffff).box({0.1f, 0.5f, 0.05f}, v3(0.04f, 0.02f, 0.04f)).box({-0.12f, 0.46f, -0.08f}, v3(0.04f, 0.02f, 0.04f)); bakeStatic(M_MUSHROOM, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_LEAVES).f(1).uv(0.6f).ico({0, 0.5f, 0}, {0.9f, 0.7f, 0.9f}, 1, 0.25f, 501).ico({0.6f, 0.35f, 0.3f}, {0.55f, 0.45f, 0.55f}, 0, 0.25f, 502); b.f(0).uv(1); bakeStatic(M_BUSH, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BRICK).f(2).uv(0.5f);
    b.box({0, 1.0f, 0}, {2.5f, 1.0f, 0.35f}); b.box({-1.6f, 2.4f, 0}, {0.9f, 0.4f, 0.35f}); b.box({1.9f, 2.2f, 0}, {0.6f, 0.2f, 0.35f}); b.f(0).uv(1); bakeStatic(M_RUIN_WALL, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_STONE).f(2).uv(0.5f).cyl({0, 0, 0}, 0.5f, 0.45f, 3.0f, 8, true).box({0, 0.15f, 0}, {0.7f, 0.15f, 0.7f}); b.f(0).uv(1); bakeStatic(M_RUIN_PILLAR, r); }
  { // grass tuft: 3 crossed double sided quads
    Rig r(1); b.to(r.parts[0]).c(G).l(L_TUFT).f(1).uv(1);
    for (int i = 0; i < 3; i++) { float a = i * PI / 3; v3 d(cosf(a) * 0.55f, 0, sinf(a) * 0.55f); b.dquad(v3(0, 0, 0) - d, v3(0, 0, 0) + d, v3(0, 0.75f, 0) + d, v3(0, 0.75f, 0) - d); }
    for (auto& v : r.parts[0].v) v.n = v3(0, 1, 0);
    b.f(0); bakeStatic(M_GRASS, r);
  }
  { Rig r(1); b.to(r.parts[0]).c(0xffffff).l(L_FLOWER).f(0);
    for (int i = 0; i < 2; i++) { float a = i * PI / 2 + 0.3f; v3 d(cosf(a) * 0.4f, 0, sinf(a) * 0.4f); b.dquad(v3(0, 0, 0) - d, v3(0, 0, 0) + d, v3(0, 0.6f, 0) + d, v3(0, 0.6f, 0) - d); }
    for (auto& v : r.parts[0].v) v.n = v3(0, 1, 0);
    bakeStatic(M_FLOWERS, r); }
  // desert
  for (int v = 0; v < 2; v++) {
    Rig r(1); b.to(r.parts[0]).c(v ? 0x5a9a3a : 0x4a8a34).l(L_CACTUS).uv(0.8f); float h = v ? 2.2f : 3.4f;
    b.cyl({0, -0.1f, 0}, 0.38f, 0.34f, h, 8, false).sphere({0, h - 0.1f, 0}, {0.34f, 0.3f, 0.34f}, 8, 4);
    b.limb({0.3f, h * 0.45f, 0}, {0.8f, h * 0.5f, 0}, 0.2f, 0.18f, 6).limb({0.8f, h * 0.45f, 0}, {0.85f, h * 0.8f, 0}, 0.18f, 0.16f, 6).sphere({0.85f, h * 0.8f, 0}, {0.16f, 0.14f, 0.16f}, 6, 3);
    if (!v) b.limb({-0.3f, h * 0.35f, 0}, {-0.75f, h * 0.38f, 0}, 0.18f, 0.16f, 6).limb({-0.75f, h * 0.33f, 0}, {-0.78f, h * 0.62f, 0}, 0.16f, 0.14f, 6).sphere({-0.78f, h * 0.62f, 0}, {0.14f, 0.12f, 0.14f}, 6, 3);
    b.uv(1); bakeStatic(v ? M_CACTUS_PROP2 : M_CACTUS_PROP, r);
  }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BARK).f(2); Rng rg(77);
    b.limb({0, -0.2f, 0}, {0.1f, 2.4f, 0}, 0.22f, 0.12f, 6);
    for (int i = 0; i < 5; i++) { float a = i * 1.3f; v3 s0(0.05f, 1.2f + i * 0.25f, 0); v3 e(cosf(a) * 1.2f, 2.0f + i * 0.3f, sinf(a) * 1.2f); b.limb(s0, e, 0.09f, 0.02f, 4); }
    b.f(0); bakeStatic(M_DEAD_TREE, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_SAND).f(2).uv(0.4f).ico({0, 1.0f, 0}, {2.2f, 1.6f, 1.8f}, 1, 0.2f, 601).ico({1.5f, 0.6f, 0.8f}, {1.0f, 0.8f, 0.9f}, 0, 0.2f, 602); b.f(0).uv(1); bakeStatic(M_SANDSTONE, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_SAND).f(2).uv(0.4f).box({-2.0f, 2.0f, 0}, {0.6f, 2.0f, 0.7f}).box({2.0f, 2.0f, 0}, {0.6f, 2.0f, 0.7f}).box({0, 4.3f, 0}, {2.8f, 0.45f, 0.75f}); b.f(0).uv(1); bakeStatic(M_SANDSTONE_ARCH, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_SAND).f(2).uv(0.5f).boxT({0, 2.5f, 0}, {0.6f, 2.5f, 0.6f}, {0.35f, 2.5f, 0.35f}); b.f(0).c(0xf0c040).l(L_GOLD).e(60).cone({0, 5.0f, 0}, 0.5f, 0.8f, 4); b.e(0).uv(1); bakeStatic(M_OBELISK, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xece2c8).l(L_BONE); for (int i = 0; i < 6; i++) { float z = -0.8f + i * 0.32f; b.X(m4::T({0, 0.4f, z})).limb({-0.7f, 0, 0}, {-0.1f, 0.6f, 0}, 0.06f, 0.05f, 4).limb({0.7f, 0, 0}, {0.1f, 0.6f, 0}, 0.06f, 0.05f, 4); } b.noX().limb({0, 1.0f, -1.0f}, {0, 0.9f, 1.0f}, 0.08f, 0.08f, 5).sphere({0, 0.7f, 1.4f}, {0.35f, 0.3f, 0.45f}, 7, 5); bakeStatic(M_BONES, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BARK).f(2).limb({0, -0.2f, 0}, {0.3f, 2.5f, 0}, 0.2f, 0.16f, 6).limb({0.3f, 2.5f, 0}, {0.7f, 4.8f, 0}, 0.16f, 0.12f, 6); b.f(1).l(L_LEAVES);
    for (int i = 0; i < 6; i++) { float a = i * TAU / 6; v3 tip = v3(0.7f, 4.8f, 0) + v3(cosf(a) * 2.2f, -0.9f, sinf(a) * 2.2f); v3 sd = norm(cross(tip - v3(0.7f, 4.8f, 0), v3(0, 1, 0))) * 0.45f; b.dquad(v3(0.7f, 4.8f, 0) - sd * 0.3f, v3(0.7f, 4.8f, 0) + sd * 0.3f, tip + sd * 0.2f, tip - sd * 0.2f); }
    b.f(0); bakeStatic(M_PALM, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_TUFT).f(1); for (int i = 0; i < 2; i++) { float a = i * PI / 2; v3 d(cosf(a) * 0.5f, 0, sinf(a) * 0.5f); b.dquad(-d, d, d + v3(0, 0.6f, 0), -d + v3(0, 0.6f, 0)); } for (auto& v : r.parts[0].v) v.n = v3(0, 1, 0); b.f(0); bakeStatic(M_DESERT_GRASS, r); }
  // graveyard
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_TOMB).f(2).uv(0.8f).box({0, 0.6f, 0}, {0.45f, 0.6f, 0.12f}).sphere({0, 1.2f, 0}, {0.45f, 0.25f, 0.12f}, 8, 3, 0.5f); b.c(G).l(L_DIRT).box({0, 0.05f, 0.7f}, {0.45f, 0.08f, 0.7f}); b.f(0).uv(1); bakeStatic(M_TOMB, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_STONE).f(2).box({0, 0.8f, 0}, {0.1f, 0.8f, 0.1f}).box({0, 1.15f, 0}, {0.42f, 0.1f, 0.1f}); b.f(0); bakeStatic(M_TOMB_CROSS, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BARK).f(2); b.limb({0, -0.2f, 0}, {-0.3f, 1.8f, 0.2f}, 0.28f, 0.16f, 6).limb({-0.3f, 1.8f, 0.2f}, {0.3f, 3.2f, -0.2f}, 0.16f, 0.08f, 5);
    for (int i = 0; i < 6; i++) { float a = i * 1.1f + 0.5f; v3 s0 = v3(-0.2f, 1.6f + i * 0.25f, 0.1f); b.limb(s0, s0 + v3(cosf(a) * 1.3f, 0.6f + (i % 2) * 0.3f, sinf(a) * 1.3f), 0.08f, 0.015f, 4); }
    b.f(0); bakeStatic(M_DEAD_TREE2, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x2a2a30).l(L_METAL); for (int i = 0; i < 7; i++) { float x = -1.5f + i * 0.5f; b.box({x, 0.7f, 0}, {0.03f, 0.7f, 0.03f}).cone({x, 1.4f, 0}, 0.05f, 0.12f, 4); } b.box({0, 1.2f, 0}, {1.6f, 0.03f, 0.03f}).box({0, 0.3f, 0}, {1.6f, 0.03f, 0.03f}); bakeStatic(M_FENCE, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_BRICK).f(2).uv(0.5f).box({0, 1.5f, 0}, {2.0f, 1.5f, 2.5f});
    b.quad({-2.2f, 3.0f, -2.7f}, {2.2f, 3.0f, -2.7f}, {0, 4.6f, -2.7f}, {0, 4.6f, -2.7f}); b.f(0).c(0x3a3440).l(L_STONE).boxT({0, 3.8f, 0}, {2.2f, 0.8f, 2.7f}, {0.1f, 0.8f, 2.7f}); b.c(0x101014).box({0, 1.1f, 2.51f}, {0.7f, 1.1f, 0.02f}); b.uv(1); bakeStatic(M_CRYPT, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x2a2a30).l(L_METAL).cyl({0, 0, 0}, 0.06f, 0.05f, 2.2f, 5, false).box({0.25f, 2.15f, 0}, {0.3f, 0.03f, 0.03f}); b.c(0xffd070).e(255).box({0.5f, 1.9f, 0}, {0.12f, 0.16f, 0.12f}); b.e(0).c(0x2a2a30).cone({0.5f, 2.06f, 0}, 0.16f, 0.12f, 4); bakeStatic(M_LANTERN, r); }
  { Rig r(1); b.to(r.parts[0]).c(G).l(L_TUFT).f(1); for (int i = 0; i < 3; i++) { float a = i * PI / 3; v3 d(cosf(a) * 0.5f, 0, sinf(a) * 0.5f); b.dquad(-d, d, d + v3(0, 0.55f, 0), -d + v3(0, 0.55f, 0)); } for (auto& v : r.parts[0].v) v.n = v3(0, 1, 0); b.f(0); bakeStatic(M_GRAVE_GRASS, r); }
}

// ---------------------------------------------------------------- interactables
static void buildInteractables() {
  MB b;
  for (int g = 0; g < 2; g++) { // chests
    Rig r(2); r.parts[1].parent = 0; r.parts[1].pivot = {0, 0.62f, -0.4f};
    u32 body = g ? 0xf0c040 : 0x8a5a30, trim = g ? 0xfff0a0 : 0xd4a43a;
    b.to(r.parts[0]).c(body).l(g ? L_GOLD : L_WOOD).e(g ? 40 : 0).box({0, 0.32f, 0}, {0.62f, 0.32f, 0.42f});
    b.c(trim).l(L_GOLD).box({0, 0.32f, 0.425f}, {0.64f, 0.05f, 0.01f}).box({-0.5f, 0.32f, 0.425f}, {0.05f, 0.33f, 0.01f}).box({0.5f, 0.32f, 0.425f}, {0.05f, 0.33f, 0.01f});
    b.c(0x2a2020).box({0, 0.55f, 0.43f}, {0.06f, 0.08f, 0.02f});
    b.to(r.parts[1]).c(body).l(g ? L_GOLD : L_WOOD).X(m4::T({0, 0.62f, 0}) * m4::RZ(PI / 2)).cyl({0, -0.62f, 0}, 0.42f, 0.42f, 1.24f, 8, true).noX();
    b.c(trim).l(L_GOLD).X(m4::T({0, 0.62f, 0}) * m4::RZ(PI / 2)).cyl({0, -0.5f, 0}, 0.435f, 0.435f, 0.08f, 8, false).cyl({0, 0.42f, 0}, 0.435f, 0.435f, 0.08f, 8, false).noX();
    b.e(0);
    // remove lower half of lid cylinder visually by squashing: we keep it (inside body)
    std::vector<ClipDef> cl = {
      {CL_IDLE, 1.0f, true, [](float, Pose&) {}},
      {CL_SPECIAL, 0.6f, false, [](float t, Pose& p) { float k = smooth01(t); p.p[1].r.x = -1.9f * k; p.rootS = {1 + sinf(t * PI) * 0.08f, 1 - sinf(t * PI) * 0.06f, 1 + sinf(t * PI) * 0.08f}; }},
    };
    bakeModel(g ? M_CHEST_GOLD : M_CHEST, r, cl);
  }
  { // Charge shrine: "upgrade tower"
    Rig r(2); r.parts[1].parent = 0; r.parts[1].pivot = {0, 3.6f, 0};
    b.to(r.parts[0]).c(0x8a8f9a).l(L_STONE).uv(0.6f).cyl({0, 0, 0}, 1.1f, 1.0f, 0.4f, 8, true).cyl({0, 0.4f, 0}, 0.6f, 0.45f, 2.6f, 8, true);
    b.c(0x6a707a).box({0, 3.0f, 0}, {0.7f, 0.12f, 0.7f});
    b.c(0x40d0ff).l(L_RUNE).e(160).cyl({0, 0.9f, 0}, 0.62f, 0.55f, 1.4f, 8, false); b.e(0);
    for (int i = 0; i < 4; i++) { float a = i * PI / 2 + PI / 4; b.c(0x7a808a).l(L_STONE).box({cosf(a) * 0.62f, 3.25f, sinf(a) * 0.62f}, {0.08f, 0.25f, 0.08f}); }
    b.to(r.parts[1]).c(0x60e8ff).l(L_CRYSTAL).e(255).ico({0, 3.7f, 0}, {0.35f, 0.55f, 0.35f}, 0, 0.0f, 1); b.e(0);
    std::vector<ClipDef> cl = {{CL_IDLE, 2.0f, true, [](float t, Pose& p) { p.p[1].r.y = t * TAU; p.p[1].t.y = sinf(t * TAU) * 0.15f; }}};
    b.uv(1);
    bakeModel(M_SHRINE_CHARGE, r, cl);
  }
  { Rig r(1); b.to(r.parts[0]).c(0x7a6a5a).l(L_STONE).cyl({0, 0, 0}, 0.9f, 0.8f, 0.9f, 8, true); b.c(0xffd23f).l(L_GOLD).e(80);
    for (int i = 0; i < 9; i++) { float a = i * 2.4f, rr = 0.15f + (i % 3) * 0.2f; b.cyl({cosf(a) * rr, 0.9f + (i % 4) * 0.06f, sinf(a) * rr}, 0.18f, 0.18f, 0.05f, 8, true); }
    b.X(m4::T({0, 1.7f, 0}) * m4::RX(PI / 2)).cyl({0, -0.06f, 0}, 0.5f, 0.5f, 0.12f, 12, true).noX(); b.e(0); bakeStatic(M_SHRINE_GREED, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x2a2030).l(L_STONE).boxT({0, 1.2f, 0}, {0.6f, 1.2f, 0.6f}, {0.3f, 1.2f, 0.3f}); b.c(0xe0e0e0).l(L_BONE).sphere({0, 2.65f, 0}, {0.32f, 0.3f, 0.32f}, 7, 5); b.c(0xc040ff).e(255).box({0.11f, 2.7f, 0.28f}, {0.06f, 0.06f, 0.04f}).box({-0.11f, 2.7f, 0.28f}, {0.06f, 0.06f, 0.04f}); b.cone({0, 2.95f, 0}, 0.2f, 0.6f, 5); b.e(0); bakeStatic(M_SHRINE_CURSED, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x5a5468).l(L_STONE).box({0, 0.45f, 0}, {1.1f, 0.45f, 0.7f}); b.c(0xb8c0cc).l(L_METAL);
    b.X(m4::T({0, 1.3f, 0}) * m4::RZ(0.7f)).box({0, 0, 0}, {0.06f, 0.9f, 0.02f}).noX(); b.X(m4::T({0, 1.3f, 0}) * m4::RZ(-0.7f)).box({0, 0, 0}, {0.06f, 0.9f, 0.02f}).noX();
    b.c(0xc040ff).e(255).cone({-0.8f, 0.9f, 0}, 0.15f, 0.5f, 5).cone({0.8f, 0.9f, 0}, 0.15f, 0.5f, 5); b.e(0); bakeStatic(M_SHRINE_CHALLENGE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x8a8a8a).l(L_ROCK).uv(0.5f).boxT({0, 1.6f, 0}, {0.85f, 1.6f, 0.75f}, {0.7f, 1.6f, 0.65f});
    b.box({0, 2.4f, 0.75f}, {0.6f, 0.15f, 0.12f}).boxT({0, 1.8f, 0.85f}, {0.18f, 0.5f, 0.15f}, {0.1f, 0.5f, 0.1f}).box({0, 0.9f, 0.72f}, {0.4f, 0.08f, 0.1f}); b.uv(1); bakeStatic(M_MOAI, r); }
  { // shady guy: hooded merchant with tintable hat (flag 1)
    MB bb; HSpec s; s.skin = 0x2a2430; s.top = 0x2a2430; s.bottom = 0x1a1620; s.feet = 0x141018; s.torsoW = 0.5f; s.torsoD = 0.35f;
    HJ J; Rig r = humanoid(s, J, bb);
    bb.to(r.parts[P_TORSO]).c(0x2a2430).l(L_CLOTH).boxT(J.hip + v3(0, -0.4f, 0), v3(0.38f, 0.45f, 0.3f), v3(0.28f, 0.45f, 0.22f));
    Part& H = r.parts[P_HEAD];
    bb.to(H).c(0x1a1620).l(L_CLOTH).sphere(J.headC, {0.24f, 0.25f, 0.24f}, 8, 6);
    addEyes(bb, H, J.headC + v3(0, 0.02f, 0.21f), 0.07f, 0.03f, 0xfff060, true);
    bb.to(H).c(0x808080).l(L_CLOTH).f(1).cyl(J.headC + v3(0, 0.15f, 0), 0.5f, 0.5f, 0.04f, 12, true).cyl(J.headC + v3(0, 0.18f, 0), 0.26f, 0.22f, 0.3f, 10, true); bb.f(0);
    bakeModel(M_SHADY, r, {humanClips(1.0f, 0.5f, 0)[0]});
  }
  { Rig r(1); b.to(r.parts[0]).c(0xd8dce4).l(L_METAL).box({0, 0.45f, 0}, {0.7f, 0.45f, 0.5f}); b.c(0x202830).box({-0.15f, 0.45f, 0.505f}, {0.45f, 0.32f, 0.01f}); b.c(0xffe080).e(200).box({-0.15f, 0.45f, 0.51f}, {0.38f, 0.25f, 0.01f}); b.e(0).c(0x404850).box({0.5f, 0.6f, 0.505f}, {0.1f, 0.1f, 0.01f}).box({0.5f, 0.3f, 0.505f}, {0.1f, 0.06f, 0.01f}); b.c(0x808890).l(L_STONE).box({0, -0.0f, 0}, {0.8f, 0.05f, 0.6f}); bakeStatic(M_MICROWAVE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x5a4a6a).l(L_STONE).cyl({0, 0, 0}, 0.7f, 0.6f, 0.5f, 8, true).cyl({0, 0.5f, 0}, 0.3f, 0.3f, 1.6f, 6, false); b.c(0xd02020).e(80).X(m4::T({0, 2.4f, 0})).limb({-0.35f, -0.3f, 0}, {-0.35f, 0.2f, 0}, 0.12f, 0.12f, 6).limb({0.35f, -0.3f, 0}, {0.35f, 0.2f, 0}, 0.12f, 0.12f, 6).limb({-0.35f, 0.2f, 0}, {0.35f, 0.2f, 0}, 0.12f, 0.12f, 6); b.c(0xe0e0e8).limb({-0.35f, -0.45f, 0}, {-0.35f, -0.3f, 0}, 0.13f, 0.13f, 6).limb({0.35f, -0.45f, 0}, {0.35f, -0.3f, 0}, 0.13f, 0.13f, 6); b.noX().e(0); bakeStatic(M_SHRINE_SUCC, r); }
  for (int pi = 0; pi < 2; pi++) {
    Rig r(2); r.parts[1].parent = 0; r.parts[1].pivot = {0, 2.2f, 0};
    b.to(r.parts[0]).c(0x6a6a78).l(L_STONE).uv(0.6f).box({-1.6f, 1.6f, 0}, {0.4f, 1.6f, 0.4f}).box({1.6f, 1.6f, 0}, {0.4f, 1.6f, 0.4f}).box({0, 3.5f, 0}, {2.1f, 0.35f, 0.45f}).box({0, 0.1f, 0}, {2.3f, 0.1f, 0.8f});
    b.uv(1).c(pi ? 0x40c4ff : 0xff3040).l(L_PORTAL).e(255);
    b.to(r.parts[1]).X(m4::T({0, 1.75f, 0}) * m4::RX(PI / 2)).cyl({0, -0.02f, 0}, 1.25f, 1.25f, 0.04f, 16, true).noX(); b.e(0);
    std::vector<ClipDef> cl = {{CL_IDLE, 1.5f, true, [](float t, Pose& p) { p.p[1].r.z = t * TAU; }}};
    r.parts[1].pivot = {0, 1.75f, 0};
    bakeModel(pi ? M_PORTAL_EXIT : M_PORTAL_BOSS, r, cl);
  }
  auto pot = [&](int id, float k, u32 col, u8 layer, u8 em) {
    Rig r(1); b.to(r.parts[0]).c(col).l(layer).e(em).lathe({0, 0, 0}, {{0.001f, 0}, {0.25f * k, 0.02f * k}, {0.42f * k, 0.3f * k}, {0.38f * k, 0.6f * k}, {0.2f * k, 0.78f * k}, {0.24f * k, 0.86f * k}, {0.001f, 0.86f * k}}, 10); b.e(0);
    bakeStatic(id, r);
  };
  pot(M_POT, 1.0f, 0xc07a4a, L_DIRT, 0); pot(M_POT_BIG, 1.5f, 0xa0603a, L_DIRT, 0); pot(M_POT_SILVER, 1.0f, 0xd8dde6, L_METAL, 40);
  { Rig r(1); b.to(r.parts[0]).c(0xff8f1a).l(L_PUMPKIN).sphere({0, 0.4f, 0}, {0.55f, 0.4f, 0.55f}, 12, 6); b.c(0x3a7a2a).cyl({0, 0.75f, 0}, 0.06f, 0.04f, 0.2f, 5, true); b.c(0xffe060).e(255).box({0.16f, 0.48f, 0.5f}, {0.07f, 0.07f, 0.03f}).box({-0.16f, 0.48f, 0.5f}, {0.07f, 0.07f, 0.03f}).box({0, 0.28f, 0.52f}, {0.2f, 0.04f, 0.03f}); b.e(0); bakeStatic(M_PUMPKIN, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xb8905a).l(L_TUFT); Rng rg(9); for (int i = 0; i < 16; i++) { v3 d = rg.onSphere(); v3 c0 = v3(0, 0.55f, 0) + d * 0.45f; v3 e = norm(cross(d, rg.onSphere())); b.limb(c0 - e * 0.35f, c0 + e * 0.35f, 0.03f, 0.03f, 3, false); } bakeStatic(M_TUMBLEWEED, r); }
  { Rig r(2); r.parts[1].parent = 0; r.parts[1].pivot = {0, 1.2f, 0};
    b.to(r.parts[1]).c(0x7fff9f).l(L_GOLD).e(200).X(m4::T({0, 1.2f, 0}) * m4::RX(PI / 2)).limb({0, -0.5f, 0}, {0, 0.3f, 0}, 0.06f, 0.06f, 6).noX(); b.X(m4::T({0, 1.2f, -0.45f})).sphere({0, 0, 0}, {0.18f, 0.18f, 0.06f}, 8, 4).noX().box({0.1f, 1.2f, 0.3f}, {0.08f, 0.04f, 0.03f}); b.e(0);
    bakeModel(M_CRYPT_KEY, r, {{CL_IDLE, 2.0f, true, [](float t, Pose& p) { p.p[1].r.y = t * TAU; p.p[1].t.y = sinf(t * TAU) * 0.2f; }}}); }
  { Rig r(1); b.to(r.parts[0]).c(0x5a5060).l(L_STONE).cyl({0, 0, 0}, 1.4f, 1.3f, 0.4f, 8, true); b.c(0xff4040).l(L_CRYSTAL).e(255).ico({0, 1.2f, 0}, {0.4f, 0.7f, 0.4f}, 0, 0, 2); b.e(0); bakeStatic(M_ALTAR, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x6a4a2a).l(L_BARK).cyl({0, 0, 0}, 0.5f, 0.35f, 2.6f, 7, true); b.c(0x60ff80).e(255).ico({0, 2.9f, 0}, {0.4f, 0.4f, 0.4f}, 0, 0.1f, 5); b.e(0); bakeStatic(M_HEAL_PILLAR, r); }
}

// ---------------------------------------------------------------- pickups & projectiles
static void buildPickups() {
  MB b;
  auto gem = [&](int id, float s, u32 col) { Rig r(1); b.to(r.parts[0]).c(col).l(L_CRYSTAL).e(200).ico({0, 0, 0}, {s * 0.6f, s, s * 0.6f}, 0, 0, 1); b.e(0); bakeStatic(id, r); };
  gem(M_XP_SMALL, 0.16f, 0x38b8ff); gem(M_XP_MED, 0.22f, 0x40ff90); gem(M_XP_BIG, 0.3f, 0xff4080);
  { Rig r(1); b.to(r.parts[0]).c(0xffd23f).l(L_GOLD).e(80).X(m4::RX(PI / 2)).cyl({0, -0.03f, 0}, 0.22f, 0.22f, 0.06f, 10, true).noX(); b.e(0); bakeStatic(M_GOLD_COIN, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xdfe6f0).l(L_METAL).e(80).X(m4::RX(PI / 2)).cyl({0, -0.03f, 0}, 0.22f, 0.22f, 0.06f, 10, true).noX(); b.e(0); bakeStatic(M_SILVER_COIN, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xe0a050).l(L_NOISE).sphere({0, 0.12f, 0}, {0.3f, 0.14f, 0.3f}, 8, 4, 0.48f); b.c(0x4caf50).cyl({0, 0.02f, 0}, 0.31f, 0.31f, 0.03f, 8, false); b.c(0x6d3b1f).cyl({0, -0.06f, 0}, 0.3f, 0.3f, 0.08f, 8, true); b.c(0xe0a050).cyl({0, -0.14f, 0}, 0.28f, 0.29f, 0.08f, 8, true); bakeStatic(M_BORGAR, r); }
  // powerup tokens
  { Rig r(1); b.to(r.parts[0]).c(0x4fc3f7).l(L_PLAIN).e(160).sphere({0, 0, 0}, {0.35f, 0.35f, 0.35f}, 8, 6); b.c(0xe1f5fe).e(255).box({0, 0, 0.33f}, {0.12f, 0.15f, 0.03f}); b.e(0); bakeStatic(M_PU_SHIELD, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x90caf9).l(L_METAL).e(120).X(m4::RX(PI / 2)).cyl({0, -0.06f, 0}, 0.34f, 0.34f, 0.12f, 12, true).noX(); b.c(0x1a2a40).box({0, 0.1f, 0.07f}, {0.025f, 0.12f, 0.01f}).box({0.07f, 0, 0.07f}, {0.08f, 0.025f, 0.01f}); b.e(0); bakeStatic(M_PU_CLOCK, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xffd23f).l(L_GOLD).e(150); for (int i = 0; i < 4; i++) b.X(m4::T({0, -0.15f + i * 0.08f, 0}) * m4::RY(i * 0.7f)).cyl({0.04f * (i & 1), 0, 0}, 0.25f, 0.25f, 0.06f, 10, true).noX(); b.e(0); bakeStatic(M_PU_GOLD, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xff3030).l(L_PLAIN).e(170).ico({0, 0, 0}, {0.3f, 0.3f, 0.3f}, 0, 0, 3); for (int i = 0; i < 8; i++) { v3 d = norm(v3(cosf(i * 0.8f), sinf(i * 1.7f), sinf(i * 0.8f))); b.limb(d * 0.25f, d * 0.5f, 0.07f, 0.005f, 4); } b.e(0); bakeStatic(M_PU_RAGE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xe02020).l(L_PLAIN).e(140).limb({-0.2f, -0.25f, 0}, {-0.2f, 0.1f, 0}, 0.09f, 0.09f, 6).limb({0.2f, -0.25f, 0}, {0.2f, 0.1f, 0}, 0.09f, 0.09f, 6).limb({-0.2f, 0.1f, 0}, {0.2f, 0.1f, 0}, 0.09f, 0.09f, 6); b.c(0xe0e0e8).limb({-0.2f, -0.4f, 0}, {-0.2f, -0.25f, 0}, 0.095f, 0.095f, 6).limb({0.2f, -0.4f, 0}, {0.2f, -0.25f, 0}, 0.095f, 0.095f, 6); b.e(0); bakeStatic(M_PU_MAGNET, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x2a2630).l(L_METAL).e(30).sphere({0, 0, 0}, {0.32f, 0.32f, 0.32f}, 8, 6); b.c(0x8a6a4a).limb({0.15f, 0.25f, 0}, {0.3f, 0.45f, 0}, 0.03f, 0.03f, 4); b.c(0xffb020).e(255).sphere({0.3f, 0.48f, 0}, {0.07f, 0.07f, 0.07f}, 5, 3); b.e(0); bakeStatic(M_PU_NUKE, r); }
  // projectiles (+Z forward)
  { Rig r(1); b.to(r.parts[0]).c(0xff7a1a).l(L_PLAIN).e(255).sphere({0, 0, 0}, {0.28f, 0.28f, 0.28f}, 8, 6); b.c(0xffe080).sphere({0, 0, 0.12f}, {0.16f, 0.16f, 0.18f}, 6, 4); b.e(0); bakeStatic(M_P_FIREBALL, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x8a5a30).l(L_WOOD).limb({0, 0, -0.5f}, {0, 0, 0.4f}, 0.025f, 0.025f, 4); b.c(0xd0d8e0).l(L_METAL).limb({0, 0, 0.4f}, {0, 0, 0.6f}, 0.06f, 0.001f, 4); b.c(0xff5050).l(L_PLAIN).dquad({0, 0, -0.5f}, {0, 0, -0.3f}, {0, 0.1f, -0.25f}, {0, 0.1f, -0.5f}).dquad({0, 0, -0.5f}, {0, 0, -0.3f}, {0.1f, 0, -0.25f}, {0.1f, 0, -0.5f}); bakeStatic(M_P_ARROW, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xfff0a0).l(L_PLAIN).e(255).limb({0, 0, -0.25f}, {0, 0, 0.12f}, 0.05f, 0.06f, 5); b.e(0); bakeStatic(M_P_BULLET, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xf4ecd8).l(L_BONE).limb({-0.35f, 0, 0}, {0.35f, 0, 0}, 0.06f, 0.06f, 5).sphere({-0.4f, 0.06f, 0}, {0.08f, 0.08f, 0.08f}, 5, 3).sphere({-0.4f, -0.06f, 0}, {0.08f, 0.08f, 0.08f}, 5, 3).sphere({0.4f, 0.06f, 0}, {0.08f, 0.08f, 0.08f}, 5, 3).sphere({0.4f, -0.06f, 0}, {0.08f, 0.08f, 0.08f}, 5, 3); bakeStatic(M_P_BONE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x6a4a2a).l(L_WOOD).limb({0, 0, -0.45f}, {0, 0, 0.45f}, 0.04f, 0.04f, 5); b.c(0xc9d1db).l(L_METAL).e(40).boxT({0.0f, 0, 0.32f}, {0.02f, 0.12f, 0.15f}, {0.02f, 0.3f, 0.22f}); b.e(0); r.parts[0].pivot = {0, 0, 0}; bakeStatic(M_P_AXE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x4f8fe0).l(L_METAL).e(60).X(m4::RX(PI / 2)).cyl({0, -0.05f, 0}, 0.5f, 0.45f, 0.1f, 10, true).noX(); b.c(0xf5c542).l(L_GOLD).sphere({0, 0, 0.07f}, {0.14f, 0.14f, 0.08f}, 6, 4); b.e(0); bakeStatic(M_P_SHIELD, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xffe14d).l(L_PLAIN).e(30); for (int i = 0; i < 6; i++) { float a = -0.9f + i * 0.36f; b.sphere({sinf(a) * 0.35f, 0, cosf(a) * 0.35f - 0.25f}, {0.09f, 0.08f, 0.09f}, 5, 3); } b.c(0x5a3a1a).sphere({sinf(-1.05f) * 0.35f, 0, cosf(-1.05f) * 0.35f - 0.25f}, {0.05f, 0.05f, 0.05f}, 4, 3); b.e(0); bakeStatic(M_P_BANANA, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x0a0610).l(L_PLAIN).sphere({0, 0, 0}, {0.5f, 0.5f, 0.5f}, 10, 8); b.c(0xa34fff).l(L_PORTAL).e(255).ringXZ({0, 0, 0}, 0.55f, 0.95f, 16); b.e(0); bakeStatic(M_P_BLACKHOLE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xff2040).l(L_PLAIN).e(255).sphere({0, 0, 0}, {0.22f, 0.22f, 0.22f}, 7, 5); b.e(0); bakeStatic(M_P_BLOODORB, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x9a8f84).l(L_ROCK).ico({0, 0, 0}, {0.42f, 0.36f, 0.4f}, 0, 0.25f, 7); bakeStatic(M_P_ROCK, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xc8d0dc).l(L_METAL).e(60).boxT({0, 0, 0.1f}, {0.02f, 0.32f, 0.3f}, {0.02f, 0.18f, 0.12f}); b.c(0x6a1a1a).box({0, 0, -0.32f}, {0.03f, 0.04f, 0.15f}); b.e(0); bakeStatic(M_P_BLADE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xffffff).l(L_PLAIN).e(40).box({0, 0, 0}, {0.18f, 0.18f, 0.18f}); b.c(0xd02020).e(0); for (int i = 0; i < 6; i++) { v3 n[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}; b.box(n[i] * 0.185f, v3(0.04f, 0.04f, 0.04f)); } bakeStatic(M_P_DIE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x3a3a44).l(L_METAL).sphere({0, 0, 0}, {0.35f, 0.18f, 0.35f}, 8, 4, 0.5f); b.c(0xff2020).e(255).sphere({0, 0.16f, 0}, {0.07f, 0.07f, 0.07f}, 5, 3); b.e(0); bakeStatic(M_P_MINE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x7bd23f).l(L_PLAIN).e(120).sphere({0, 0, 0}, {0.2f, 0.2f, 0.2f}, 7, 5); b.c(0xe0f0e0).e(0).cyl({0, 0.15f, 0}, 0.07f, 0.06f, 0.15f, 5, true); bakeStatic(M_P_FLASK, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xff5fa2).l(L_METAL).limb({0, 0, -0.3f}, {0, 0, 0.25f}, 0.1f, 0.1f, 6); b.c(0xffffff).limb({0, 0, 0.25f}, {0, 0, 0.45f}, 0.1f, 0.001f, 6); b.c(0x7a4aa0).dquad({0, 0, -0.3f}, {0, 0, -0.1f}, {0, 0.2f, -0.35f}, {0, 0.2f, -0.4f}).dquad({0, 0, -0.3f}, {0, 0, -0.1f}, {0.2f, 0, -0.35f}, {0.2f, 0, -0.4f}).dquad({0, 0, -0.3f}, {0, 0, -0.1f}, {-0.2f, 0, -0.35f}, {-0.2f, 0, -0.4f}); bakeStatic(M_P_ROCKET, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x9be7ff).l(L_METAL).e(160).boxT({0, 0, 0.12f}, {0.015f, 0.07f, 0.22f}, {0.015f, 0.01f, 0.2f}); b.c(0x203040).e(0).box({0, 0, -0.14f}, {0.025f, 0.03f, 0.08f}); b.c(0x4fc3f7).box({0, 0, -0.06f}, {0.08f, 0.02f, 0.02f}); bakeStatic(M_P_DAGGER, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xdfe6ee).l(L_PORTAL).e(30); for (int i = 0; i < 6; i++) b.cyl({sinf(i * 1.3f) * 0.08f, i * 0.4f, 0}, 0.2f + i * 0.15f, 0.25f + i * 0.17f, 0.36f, 8, false); b.e(0); bakeStatic(M_P_TORNADO, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x40f0ff).l(L_PLAIN).e(255).sphere({0, 0, 0}, {0.2f, 0.2f, 0.2f}, 7, 5); b.e(0); bakeStatic(M_P_ORB, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x80f0ff).l(L_PLAIN).e(255).sphere({0, 0, 0}, {0.18f, 0.18f, 0.2f}, 6, 4).limb({0, 0, -0.05f}, {0, 0, -0.5f}, 0.14f, 0.01f, 5, false); b.e(0); bakeStatic(M_P_SOUL, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xb8e070).l(L_PLAIN).limb({0, 0, -0.2f}, {0, 0, 0.25f}, 0.04f, 0.001f, 4); bakeStatic(M_P_SPIKE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xd040ff).l(L_PLAIN).e(255).sphere({0, 0, 0}, {0.3f, 0.3f, 0.3f}, 7, 5); b.c(0xffc0ff).sphere({0, 0, 0}, {0.16f, 0.16f, 0.16f}, 5, 3); b.e(0); bakeStatic(M_P_ENEMYBALL, r); }
  { Rig r(1); b.to(r.parts[0]).c(0x6a4a2a).l(L_WOOD).limb({0, 0, -0.9f}, {0, 0, 0.6f}, 0.04f, 0.04f, 5); b.c(0xd0d8ff).l(L_METAL).e(80); for (int i = 0; i < 6; i++) { float a0 = i * 0.25f, a1 = (i + 1) * 0.25f; b.dquad({cosf(a0) * 0.9f - 0.0f, 0, 0.6f - sinf(a0) * 0.2f}, {cosf(a1) * 0.9f, 0, 0.6f - sinf(a1) * 0.2f}, {cosf(a1) * 0.7f, 0, 0.6f - sinf(a1) * 0.12f}, {cosf(a0) * 0.7f, 0, 0.6f - sinf(a0) * 0.12f}); } b.e(0); bakeStatic(M_P_SCYTHE, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xffe680).l(L_PLAIN).e(255); for (int i = 0; i < 8; i++) { float a0 = -0.8f + i * 0.2f, a1 = a0 + 0.2f; b.dquad({sinf(a0) * 0.9f, 0, cosf(a0) * 0.9f - 0.6f}, {sinf(a1) * 0.9f, 0, cosf(a1) * 0.9f - 0.6f}, {sinf(a1) * 0.6f, 0, cosf(a1) * 0.6f - 0.6f}, {sinf(a0) * 0.6f, 0, cosf(a0) * 0.6f - 0.6f}); } b.e(0); bakeStatic(M_P_WAVE, r); }
  { // Bob (dead): little zombie buddy
    MB bb; HSpec s; s.leg = 0.3f; s.torso = 0.28f; s.head = 0.18f; s.shoulder = 0.26f; s.hip = 0.16f; s.arm = 0.3f; s.limbR = 0.045f; s.torsoW = 0.24f; s.torsoD = 0.16f; s.skin = 0x8aa07a; s.top = 0x6a5a7a; s.bottom = 0x3a3a4a;
    HJ J; Rig r = humanoid(s, J, bb); bb.to(r.parts[P_HEAD]).c(0x8aa07a).l(L_SKIN).sphere(J.headC, {0.16f, 0.15f, 0.15f}, 7, 5); addEyes(bb, r.parts[P_HEAD], J.headC + v3(0, 0.02f, 0.14f), 0.05f, 0.022f, 0x101010);
    bakeModel(M_P_BOB, r, humanClips(0.4f, 1.0f, 3));
  }
  { Rig r(1); b.to(r.parts[0]).c(0xd8e0ea).l(L_METAL).e(40).box({0, 0, 0.5f}, {0.02f, 0.06f, 0.5f}); b.c(0x7a4a2a).box({0, 0, -0.08f}, {0.025f, 0.03f, 0.1f}); b.c(0xd4a43a).box({0, 0, 0.02f}, {0.12f, 0.03f, 0.02f}); b.e(0); bakeStatic(M_P_SWORD, r); }
  { Rig r(1); b.to(r.parts[0]).c(0xe8eef6).l(L_METAL).e(60).box({0, 0, 0.45f}, {0.012f, 0.03f, 0.45f}); b.e(0); bakeStatic(M_P_KATANA, r); }
}

static void buildFx() {
  MB b;
  { // slash: 150 degree ribbon, alpha encoded in emis (fx shader uses emis as alpha)
    Rig r(1); b.to(r.parts[0]).l(L_PLAIN);
    int seg = 14; float a0 = -1.3f, a1 = 1.3f;
    for (int i = 0; i < seg; i++) {
      float t0 = i / (float)seg, t1 = (i + 1) / (float)seg;
      float b0 = lerpf(a0, a1, t0), b1 = lerpf(a0, a1, t1);
      float w0 = sinf(t0 * PI) * 0.35f + 0.05f, w1 = sinf(t1 * PI) * 0.35f + 0.05f;
      u8 al0 = (u8)(sinf(t0 * PI) * 255), al1 = (u8)(sinf(t1 * PI) * 255);
      u16 i0, i1, i2, i3;
      b.cc(Col(255, 255, 255)).e(al0); i0 = b.vert({sinf(b0) * 1.0f, 0, cosf(b0) * 1.0f}, {0, 1, 0}, {t0, 0}); i3 = b.vert({sinf(b0) * (1.0f - w0), 0, cosf(b0) * (1.0f - w0)}, {0, 1, 0}, {t0, 1});
      b.e(al1); i1 = b.vert({sinf(b1) * 1.0f, 0, cosf(b1) * 1.0f}, {0, 1, 0}, {t1, 0}); i2 = b.vert({sinf(b1) * (1.0f - w1), 0, cosf(b1) * (1.0f - w1)}, {0, 1, 0}, {t1, 1});
      b.quadIdx(i0, i1, i2, i3); b.quadIdx(i3, i2, i1, i0);
    }
    b.e(0); bakeStatic(M_FX_SLASH, r, true);
  }
  { Rig r(1); b.to(r.parts[0]).cc(Col(255, 255, 255)).e(255).l(L_PLAIN).ico({0, 0, 0}, {1, 1, 1}, 1, 0.0f, 3); b.e(0); bakeStatic(M_FX_SPHERE, r, true); }
  { Rig r(1); b.to(r.parts[0]).l(L_PLAIN); int seg = 24;
    for (int i = 0; i < seg; i++) { float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg; u16 i0, i1, i2, i3;
      b.cc(Col(255, 255, 255)).e(255); i0 = b.vert({cosf(a0), 0, sinf(a0)}, {0, 1, 0}, {0, 0}); i1 = b.vert({cosf(a1), 0, sinf(a1)}, {0, 1, 0}, {1, 0});
      b.e(0); i2 = b.vert({cosf(a1) * 0.7f, 0, sinf(a1) * 0.7f}, {0, 1, 0}, {1, 1}); i3 = b.vert({cosf(a0) * 0.7f, 0, sinf(a0) * 0.7f}, {0, 1, 0}, {0, 1});
      b.quadIdx(i0, i1, i2, i3); b.quadIdx(i3, i2, i1, i0); }
    b.e(0); bakeStatic(M_FX_RING, r, true); }
  { Rig r(1); b.to(r.parts[0]).l(L_PLAIN); int seg = 12; // cone along +Z, apex at origin
    for (int i = 0; i < seg; i++) { float a0 = TAU * i / seg, a1 = TAU * (i + 1) / seg; u16 i0, i1, i2;
      b.cc(Col(255, 255, 255)).e(255); i0 = b.vert({0, 0, 0}, {0, 1, 0}, {0, 0}); b.e(60); i1 = b.vert({cosf(a0) * 0.45f, sinf(a0) * 0.45f, 1}, {0, 1, 0}, {1, 0}); i2 = b.vert({cosf(a1) * 0.45f, sinf(a1) * 0.45f, 1}, {0, 1, 0}, {1, 1});
      b.tri(i0, i1, i2); b.tri(i0, i2, i1); }
    b.e(0); bakeStatic(M_FX_CONE, r, true); }
  { Rig r(1); b.to(r.parts[0]).cc(Col(255, 255, 255)).e(255).l(L_PLAIN).box({0, 0, 0.5f}, {0.5f, 0.5f, 0.5f}); b.e(0); bakeStatic(M_FX_BEAM, r, true); }
}

void ModelLibrary::build() {
  anim.clear(); stat.clear(); idx.clear();
  buildFox(); buildNinja(); buildSkeletonBase(M_CALCIUM, 0xf2ead4, 0x2f7de1, 0); buildMonke(); buildOgre();
  buildRocco(); buildBramble(); buildSizzle(); buildDigby(); buildHoot();
  buildSlime(); buildGoblin(M_GOBLIN, 1.0f, 0); buildBee(); buildGoblin(M_GOBLIN_STRONG, 1.25f, 1); buildBoomer(); buildGolem(M_GOLEM, 1.15f, 0x8a8478, 0x4a7a3a, 0);
  buildGoblin(M_GOBLIN_TANK, 1.6f, 2); buildGhoul(); buildSkeletonBase(M_SKELETON, 0xe8e0c8, 0, 1); buildSkeletonBase(M_SKELETON_ARMORED, 0xe0d8c0, 0, 2);
  buildTumblebone(); buildMummy(M_MUMMY, 1.0f, 0); buildWisp(); buildCactusShooter(); buildScorpion(M_SCORPION, 1.0f, 0x2a2a34, 0x3a3a48, false);
  buildSkeletonBase(M_SKELETON_DUSTY, 0xd8c098, 0, 3); buildSkeletonBase(M_SKELETON_ARMORED_DUSTY, 0xd0b890, 0, 4); buildScorpion(M_SCORPION_BIG, 1.6f, 0x6a2a20, 0x8a3a28, false);
  buildMummy(M_MUMMY_TANK, 1.4f, 1); buildMummy(M_MUMMY_ANCIENT, 1.2f, 2);
  buildGhost(M_GHOST, 1.0f, 0xe8f0ff, 0); buildHead(); buildZombie(); buildGhost(M_GHOST_GREATER, 1.35f, 0xc8e0ff, 1); buildGhost(M_GHOST_PURPLE, 1.35f, 0xc070ff, 2); buildGhost(M_GHOST_RED, 1.45f, 0xff4050, 3);
  buildSkeletonBase(M_SKELETON_GOLD, 0xffcc40, 0, 5); buildSkeletonBase(M_SKELETON_XP, 0x80d8ff, 0, 6);
  buildGolem(M_MB_STONEGOLEM, 2.4f, 0x7a7a80, 0x4a7a3a, 1); buildChunkham(); buildGolem(M_MB_SANDGOLEM, 2.4f, 0xc8a060, 0xb08040, 2);
  buildScorpion(M_MB_SCORPIONUSSY, 2.6f, 0xc04a90, 0xe070b0, true); buildGhost(M_MB_GHOSTHAM, 2.6f, 0xe8f0ff, 5); buildSkeletonBase(M_MB_SKELETONKING, 0xf0e8d0, 0, 7);
  buildEnt(M_B_LILBARK, 1.6f, 0x7a5a3a, 0x4a9a3a, 0); buildEnt(M_B_CHADBARK, 2.4f, 0x6a4a2a, 0x3a8a34, 1); buildEnt(M_B_BARKVADER, 2.7f, 0x3a2a2a, 0x2a3a2a, 2);
  buildAnubis(M_B_LILANUBIS, 1.6f, 0x2a2a34, 0xf0c040, 0x40c0ff, 0); buildAnubis(M_B_ANUBRUH, 2.2f, 0x22222c, 0xf0c040, 0x3080ff, 1); buildAnubis(M_B_JUGEANUBIS, 2.6f, 0x1a1a24, 0xffd860, 0xff4040, 2);
  buildBigBob(); buildGhost(M_B_GHOSTKING, 3.4f, 0xd8c8ff, 6); buildGhost(M_SWARMGHOST, 1.1f, 0xb8d8ff, 4);
  buildProps(); buildInteractables(); buildPickups(); buildFx();
  // scale bosses' radius/height already in geometry
  LOGI("Models built: %d models, %zu anim verts (%.1f MB), %zu static verts, %zu indices", (int)M_COUNT, anim.size(), anim.size() * sizeof(AnimVtx) / 1048576.0, stat.size(), idx.size());
}
