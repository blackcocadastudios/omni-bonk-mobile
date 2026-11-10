#include "audio.h"
#include "base.h"
#include <atomic>

static int gRate = 48000;
static std::vector<float> gSfx[SFX_COUNT];
static std::atomic<float> gMusicVol{0.55f}, gSfxVol{0.8f}, gIntensity{0.f};
static std::atomic<int> gTrack{-1};

// SPSC ring of play commands
struct Cmd { int sfx; float vol, pitch; bool attract; };
static Cmd gQ[256];
static std::atomic<int> gQHead{0}, gQTail{0};

struct Voice { int sfx = -1; float pos = 0, vol = 1, pitch = 1; };
static Voice gV[24];
static float gLastPlay[SFX_COUNT];
static double gClock = 0;
static bool gAttract = false;                 // written by the main thread only
static std::atomic<bool> gFlush{false};        // asks the audio thread to clear its voices

static float osc(int type, float ph) {
  ph -= floorf(ph);
  switch (type) {
    case 0: return ph < 0.5f ? 0.6f : -0.6f;                    // square
    case 1: return 4.f * fabsf(ph - 0.5f) - 1.f;                 // triangle
    case 2: return (ph * 2 - 1) * 0.7f;                          // saw
    default: return sinf(ph * TAU);
  }
}
static u32 gNoise = 22222;
static float noise() { gNoise ^= gNoise << 13; gNoise ^= gNoise >> 17; gNoise ^= gNoise << 5; return (gNoise & 0xffff) / 32768.f - 1.f; }

// generic synth: frequency sweep f0->f1, wave type, noise mix, duration, envelope (attack, decay curve)
static std::vector<float> synth(float dur, std::function<float(float t, float& ph)> f) {
  int n = (int)(dur * gRate); std::vector<float> v(n); float ph = 0;
  for (int i = 0; i < n; i++) { float t = i / (float)gRate; v[i] = f(t, ph); }
  return v;
}
static float env(float t, float dur, float a = 0.005f) { if (t < a) return t / a; return std::max(0.f, 1 - (t - a) / (dur - a)); }

