#include "render.h"

Renderer gR;

// ------------------------------------------------------------------ shaders
static const char* kModelVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aNrm;
layout(location=2) in vec4 aCol;
layout(location=3) in vec2 aUV;
layout(location=4) in vec4 aExt;
layout(location=5) in vec4 iPos;
layout(location=6) in vec4 iRot;
layout(location=7) in vec4 iScl;
layout(location=8) in vec4 iTA;
layout(location=9) in vec4 iTB;
layout(location=10) in vec3 aPos2;
layout(location=11) in vec4 aNrm2;
uniform mat4 uVP; uniform vec2 uSnap; uniform float uTime;
out vec3 vCol; out vec2 vUV; flat out float vLayer; out vec3 vN; out vec3 vW; out float vFlash; out float vEmis; out vec4 vTint;
vec3 qrot(vec4 q, vec3 v){ vec3 t = 2.0*cross(q.xyz, v); return v + q.w*t + cross(q.xyz, t); }
void main(){
  float flag = floor(aExt.y*255.0+0.5);
  vec3 lp = mix(aPos, aPos2, iScl.w);
  vec3 wp = qrot(iRot, lp*iScl.xyz) + iPos.xyz;
  if (flag > 2.5) { float h = aUV.y; wp.x += sin(uTime*2.1 + wp.x*0.35 + wp.z*0.27)*0.12*h; wp.z += cos(uTime*1.7 + wp.z*0.31)*0.08*h; }
  vec3 n = qrot(iRot, mix(aNrm.xyz, aNrm2.xyz, iScl.w) / max(iScl.xyz, vec3(0.001)));
  vec3 base = aCol.rgb;
  if (flag > 2.5) {} else if (flag > 1.5) base = iTB.rgb*base.r*2.0; else if (flag > 0.5) base = iTA.rgb*base.r*2.0; else base = mix(base, base*iTA.rgb*1.6, iTA.a);
  vCol = base; vUV = aUV; vLayer = floor(aExt.x*255.0+0.5); vN = n; vW = wp; vFlash = iPos.w; vEmis = aCol.a; vTint = iTA;
  vec4 cp = uVP*vec4(wp,1.0);
  if (uSnap.x > 0.0) cp.xy = floor(cp.xy/cp.w*uSnap + 0.5)/uSnap*cp.w;
  gl_Position = cp;
})";
static const char* kModelFS = R"(#version 300 es
precision mediump float;
precision mediump sampler2DArray;
uniform sampler2DArray uTex;
uniform vec3 uSunDir, uSunCol, uAmbSky, uAmbGnd, uFogCol, uCam; uniform float uFogDen;
in vec3 vCol; in vec2 vUV; flat in float vLayer; in vec3 vN; in vec3 vW; in float vFlash; in float vEmis; in vec4 vTint;
out vec4 o;
void main(){
  vec4 t = texture(uTex, vec3(vUV, vLayer));
  if (t.a < 0.5) discard;
  vec3 n = normalize(vN);
  float diff = clamp(dot(n, uSunDir)*0.6+0.4, 0.0, 1.0);
  vec3 amb = mix(uAmbGnd, uAmbSky, n.y*0.5+0.5);
  vec3 alb = vCol*t.rgb;
  vec3 c = alb*(amb + uSunCol*diff*0.95);
  c = mix(c, alb*1.3 + vec3(0.06), vEmis);
  float d = length(vW - uCam);
  float f = clamp(1.0 - exp(-(d*uFogDen)*(d*uFogDen)), 0.0, 1.0);
  c = mix(c, uFogCol, f*(1.0 - vEmis*0.45));
  c = mix(c, vec3(1.0), vFlash);
  o = vec4(c, 1.0);
})";
static const char* kFxVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 aPos;
layout(location=2) in vec4 aCol;
layout(location=5) in vec4 iPos;
layout(location=6) in vec4 iRot;
layout(location=7) in vec4 iScl;
layout(location=8) in vec4 iTA;
uniform mat4 uVP;
out vec4 vC;
vec3 qrot(vec4 q, vec3 v){ vec3 t = 2.0*cross(q.xyz, v); return v + q.w*t + cross(q.xyz, t); }
void main(){
  vec3 wp = qrot(iRot, aPos*iScl.xyz) + iPos.xyz;
  vC = vec4(aCol.rgb*iTA.rgb, aCol.a*iTA.a);
  gl_Position = uVP*vec4(wp,1.0);
})";
static const char* kFxFS = R"(#version 300 es
precision mediump float;
in vec4 vC; out vec4 o;
void main(){ o = vC; })";
static const char* kSkyVS = R"(#version 300 es
precision highp float;
out vec2 vP;
void main(){ vec2 p = vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2))*2.0-1.0; vP = p; gl_Position = vec4(p, 0.9999, 1.0); })";
static const char* kSkyFS = R"(#version 300 es
precision highp float;
uniform mat4 uInvVP; uniform vec3 uCam, uTop, uHor, uSunDir, uSunCol, uFog; uniform float uStars, uTime;
in vec2 vP; out vec4 o;
void main(){
  vec4 a = uInvVP*vec4(vP, 1.0, 1.0);
  vec3 dir = normalize(a.xyz/a.w - uCam);
  float h = dir.y;
  vec3 c = mix(uHor, uTop, smoothstep(-0.05, 0.65, h));
  c = mix(uFog, c, smoothstep(-0.25, 0.08, h));
  float s = max(dot(dir, uSunDir), 0.0);
  c += uSunCol*(smoothstep(0.9985, 0.9992, s)*1.2 + pow(s, 10.0)*0.22);
  float band = sin(dir.x*5.0 + uTime*0.01)*sin(dir.z*4.0 - uTime*0.008)*0.5+0.5;
  c = mix(c, c*1.08 + vec3(0.02), smoothstep(0.55, 0.9, band)*smoothstep(0.05, 0.3, h)*smoothstep(0.8, 0.3, h));
  if (uStars > 0.5 && h > 0.04) {
    vec3 q = floor(dir*160.0);
    float hs = fract(sin(dot(q, vec3(12.9898, 78.233, 37.719)))*43758.5453);
    if (hs > 0.9965) c += vec3(0.9)*smoothstep(0.04, 0.35, h)*(0.6+0.4*sin(uTime*3.0+hs*90.0));
  }
  o = vec4(c, 1.0);
})";
static const char* kBillVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec2 aCorner;
layout(location=1) in vec4 iPS;
layout(location=2) in vec4 iUV;
layout(location=3) in vec4 iCol;
layout(location=4) in float iRot;
uniform mat4 uVP; uniform vec3 uRight, uUp, uCam; uniform float uFogDen;
out vec2 vUV; out vec4 vC;
void main(){
  vec2 c = aCorner; float cs = cos(iRot), sn = sin(iRot);
  vec2 r = vec2(c.x*cs - c.y*sn, c.x*sn + c.y*cs);
  vec3 wp = iPS.xyz + (uRight*r.x + uUp*r.y)*iPS.w;
  vUV = vec2(mix(iUV.x, iUV.z, c.x+0.5), mix(iUV.w, iUV.y, c.y+0.5));
  float d = length(iPS.xyz - uCam);
  float f = clamp(1.0 - exp(-(d*uFogDen)*(d*uFogDen)), 0.0, 1.0);
  vC = vec4(iCol.rgb, iCol.a*(1.0-f));
  gl_Position = uVP*vec4(wp,1.0);
})";
static const char* kTexFS = R"(#version 300 es
precision mediump float;
uniform sampler2D uTex; in vec2 vUV; in vec4 vC; out vec4 o;
void main(){ vec4 t = texture(uTex, vUV); o = t*vC; if (o.a < 0.01) discard; })";
static const char* kDecalVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec2 aGrid;
layout(location=1) in vec4 iD;
layout(location=2) in vec4 iUV;
layout(location=3) in vec4 iCol;
layout(location=4) in float iY;
uniform mat4 uVP; uniform highp sampler2D uHeight; uniform vec3 uTerr;
out vec2 vUV; out vec4 vC;
float hAt(ivec2 i){ int n = int(uTerr.z)-1; return texelFetch(uHeight, clamp(i, ivec2(0), ivec2(n)), 0).r; }
float H(vec2 xz){ vec2 f = (xz + uTerr.x)/uTerr.y; ivec2 i = ivec2(floor(f)); vec2 t = fract(f);
  return mix(mix(hAt(i), hAt(i+ivec2(1,0)), t.x), mix(hAt(i+ivec2(0,1)), hAt(i+ivec2(1,1)), t.x), t.y); }