static void buildSfx() {
  auto sweep = [](float dur, float f0, float f1, int wave, float nz, float vol) {
    return synth(dur, [=](float t, float& ph) { float k = t / dur; float f = f0 * powf(f1 / f0, k); ph += f / gRate; float e = env(t, dur); return (osc(wave, ph) * (1 - nz) + noise() * nz) * e * e * vol; });
  };
  gSfx[SFX_JUMP] = sweep(0.12f, 220, 640, 0, 0.0f, 0.35f);
  gSfx[SFX_SLIDE] = synth(0.25f, [](float t, float&) { static float lp = 0; lp += (noise() - lp) * 0.08f; return lp * env(t, 0.25f) * 1.2f; });
  gSfx[SFX_HIT] = synth(0.04f, [](float t, float&) { return noise() * env(t, 0.04f) * 0.35f; });
  gSfx[SFX_KILL] = synth(0.09f, [](float t, float& ph) { float f = 700 - t * 5000; ph += std::max(80.f, f) / gRate; return (osc(3, ph) * 0.7f + noise() * 0.3f) * env(t, 0.09f) * 0.35f; });
  gSfx[SFX_XP] = sweep(0.05f, 1300, 1700, 3, 0, 0.18f);
  gSfx[SFX_COIN] = synth(0.14f, [](float t, float& ph) { float f = t < 0.05f ? 988 : 1319; ph += f / gRate; return osc(0, ph) * env(t, 0.14f) * 0.22f; });
  auto arp = [](std::vector<float> notes, float step, int wave, float vol) {
    float dur = notes.size() * step + 0.25f;
    return synth(dur, [=](float t, float& ph) { int i = std::min((int)notes.size() - 1, (int)(t / step)); ph += notes[i] / gRate; float lt = t - i * step; float e = std::max(0.f, 1 - lt / (step * 2.5f)); float tail = t > notes.size() * step ? std::max(0.f, 1 - (t - notes.size() * step) / 0.25f) : 1; return osc(wave, ph) * e * tail * vol; });
  };
  gSfx[SFX_LEVELUP] = arp({523, 659, 784, 1047, 1319}, 0.07f, 0, 0.28f);
  gSfx[SFX_CHEST] = arp({392, 523, 659, 784, 1047, 1568}, 0.06f, 1, 0.4f);
  gSfx[SFX_SHRINE] = synth(0.7f, [](float t, float& ph) { ph += 1.f / gRate; float s = osc(3, ph * 440) + osc(3, ph * 554) * 0.7f + osc(3, ph * 659) * 0.6f; float e = sinf(std::min(1.f, t / 0.7f) * PI); return s * e * 0.15f; });
  gSfx[SFX_DENY] = sweep(0.2f, 160, 120, 0, 0.1f, 0.3f);
  gSfx[SFX_PORTAL] = synth(0.9f, [](float t, float& ph) { float f = 200 + 600 * sinf(t / 0.9f * PI); ph += f / gRate; return (osc(1, ph) * 0.6f + noise() * 0.2f) * sinf(t / 0.9f * PI) * 0.35f; });
  gSfx[SFX_BOSS] = synth(1.3f, [](float t, float& ph) { float f = 98 * (1 + 0.02f * sinf(t * 30)); ph += f / gRate; return (osc(2, ph) * 0.7f + osc(2, ph * 1.5f) * 0.4f + noise() * 0.15f * env(t, 0.4f)) * env(t, 1.3f, 0.08f) * 0.45f; });
  gSfx[SFX_SWARM] = synth(0.9f, [](float t, float& ph) { float f = ((int)(t * 6) % 2) ? 660 : 880; ph += f / gRate; return osc(0, ph) * env(t, 0.9f, 0.02f) * 0.2f; });
  gSfx[SFX_EXPLODE] = synth(0.5f, [](float t, float&) { static float lp = 0; lp += (noise() - lp) * (0.25f - t * 0.4f > 0.02f ? 0.25f - t * 0.4f : 0.02f); return lp * env(t, 0.5f) * 1.4f; });
  gSfx[SFX_POWERUP] = synth(0.35f, [](float t, float& ph) { float f = 400 * powf(3.f, t / 0.35f) * (1 + 0.05f * sinf(t * 80)); ph += f / gRate; return osc(0, ph) * env(t, 0.35f) * 0.25f; });
  gSfx[SFX_HEAL] = arp({523, 659, 880}, 0.06f, 3, 0.3f);
  gSfx[SFX_BREAK] = synth(0.15f, [](float t, float&) { return noise() * env(t, 0.15f) * env(t, 0.15f) * 0.5f; });
  gSfx[SFX_HURT] = sweep(0.16f, 240, 100, 0, 0.3f, 0.4f);
  gSfx[SFX_DEATH] = sweep(0.9f, 420, 50, 2, 0.1f, 0.4f);
  gSfx[SFX_VICTORY] = arp({523, 659, 784, 659, 784, 1047, 1319, 1568}, 0.11f, 0, 0.3f);
  gSfx[SFX_SHOOT] = synth(0.06f, [](float t, float& ph) { float f = 900 - t * 9000; ph += std::max(120.f, f) / gRate; return (osc(0, ph) * 0.5f + noise() * 0.5f) * env(t, 0.06f) * 0.18f; });
  gSfx[SFX_SLASH] = synth(0.12f, [](float t, float&) { static float lp = 0, hp = 0; float n = noise(); lp += (n - lp) * 0.3f; hp = n - lp; return hp * sinf(t / 0.12f * PI) * 0.35f; });
  gSfx[SFX_ZAP] = synth(0.16f, [](float t, float& ph) { ph += (1200 + noise() * 800) / gRate; return (osc(0, ph) * 0.4f + noise() * 0.4f) * env(t, 0.16f) * 0.25f; });
  gSfx[SFX_FIRE] = synth(0.2f, [](float t, float&) { static float lp = 0; lp += (noise() - lp) * 0.12f; return lp * sinf(t / 0.2f * PI) * 0.6f; });
  gSfx[SFX_CLICK] = synth(0.03f, [](float t, float& ph) { ph += 1800.f / gRate; return osc(0, ph) * env(t, 0.03f) * 0.2f; });
}