void main(){
  float cs = cos(iD.w), sn = sin(iD.w);
  vec2 g = vec2(aGrid.x*cs - aGrid.y*sn, aGrid.x*sn + aGrid.y*cs)*iD.z;
  vec2 xz = iD.xy + g;
  float y = H(xz) + 0.08 + iY;
  vUV = vec2(mix(iUV.x, iUV.z, aGrid.x*0.5+0.5), mix(iUV.y, iUV.w, aGrid.y*0.5+0.5));
  vC = iCol;
  gl_Position = uVP*vec4(xz.x, y, xz.y, 1.0);
})";
static const char* kPostVS = R"(#version 300 es
precision highp float;
out vec2 vUV;
void main(){ vec2 p = vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2)); vUV = p; gl_Position = vec4(p*2.0-1.0, 0.0, 1.0); })";
static const char* kPostFS = R"(#version 300 es
precision mediump float;
uniform sampler2D uScene; uniform float uVig; in vec2 vUV; out vec4 o;
void main(){ vec3 c = texture(uScene, vUV).rgb; float d = length(vUV-0.5); c *= 1.0 - uVig*smoothstep(0.42, 0.85, d); o = vec4(c, 1.0); })";
static const char* kUIVS = R"(#version 300 es
precision highp float;
layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV; layout(location=2) in vec4 aCol;
uniform vec2 uScreen; out vec2 vUV; out vec4 vC;
void main(){ vUV = aUV; vC = aCol; gl_Position = vec4(aPos.x/uScreen.x*2.0-1.0, 1.0-aPos.y/uScreen.y*2.0, 0.0, 1.0); })";