void audioInit(int sampleRate) {
  gRate = sampleRate > 0 ? sampleRate : 48000;
  buildSfx();
  for (auto& t : gLastPlay) t = -1;
}

void audioPlay(int sfx, float vol, float pitch) {
  if (sfx < 0 || sfx >= SFX_COUNT) return;
  int h = gQHead.load(std::memory_order_relaxed), nh = (h + 1) & 255;
  if (nh == gQTail.load(std::memory_order_acquire)) return;
  gQ[h] = {sfx, gAttract ? vol * 0.3f : vol, pitch, gAttract};
  gQHead.store(nh, std::memory_order_release);
}
void audioAttract(bool on) { gAttract = on; }
void audioFlush() { gFlush.store(true, std::memory_order_release); }
void audioMusic(int track) { gTrack.store(track); }
void audioSetVolume(float music, float sfx) { gMusicVol.store(music); gSfxVol.store(sfx); }
void audioSetIntensity(float x) { gIntensity.store(clampf(x, 0, 1)); }

// ---------------------------------------------------------------- music sequencer
struct Song { float bpm; int root; int scale[7]; int prog[4]; int wave; float swing; };
static const Song kSongs[5] = {
  {100, 57, {0, 2, 3, 5, 7, 8, 10}, {0, 5, 2, 6}, 1, 0},   // menu: A minor, chill
  {142, 57, {0, 2, 3, 5, 7, 8, 10}, {0, 5, 3, 6}, 0, 0},   // forest: A minor, upbeat
  {132, 52, {0, 1, 4, 5, 7, 8, 10}, {0, 1, 0, 6}, 0, 0},   // desert: E phrygian dominant
  {120, 50, {0, 2, 3, 5, 6, 8, 10}, {0, 4, 5, 3}, 1, 0},   // graveyard: D locrian-ish
  {156, 45, {0, 1, 3, 5, 7, 8, 10}, {0, 1, 5, 6}, 2, 0},   // boss
};
static float midiHz(float n) { return 440.f * powf(2.f, (n - 69) / 12.f); }
struct MusicState { double t = 0; int track = -1; float phB = 0, phA = 0, phL = 0, kickPh = 0; float nz = 0; float fade = 0; };
static MusicState gM;