// ------------------------------------------------------------------ init
void Renderer::compile() {
  progModel = gfxProgram(kModelVS, kModelFS, "model");
  progFx = gfxProgram(kFxVS, kFxFS, "fx");
  progSky = gfxProgram(kSkyVS, kSkyFS, "sky");
  progBill = gfxProgram(kBillVS, kTexFS, "bill");
  progDecal = gfxProgram(kDecalVS, kTexFS, "decal");
  progPost = gfxProgram(kPostVS, kPostFS, "post");
  progUI = gfxProgram(kUIVS, kTexFS, "ui");
  auto L = [](GLuint p, const char* n) { return glGetUniformLocation(p, n); };
  um = {L(progModel, "uVP"), L(progModel, "uCam"), L(progModel, "uSnap"), L(progModel, "uTime"), L(progModel, "uTex"), L(progModel, "uSunDir"), L(progModel, "uSunCol"), L(progModel, "uAmbSky"), L(progModel, "uAmbGnd"), L(progModel, "uFogCol"), L(progModel, "uFogDen")};
  uf = {L(progFx, "uVP"), -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
}

void Renderer::init() {
  glGenVertexArrays(1, &vao); glBindVertexArray(vao);
  compile();
  // texture array of pixel layers
  std::vector<u8> layers; artBuildLayers(layers);
  glGenTextures(1, &texLayers);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texLayers);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, LAYER_SIZE, LAYER_SIZE, L_COUNT, 0, GL_RGBA, GL_UNSIGNED_BYTE, layers.data());
  glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 4);
  // UI atlas
  Tex2D at = gfxTexRGBA(ATLAS_SIZE, ATLAS_SIZE, atlas.px.data(), true, false, false);
  texAtlas = at.id;
  // model buffers
  glGenBuffers(1, &mAnim); glBindBuffer(GL_ARRAY_BUFFER, mAnim); glBufferData(GL_ARRAY_BUFFER, gModels.anim.size() * sizeof(AnimVtx), gModels.anim.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &mStat); glBindBuffer(GL_ARRAY_BUFFER, mStat); glBufferData(GL_ARRAY_BUFFER, gModels.stat.size() * sizeof(StatVtx), gModels.stat.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &mIdx); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIdx); glBufferData(GL_ELEMENT_ARRAY_BUFFER, gModels.idx.size() * sizeof(u16), gModels.idx.data(), GL_STATIC_DRAW);
  instBuf.init(); billBuf.init(); decalBuf.init(); uiBuf.init(); uiIdx.init(GL_ELEMENT_ARRAY_BUFFER);
  float quad[] = {-.5f, -.5f, .5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, .5f, -.5f, .5f};
  glGenBuffers(1, &quadVbo); glBindBuffer(GL_ARRAY_BUFFER, quadVbo); glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  // decal grid 8x8
  const int G = 8; std::vector<float> gv; std::vector<u16> gi;
  for (int j = 0; j <= G; j++) for (int i = 0; i <= G; i++) { gv.push_back(i / (float)G * 2 - 1); gv.push_back(j / (float)G * 2 - 1); }
  for (int j = 0; j < G; j++) for (int i = 0; i < G; i++) { u16 a = j * (G + 1) + i, b = a + 1, c = a + G + 1, d = c + 1; gi.insert(gi.end(), {a, c, d, a, d, b}); }
  glGenBuffers(1, &gridVbo); glBindBuffer(GL_ARRAY_BUFFER, gridVbo); glBufferData(GL_ARRAY_BUFFER, gv.size() * 4, gv.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &gridIbo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridIbo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, gi.size() * 2, gi.data(), GL_STATIC_DRAW);
  gridIdxCount = (int)gi.size();
  ready = true;
  gfxCheck("renderer init");
}

void Renderer::destroyChunks() {
  for (auto& c : chunks) { if (c.vbo) glDeleteBuffers(1, &c.vbo); if (c.ibo) glDeleteBuffers(1, &c.ibo); }
  chunks.clear();
}

void Renderer::destroy() {
  destroyChunks();
  scene.destroy();
  GLuint ps[] = {progModel, progFx, progSky, progBill, progDecal, progPost, progUI};
  for (GLuint p : ps) if (p) glDeleteProgram(p);
  GLuint ts[] = {texLayers, texAtlas, texHeight};
  glDeleteTextures(3, ts);
  GLuint bs[] = {mAnim, mStat, mIdx, quadVbo, gridVbo, gridIbo};
  glDeleteBuffers(6, bs);
  instBuf.destroy(); billBuf.destroy(); decalBuf.destroy(); uiBuf.destroy(); uiIdx.destroy();
  if (vao) glDeleteVertexArrays(1, &vao);
  vao = 0; texHeight = 0; ready = false; world = nullptr;
}

// ------------------------------------------------------------------ world upload
struct TV { float x, y, z; i8 nx, ny, nz, pad; u8 r, g, b, e; float u, v; u8 layer, flag, a, b2; }; // 32 B

void Renderer::setWorld(const World* w) {
  destroyChunks();
  world = w;
  if (!w) return;
  pal = w->pal;
  const Terrain& T = w->T;
  // height texture
  if (texHeight) glDeleteTextures(1, &texHeight);
  glGenTextures(1, &texHeight);
  glBindTexture(GL_TEXTURE_2D, texHeight);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, T.N, T.N, 0, GL_RED, GL_FLOAT, T.h.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // AO map from trees/rocks
  std::vector<float> ao(T.N * T.N, 0.f);
  for (auto& p : w->props) {
    if (p.grass || p.colR <= 0) continue;
    float rad = p.colR * 3.0f + 1.0f;
    int i0 = (int)((p.pos.x - rad + T.half()) / T.cell), i1 = (int)((p.pos.x + rad + T.half()) / T.cell);
    int j0 = (int)((p.pos.z - rad + T.half()) / T.cell), j1 = (int)((p.pos.z + rad + T.half()) / T.cell);
    for (int j = std::max(0, j0); j <= std::min(T.N - 1, j1); j++)
      for (int i = std::max(0, i0); i <= std::min(T.N - 1, i1); i++) {
        float x = -T.half() + i * T.cell, z = -T.half() + j * T.cell;
        float d = sqrtf(sq(x - p.pos.x) + sq(z - p.pos.z));
        ao[j * T.N + i] = std::max(ao[j * T.N + i], sat(1 - d / rad) * 0.45f);
      }
  }
  Col g1 = hexc(pal.ground1), g2 = hexc(pal.ground2), g3 = hexc(pal.ground3), rk = hexc(pal.rock);
  u8 baseLayer = w->map == MAP_DESERT ? L_SAND : (w->map == MAP_GRAVEYARD ? L_GRASS : L_GRASS);
  const int CQ = 15; chunkN = (T.N - 1) / CQ;
  chunks.resize(chunkN * chunkN);
  for (int cz = 0; cz < chunkN; cz++)
    for (int cx = 0; cx < chunkN; cx++) {
      Chunk& C = chunks[cz * chunkN + cx];
      std::vector<TV> V; std::vector<u16> I;
      float ymin = 1e9, ymax = -1e9;
      for (int j = 0; j <= CQ; j++)
        for (int i = 0; i <= CQ; i++) {
          int gi = cx * CQ + i, gj = cz * CQ + j;
          float x = -T.half() + gi * T.cell, z = -T.half() + gj * T.cell, y = T.hv(gi, gj);
          v3 n = T.normal(x, z);
          float nz = fbm(x * 0.05f, z * 0.05f, 31, 3), nz2 = vnoise(x * 0.3f, z * 0.3f, 37);
          Col c = mixc(g1, g2, smooth01((nz - 0.35f) * 3.f));
          float path = fbm(x * 0.03f + 7, z * 0.03f - 3, 41, 2);
          float pathK = 1.f - smooth01(fabsf(path - 0.5f) * 22.f);
          c = mixc(c, g3, pathK * 0.85f);
          float slope = 1 - n.y;
          c = mixc(c, rk, smooth01((slope - 0.25f) * 4.f));
          c = mulc(c, (0.92f + nz2 * 0.16f) * (1.f - ao[gj * T.N + gi]));
          TV t; t.x = x; t.y = y; t.z = z; t.nx = (i8)(n.x * 127); t.ny = (i8)(n.y * 127); t.nz = (i8)(n.z * 127); t.pad = 0;
          t.r = c.r; t.g = c.g; t.b = c.b; t.e = 0; t.u = x * 0.25f; t.v = z * 0.25f;
          t.layer = slope > 0.3f ? L_ROCK : (pathK > 0.5f && w->map != MAP_DESERT ? L_DIRT : baseLayer); t.flag = 0; t.a = t.b2 = 0;
          V.push_back(t); ymin = std::min(ymin, y); ymax = std::max(ymax, y);
        }
      for (int j = 0; j < CQ; j++)
        for (int i = 0; i < CQ; i++) {
          u16 a = j * (CQ + 1) + i, b = a + 1, c = a + CQ + 1, d = c + 1;
          I.insert(I.end(), {a, c, d, a, d, b});
        }
      C.x0 = -T.half() + cx * CQ * T.cell; C.z0 = -T.half() + cz * CQ * T.cell; C.x1 = C.x0 + CQ * T.cell; C.z1 = C.z0 + CQ * T.cell;
      C.y0 = ymin; C.y1 = ymax + 9;
      // merge grass props into the chunk mesh
      for (size_t pi = 0; pi < w->props.size(); pi++) {
        const PropInst& p = w->props[pi];
        if (p.pos.x < C.x0 || p.pos.x >= C.x1 || p.pos.z < C.z0 || p.pos.z >= C.z1) continue;
        if (!p.grass) { C.props.push_back((int)pi); continue; }
        const ModelInfo& mi = gModels.info[p.model];
        if (V.size() + mi.vtx > 65000) continue;
        u16 base = (u16)V.size();
        q4 q = q4::yaw(p.yaw);
        const AnimVtx* av = &gModels.anim[mi.animOff / sizeof(AnimVtx)];
        const StatVtx* sv = &gModels.stat[mi.statOff / sizeof(StatVtx)];
        for (int k = 0; k < mi.vtx; k++) {
          v3 lp(av[k].x, av[k].y, av[k].z);
          v3 wp = q.rot(lp * p.scale) + p.pos;
          TV t; t.x = wp.x; t.y = wp.y; t.z = wp.z; t.nx = 0; t.ny = 127; t.nz = 0; t.pad = 0;
          Col c(sv[k].r, sv[k].g, sv[k].b);
          if (sv[k].f0 == 1) c = Col((u8)std::min(255.f, p.tintA.r * sv[k].r / 128.f), (u8)std::min(255.f, p.tintA.g * sv[k].r / 128.f), (u8)std::min(255.f, p.tintA.b * sv[k].r / 128.f));
          c = mulc(c, 1.f - ao[clampi((int)((p.pos.z + T.half()) / T.cell), 0, T.N - 1) * T.N + clampi((int)((p.pos.x + T.half()) / T.cell), 0, T.N - 1)] * 0.7f);
          t.r = c.r; t.g = c.g; t.b = c.b; t.e = 0; t.u = sv[k].u; t.v = sv[k].v; t.layer = sv[k].layer; t.flag = 3; t.a = t.b2 = 0;
          V.push_back(t);
        }
        const u16* id = &gModels.idx[mi.idxOff / sizeof(u16)];
        for (int k = 0; k < mi.idx; k++) I.push_back(base + id[k]);
      }
      glGenBuffers(1, &C.vbo); glBindBuffer(GL_ARRAY_BUFFER, C.vbo); glBufferData(GL_ARRAY_BUFFER, V.size() * sizeof(TV), V.data(), GL_STATIC_DRAW);
      glGenBuffers(1, &C.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, C.ibo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, I.size() * 2, I.data(), GL_STATIC_DRAW);
      C.idx = (int)I.size();
    }
  gfxCheck("setWorld");
}