static float musicSample() {
  int tr = gTrack.load();
  if (tr != gM.track) { gM.fade -= 1.f / (gRate * 0.4f); if (gM.fade <= 0) { gM.track = tr; gM.t = 0; gM.fade = 0; } }
  else if (gM.fade < 1) gM.fade = std::min(1.f, gM.fade + 1.f / (gRate * 0.6f));
  if (gM.track < 0) return 0;
  const Song& s = kSongs[gM.track];
  float beat = (float)(gM.t * s.bpm / 60.0);
  float bar = beat / 4.f; int barI = (int)bar; int chordDeg = s.prog[barI % 4];
  float inten = gIntensity.load();
  auto noteOf = [&](int deg, int oct) { int d = ((deg % 7) + 7) % 7; int o = (deg - d) / 7; return s.root + s.scale[d] + 12 * (o + oct); };
  // bass: 8ths, root/fifth pattern
  float e8 = beat * 2; int e8i = (int)e8; float e8f = e8 - e8i;
  int bassDeg = chordDeg + ((e8i % 8 == 3 || e8i % 8 == 6) ? 4 : 0);
  float bf = midiHz((float)noteOf(bassDeg, -1));
  gM.phB += bf / gRate;
  float bass = osc(1, gM.phB) * (1 - e8f * 0.6f) * 0.32f;
  // arpeggio 16ths over chord tones
  float e16 = beat * 4; int e16i = (int)e16; float e16f = e16 - e16i;
  int arpSteps[8] = {0, 2, 4, 7, 4, 2, 0, 4};
  int ad = chordDeg + arpSteps[e16i % 8];
  float af = midiHz((float)noteOf(ad, 1));
  gM.phA += af / gRate;
  float arp = osc(s.wave, gM.phA) * expf(-e16f * 4.f) * 0.11f * (0.6f + 0.4f * inten);
  // lead: slow melody every half bar (only with intensity)
  float lead = 0;
  if (inten > 0.3f || gM.track == 0) {
    int hb = (int)(beat / 2); u32 h = hash32(hb * 977 + gM.track * 13);
    int ld = chordDeg + (int)(h % 5) * 2 % 7 + 7;
    float lf = midiHz((float)noteOf(ld, 1)) * (1 + 0.004f * sinf((float)gM.t * 30));
    gM.phL += lf / gRate;
    float lp = beat / 2 - hb;
    lead = osc(3, gM.phL) * std::min(1.f, lp * 8) * expf(-lp * 1.5f) * 0.10f * (gM.track == 0 ? 1.f : inten);
  }
  // drums
  float bt = beat - floorf(beat);
  float kick = 0, snare = 0, hat = 0;
  if (gM.track != 0) {
    float kf = 150 * expf(-bt * 18) + 45; gM.kickPh += kf / gRate;
    kick = osc(3, gM.kickPh) * expf(-bt * 10) * 0.42f;
    int bi = (int)beat % 4;
    if (bi == 1 || bi == 3) snare = noise() * expf(-bt * 14) * 0.2f;
    float ht = e8 - floorf(e8); hat = noise() * expf(-ht * 40) * 0.06f * (0.5f + inten);
  }
  gM.t += 1.0 / gRate;
  return (bass + arp + lead + kick + snare + hat) * gM.fade;
}

void audioRender(float* out, int frames) {
  // pull commands
  if (gFlush.exchange(false, std::memory_order_acq_rel)) {
    // stream was stopped/restarted: discard stale commands so they do not burst out all at once
    for (auto& v : gV) v.sfx = -1;
    gQTail.store(gQHead.load(std::memory_order_acquire), std::memory_order_release);
    for (auto& tl : gLastPlay) tl = -1;
  }
  int t = gQTail.load(std::memory_order_relaxed);
  while (t != gQHead.load(std::memory_order_acquire)) {
    Cmd c = gQ[t];
    t = (t + 1) & 255;
    if (gClock - gLastPlay[c.sfx] < (c.attract ? 0.25 : 0.03)) continue; // de-dupe spam (demo run is limited harder)
    gLastPlay[c.sfx] = (float)gClock;
    int best = 0; float bp = -1;
    for (int i = 0; i < 24; i++) { if (gV[i].sfx < 0) { best = i; bp = 1e9f; break; } if (gV[i].pos > bp) { bp = gV[i].pos; best = i; } }
    float jitter = (c.sfx == SFX_XP || c.sfx == SFX_KILL || c.sfx == SFX_COIN || c.sfx == SFX_HIT) ? 0.9f + (hash32((u32)(gClock * 1000)) % 100) / 500.f : 1.f;
    gV[best] = {c.sfx, 0, c.vol, c.pitch * jitter};
  }
  gQTail.store(t, std::memory_order_release);
  float mv = gMusicVol.load(), sv = gSfxVol.load();
  for (int i = 0; i < frames; i++) {
    float s = 0;
    for (auto& v : gV) {
      if (v.sfx < 0) continue;
      const std::vector<float>& b = gSfx[v.sfx];
      int ip = (int)v.pos; if (ip >= (int)b.size()) { v.sfx = -1; continue; }
      s += b[ip] * v.vol; v.pos += v.pitch;
    }
    float m = musicSample() * mv;
    float o = s * sv + m;
    o = o / (1 + fabsf(o) * 0.6f); // soft clip
    out[i * 2] = o; out[i * 2 + 1] = o;
  }
  gClock += frames / (double)gRate;
}