// ------------------------------------------------------------------ per frame
void Renderer::beginFrame(int w, int h, float dt) {
  screenW = w; screenH = h; time += dt;
  int nw = std::max(64, (int)(w * set.renderScale)), nh = std::max(64, (int)(h * set.renderScale));
  if (nw != rw || nh != rh) { rw = nw; rh = nh; scene.create(rw, rh, true, false); }
  cmds.clear(); fxCmds.clear(); billAdd.clear(); billAlpha.clear(); decals.clear();
}

void Renderer::setCamera(v3 eye, v3 target, float fovy) {
  camPos = eye; camTarget = target; fov = fovy;
  float viewD = std::min(set.viewDist, 2.2f / std::max(0.002f, pal.fogDensity * fogMul));
  view = m4::lookAt(eye, target, v3(0, 1, 0));
  proj = m4::persp(fovy, (float)screenW / std::max(1, screenH), 0.15f, viewD + 20);
  vp = proj * view;
  camFwd = norm(target - eye); camRight = norm(cross(camFwd, v3(0, 1, 0))); camUp = cross(camRight, camFwd);
  // frustum planes (Gribb/Hartmann), rows of vp
  for (int i = 0; i < 6; i++) {
    int r = i / 2; float s = (i & 1) ? -1.f : 1.f;
    for (int c = 0; c < 4; c++) frustum[i][c] = vp.at(3, c) + s * vp.at(r, c);
    float l = sqrtf(sq(frustum[i][0]) + sq(frustum[i][1]) + sq(frustum[i][2]));
    for (int c = 0; c < 4; c++) frustum[i][c] /= l;
  }
}
bool Renderer::sphereVisible(v3 c, float r) const {
  for (int i = 0; i < 6; i++) if (frustum[i][0] * c.x + frustum[i][1] * c.y + frustum[i][2] * c.z + frustum[i][3] < -r) return false;
  return true;
}
bool Renderer::worldToScreen(v3 p, v2& out) const {
  v4 c = vp.mul4(v4(p, 1));
  if (c.w <= 0.05f) return false;
  out = v2((c.x / c.w * 0.5f + 0.5f) * screenW, (0.5f - c.y / c.w * 0.5f) * screenH);
  return true;
}

void Renderer::model(int m, FrameRef fr, v3 pos, q4 rot, v3 scale, float flash, Col ta, Col tb) {
  DrawCmd d; d.key = ((u32)m << 24) | ((u32)(fr.frame & 0xfff) << 12) | (u32)(fr.next & 0xfff);
  d.in.p[0] = pos.x; d.in.p[1] = pos.y; d.in.p[2] = pos.z; d.in.p[3] = flash;
  d.in.q[0] = rot.x; d.in.q[1] = rot.y; d.in.q[2] = rot.z; d.in.q[3] = rot.w;
  d.in.s[0] = scale.x; d.in.s[1] = scale.y; d.in.s[2] = scale.z; d.in.s[3] = fr.next != fr.frame ? fr.blend : 0.f;
  d.in.ta[0] = ta.r; d.in.ta[1] = ta.g; d.in.ta[2] = ta.b; d.in.ta[3] = ta.a;
  d.in.tb[0] = tb.r; d.in.tb[1] = tb.g; d.in.tb[2] = tb.b; d.in.tb[3] = tb.a;
  if (gModels.info[m].transparent) { d.in.ta[3] = (u8)(ta.a * set.fxAlpha); fxCmds.push_back(d); }
  else cmds.push_back(d);
}

static void spriteUV(const UIAtlas& A, int sprite, u16& u0, u16& v0, u16& u1, u16& v1) {
  const ARect& r = A.sprite[sprite];
  u0 = (u16)((r.x + 0.5f) * 65535 / ATLAS_SIZE); v0 = (u16)((r.y + 0.5f) * 65535 / ATLAS_SIZE);
  u1 = (u16)((r.x + r.w - 0.5f) * 65535 / ATLAS_SIZE); v1 = (u16)((r.y + r.h - 0.5f) * 65535 / ATLAS_SIZE);
}
void Renderer::billboard(v3 pos, float size, int sprite, Col c, bool additive, float rot) {
  BillI b; b.x = pos.x; b.y = pos.y; b.z = pos.z; b.size = size; spriteUV(atlas, sprite, b.u0, b.v0, b.u1, b.v1);
  b.r = c.r; b.g = c.g; b.b = c.b; b.a = (u8)(c.a * set.fxAlpha); b.rot = rot;
  (additive ? billAdd : billAlpha).push_back(b);
}
void Renderer::decal(v3 pos, float radius, int sprite, Col c, float rot, float yoff) {
  DecalI d; d.x = pos.x; d.z = pos.z; d.radius = radius; d.rot = rot; spriteUV(atlas, sprite, d.u0, d.v0, d.u1, d.v1);
  d.r = c.r; d.g = c.g; d.b = c.b; d.a = c.a; d.yoff = yoff;
  decals.push_back(d);
}

void Renderer::setModelUniforms(GLuint prog, U& u) {
  glUseProgram(prog);
  glUniformMatrix4fv(u.vp, 1, GL_FALSE, vp.m);
  if (prog != progModel) return;
  glUniform3f(u.cam, camPos.x, camPos.y, camPos.z);
  float sy = set.snap ? 110.f : 0.f;
  glUniform2f(u.snap, sy * screenW / std::max(1, screenH), sy);
  glUniform1f(u.time, time);
  glUniform1i(u.tex, 0);
  float el = pal.sunElev, an = pal.sunAngle;
  v3 sd = norm(v3(cosf(an) * cosf(el * 1.2f), sinf(el * 1.2f), sinf(an) * cosf(el * 1.2f)));
  glUniform3f(u.sun, sd.x, sd.y, sd.z);
  v3 sc = c2v(hexc(pal.sun)) * (0.45f + el * 0.55f), as = c2v(hexc(pal.ambientSky)) * 0.62f, ag = c2v(hexc(pal.ambientGround)) * 0.8f, fc = c2v(hexc(pal.fog));
  glUniform3f(u.sunCol, sc.x, sc.y, sc.z); glUniform3f(u.ambSky, as.x, as.y, as.z); glUniform3f(u.ambGnd, ag.x, ag.y, ag.z);
  glUniform3f(u.fogCol, fc.x, fc.y, fc.z); glUniform1f(u.fogDen, pal.fogDensity * fogMul);
}

void Renderer::bindModelStreams(const ModelInfo& mi, int frame, int next, size_t instOff) {
  size_t aoff = mi.animOff + (size_t)frame * mi.vtx * sizeof(AnimVtx);
  size_t noff = mi.animOff + (size_t)next * mi.vtx * sizeof(AnimVtx);
  glBindBuffer(GL_ARRAY_BUFFER, mAnim);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AnimVtx), (void*)aoff);
  glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, sizeof(AnimVtx), (void*)(aoff + 12));
  glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(AnimVtx), (void*)noff);
  glVertexAttribPointer(11, 4, GL_BYTE, GL_TRUE, sizeof(AnimVtx), (void*)(noff + 12));
  glBindBuffer(GL_ARRAY_BUFFER, mStat);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(StatVtx), (void*)(mi.statOff));
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(StatVtx), (void*)(mi.statOff + 4));
  glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(StatVtx), (void*)(mi.statOff + 12));
  glBindBuffer(GL_ARRAY_BUFFER, instBuf.id);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(instOff));
  glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(instOff + 16));
  glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Inst), (void*)(instOff + 32));
  glVertexAttribPointer(8, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Inst), (void*)(instOff + 48));
  glVertexAttribPointer(9, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Inst), (void*)(instOff + 52));
}

void Renderer::drawModelCmds(std::vector<DrawCmd>& list, bool fx) {
  if (list.empty()) return;
  std::sort(list.begin(), list.end(), [](const DrawCmd& a, const DrawCmd& b) { return a.key < b.key; });
  static std::vector<Inst> insts; insts.resize(list.size());
  for (size_t i = 0; i < list.size(); i++) insts[i] = list[i].in;
  instBuf.upload(insts.data(), insts.size() * sizeof(Inst));
  for (int a = 0; a < 12; a++) glEnableVertexAttribArray(a);
  for (int a = 5; a < 10; a++) glVertexAttribDivisor(a, 1);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIdx);
  size_t i = 0;
  while (i < list.size()) {
    size_t j = i; u32 key = list[i].key;
    while (j < list.size() && list[j].key == key) j++;
    int m = key >> 24, frame = (key >> 12) & 0xfff, next = key & 0xfff;
    const ModelInfo& mi = gModels.info[m];
    bindModelStreams(mi, frame, next, i * sizeof(Inst));
    glDrawElementsInstanced(GL_TRIANGLES, mi.idx, GL_UNSIGNED_SHORT, (void*)mi.idxOff, (GLsizei)(j - i));
    i = j;
  }
  for (int a = 5; a < 10; a++) glVertexAttribDivisor(a, 0);
  for (int a = 0; a < 12; a++) glDisableVertexAttribArray(a);
  (void)fx;
}

void Renderer::renderScene(bool drawWorld) {
  glBindVertexArray(vao);
  glBindFramebuffer(GL_FRAMEBUFFER, scene.fbo);
  glViewport(0, 0, rw, rh);
  v3 fc = c2v(hexc(pal.fog));
  glClearColor(fc.x, fc.y, fc.z, 1);
  glDepthMask(GL_TRUE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // sky
  glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
  glUseProgram(progSky);
  m4 inv = vp.inverse();
  glUniformMatrix4fv(glGetUniformLocation(progSky, "uInvVP"), 1, GL_FALSE, inv.m);
  v3 top = c2v(hexc(pal.skyTop)), hor = c2v(hexc(pal.skyHorizon)), sc = c2v(hexc(pal.sun));
  float el = pal.sunElev, an = pal.sunAngle; v3 sd = norm(v3(cosf(an) * cosf(el * 1.2f), sinf(el * 1.2f), sinf(an) * cosf(el * 1.2f)));
  glUniform3f(glGetUniformLocation(progSky, "uCam"), camPos.x, camPos.y, camPos.z);
  glUniform3f(glGetUniformLocation(progSky, "uTop"), top.x, top.y, top.z);
  glUniform3f(glGetUniformLocation(progSky, "uHor"), hor.x, hor.y, hor.z);
  glUniform3f(glGetUniformLocation(progSky, "uFog"), fc.x, fc.y, fc.z);
  glUniform3f(glGetUniformLocation(progSky, "uSunDir"), sd.x, sd.y, sd.z);
  glUniform3f(glGetUniformLocation(progSky, "uSunCol"), sc.x, sc.y, sc.z);
  glUniform1f(glGetUniformLocation(progSky, "uStars"), pal.stars ? 1.f : 0.f);
  glUniform1f(glGetUniformLocation(progSky, "uTime"), time);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D_ARRAY, texLayers);
  setModelUniforms(progModel, um);
  float viewD = std::min(set.viewDist, 2.2f / std::max(0.002f, pal.fogDensity * fogMul));
  if (drawWorld && world) {
    // terrain chunks
    for (int a = 0; a < 5; a++) glEnableVertexAttribArray(a);
    glVertexAttrib4f(5, 0, 0, 0, 0); glVertexAttrib4f(6, 0, 0, 0, 1); glVertexAttrib4f(7, 1, 1, 1, 0); glVertexAttrib4f(8, 1, 1, 1, 0); glVertexAttrib4f(9, 1, 1, 1, 1); glVertexAttrib4f(10, 0, 0, 0, 1); glVertexAttrib4f(11, 0, 0, 1, 0);
    for (auto& C : chunks) {
      v3 c((C.x0 + C.x1) * .5f, (C.y0 + C.y1) * .5f, (C.z0 + C.z1) * .5f);
      float r = len(v3(C.x1 - C.x0, C.y1 - C.y0, C.z1 - C.z0)) * .5f;
      if (!sphereVisible(c, r) || distXZ(c, camPos) - r > viewD) continue;
      glBindBuffer(GL_ARRAY_BUFFER, C.vbo);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 32, (void*)0);
      glVertexAttribPointer(1, 4, GL_BYTE, GL_TRUE, 32, (void*)12);
      glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 32, (void*)16);
      glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 32, (void*)20);
      glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, 32, (void*)28);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, C.ibo);
      glDisable(GL_CULL_FACE);
      glDrawElements(GL_TRIANGLES, C.idx, GL_UNSIGNED_SHORT, 0);
      glEnable(GL_CULL_FACE);
      // queue props of visible chunk
      for (int pi : C.props) {
        const PropInst& p = world->props[pi];
        float d = distXZ(p.pos, camPos);
        float rad = gModels.info[p.model].radius * p.scale + 2;
        if (d > viewD + rad) continue;
        if (rad < 2.5f && d > viewD * 0.55f) continue;
        if (!sphereVisible(p.pos + v3(0, 2, 0), rad + 3)) continue;
        model(p.model, gModels.info[p.model].clip[CL_IDLE].first, p.pos, q4::yaw(p.yaw), v3(p.scale, p.scale, p.scale), 0, p.tintA, p.tintB);
      }
    }
    for (int a = 0; a < 5; a++) glDisableVertexAttribArray(a);
  }
  drawModelCmds(cmds, false);
  // decals
  glDepthMask(GL_FALSE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDisable(GL_CULL_FACE);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texAtlas);
  if (!decals.empty() && world) {
    glUseProgram(progDecal);
    glUniformMatrix4fv(glGetUniformLocation(progDecal, "uVP"), 1, GL_FALSE, vp.m);
    glUniform1i(glGetUniformLocation(progDecal, "uTex"), 0);
    glUniform1i(glGetUniformLocation(progDecal, "uHeight"), 1);
    glUniform3f(glGetUniformLocation(progDecal, "uTerr"), world->T.half(), world->T.cell, (float)world->T.N);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, texHeight); glActiveTexture(GL_TEXTURE0);
    decalBuf.upload(decals.data(), decals.size() * sizeof(DecalI));
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, 0);
    glBindBuffer(GL_ARRAY_BUFFER, decalBuf.id);
    for (int a = 1; a <= 4; a++) { glEnableVertexAttribArray(a); glVertexAttribDivisor(a, 1); }
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_SHORT, GL_TRUE, 32, (void*)16);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 32, (void*)24);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 32, (void*)28);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridIbo);
    glDrawElementsInstanced(GL_TRIANGLES, gridIdxCount, GL_UNSIGNED_SHORT, 0, (GLsizei)decals.size());
    for (int a = 1; a <= 4; a++) { glVertexAttribDivisor(a, 0); glDisableVertexAttribArray(a); }
    glDisableVertexAttribArray(0);
  }
  // fx meshes (additive)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  if (!fxCmds.empty()) { setModelUniforms(progFx, uf); drawModelCmds(fxCmds, true); }
  // billboards
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texAtlas);
  auto drawBills = [&](std::vector<BillI>& list) {
    if (list.empty()) return;
    glUseProgram(progBill);
    glUniformMatrix4fv(glGetUniformLocation(progBill, "uVP"), 1, GL_FALSE, vp.m);
    glUniform3f(glGetUniformLocation(progBill, "uRight"), camRight.x, camRight.y, camRight.z);
    glUniform3f(glGetUniformLocation(progBill, "uUp"), camUp.x, camUp.y, camUp.z);
    glUniform3f(glGetUniformLocation(progBill, "uCam"), camPos.x, camPos.y, camPos.z);
    glUniform1f(glGetUniformLocation(progBill, "uFogDen"), pal.fogDensity * fogMul * 0.7f);
    glUniform1i(glGetUniformLocation(progBill, "uTex"), 0);
    billBuf.upload(list.data(), list.size() * sizeof(BillI));
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, 0);
    glBindBuffer(GL_ARRAY_BUFFER, billBuf.id);
    for (int a = 1; a <= 4; a++) { glEnableVertexAttribArray(a); glVertexAttribDivisor(a, 1); }
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void*)0);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_SHORT, GL_TRUE, 32, (void*)16);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 32, (void*)24);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 32, (void*)28);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)list.size());
    for (int a = 1; a <= 4; a++) { glVertexAttribDivisor(a, 0); glDisableVertexAttribArray(a); }
    glDisableVertexAttribArray(0);
  };
  drawBills(billAdd);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  drawBills(billAlpha);
  glDepthMask(GL_TRUE);
  // post: upscale to screen (nearest => chunky pixels)
  glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
  glViewport(0, 0, screenW, screenH);
  glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
  glUseProgram(progPost);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, scene.color);
  glUniform1i(glGetUniformLocation(progPost, "uScene"), 0);
  glUniform1f(glGetUniformLocation(progPost, "uVig"), set.vignette);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  gfxCheck("renderScene");
}

// ------------------------------------------------------------------ UI
void Renderer::uiQuad(float x, float y, float w, float h, const ARect& r, Col c) {
  if (uiVerts.size() > 60000) uiFlush();
  u16 base = (u16)uiVerts.size();
  float u0 = r.x / (float)ATLAS_SIZE, v0 = r.y / (float)ATLAS_SIZE, u1 = (r.x + r.w) / (float)ATLAS_SIZE, v1 = (r.y + r.h) / (float)ATLAS_SIZE;
  uiVerts.push_back({x, y, u0, v0, c.r, c.g, c.b, c.a});
  uiVerts.push_back({x + w, y, u1, v0, c.r, c.g, c.b, c.a});
  uiVerts.push_back({x + w, y + h, u1, v1, c.r, c.g, c.b, c.a});
  uiVerts.push_back({x, y + h, u0, v1, c.r, c.g, c.b, c.a});
  uiIndices.insert(uiIndices.end(), {base, (u16)(base + 1), (u16)(base + 2), base, (u16)(base + 2), (u16)(base + 3)});
}
void Renderer::uiRect(float x, float y, float w, float h, Col c) {
  ARect r = atlas.sprite[SP_WHITE]; r.x += 1; r.y += 1; r.w = 2; r.h = 2;
  uiQuad(x, y, w, h, r, c);
}
void Renderer::uiFlush() {
  if (uiVerts.empty()) return;
  glBindVertexArray(vao);
  glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
  glViewport(0, 0, screenW, screenH);
  glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(progUI);
  glUniform2f(glGetUniformLocation(progUI, "uScreen"), (float)screenW, (float)screenH);
  glUniform1i(glGetUniformLocation(progUI, "uTex"), 0);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texAtlas);
  uiBuf.upload(uiVerts.data(), uiVerts.size() * sizeof(UIVert));
  uiIdx.upload(uiIndices.data(), uiIndices.size() * 2);
  glBindBuffer(GL_ARRAY_BUFFER, uiBuf.id);
  glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVert), (void*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVert), (void*)8);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UIVert), (void*)16);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uiIdx.id);
  glDrawElements(GL_TRIANGLES, (GLsizei)uiIndices.size(), GL_UNSIGNED_SHORT, 0);
  glDisableVertexAttribArray(0); glDisableVertexAttribArray(1); glDisableVertexAttribArray(2);
  uiVerts.clear(); uiIndices.clear();
}

void Renderer::readPixels(std::vector<u8>& out, int& w, int& h) {
  w = screenW; h = screenH; out.resize((size_t)w * h * 4);
  glBindFramebuffer(GL_FRAMEBUFFER, screenFbo);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
}
