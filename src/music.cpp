// Music pack streaming: a worker thread decodes OGG Vorbis (stb_vorbis, public domain) into a lock-free ring;
// the audio callback only copies frames out of it, so a slow SD card or decoder hiccup can never block audio.
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_MAX_CHANNELS 2
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "third_party/stb_vorbis.c"
#undef L   // stb_vorbis leaves these one-letter macros defined; they would break the C++ headers below
#undef C
#undef R
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "music.h"
#include "base.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <sys/stat.h>

static const char* kSlotName[MUSIC_SLOTS] = {"menu", "forest", "desert", "graveyard", "boss", "final", "frostpeak", "caldera"};
static std::string gPath[MUSIC_SLOTS];
static int gOutRate = 48000;

static constexpr unsigned RING = 1u << 16;   // stereo frames (~1.4 s at 48 kHz)
static constexpr unsigned PREBUFFER = 8192;  // frames decoded before a new stream is handed to the audio thread
static float gRing[RING * 2];
static std::atomic<unsigned> gHead{0}, gTail{0};       // frame counters: head advanced by the worker, tail by the audio thread
static std::atomic<int> gReqSlot{-1};
static std::atomic<unsigned> gReqGen{0}, gReadyGen{0}; // the audio thread reads the ring only while ready == requested
static std::atomic<int> gUnderruns{0};
static unsigned gAudioGen = 0;                         // audio thread's copy of the last requested generation
static bool gStarted = false;

static void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static void worker() {
  stb_vorbis* v = nullptr; unsigned gen = 0; int ch = 2; bool ready = true;
  double step = 1, frac = 0; float prev[2] = {0, 0}, cur[2] = {0, 0}; bool havePrev = false;
  static float dec[1024 * 2]; int empty = 0;
  for (;;) {
    unsigned g = gReqGen.load(std::memory_order_acquire);
    if (g != gen) {
      gen = g; ready = false;
      if (v) { stb_vorbis_close(v); v = nullptr; }
      int slot = gReqSlot.load(std::memory_order_acquire);
      if (slot >= 0 && slot < MUSIC_SLOTS && !gPath[slot].empty()) {
        int err = 0; v = stb_vorbis_open_filename(gPath[slot].c_str(), &err, nullptr);
        if (v) { stb_vorbis_info in = stb_vorbis_get_info(v); ch = std::min(2, std::max(1, in.channels)); step = in.sample_rate / (double)gOutRate; }
      }
      frac = 0; havePrev = false;
      gHead.store(0, std::memory_order_relaxed); gTail.store(0, std::memory_order_release);
      if (!v) { gReadyGen.store(gen, std::memory_order_release); ready = true; }
    }
    if (!v) { sleepMs(10); continue; }
    unsigned h = gHead.load(std::memory_order_relaxed), t = gTail.load(std::memory_order_acquire);
    if (RING - (h - t) < 8192) { if (!ready) { gReadyGen.store(gen, std::memory_order_release); ready = true; } sleepMs(4); continue; }
    int n = stb_vorbis_get_samples_float_interleaved(v, ch, dec, 1024 * ch);
    if (n <= 0) {   // end of file: loop the whole track (give up on a file that keeps returning nothing)
      if (++empty > 3) { stb_vorbis_close(v); v = nullptr; if (!ready) { gReadyGen.store(gen, std::memory_order_release); ready = true; } }
      else stb_vorbis_seek_start(v);
      continue;
    }
    empty = 0;
    for (int i = 0; i < n; i++) {
      float l = dec[i * ch], r = dec[i * ch + ch - 1];
      if (step == 1) { gRing[(h & (RING - 1)) * 2] = l; gRing[(h & (RING - 1)) * 2 + 1] = r; h++; continue; }
      // linear resampling for files that are not at the output rate (the pack script writes 48 kHz, so normally unused)
      if (!havePrev) { prev[0] = l; prev[1] = r; havePrev = true; continue; }
      cur[0] = l; cur[1] = r;
      while (frac < 1) {
        gRing[(h & (RING - 1)) * 2] = prev[0] + (cur[0] - prev[0]) * (float)frac;
        gRing[(h & (RING - 1)) * 2 + 1] = prev[1] + (cur[1] - prev[1]) * (float)frac;
        h++; frac += step;
      }
      frac -= 1; prev[0] = cur[0]; prev[1] = cur[1];
    }
    gHead.store(h, std::memory_order_release);
    if (!ready && h - t >= PREBUFFER) { gReadyGen.store(gen, std::memory_order_release); ready = true; }
  }
}

int musicPackInit(const std::string& dir, int rate) {
  if (gStarted || dir.empty()) return 0;
  gOutRate = rate > 0 ? rate : 48000;
  int found = 0; std::string list;
  for (int i = 0; i < MUSIC_SLOTS; i++) {
    std::string p = dir + "/" + kSlotName[i] + ".ogg";
    struct stat st;
    if (stat(p.c_str(), &st) != 0 || st.st_size <= 0) continue;
    int err = 0; stb_vorbis* v = stb_vorbis_open_filename(p.c_str(), &err, nullptr);
    if (!v) { LOGE("music pack: %s is not a readable OGG Vorbis file (error %d)", p.c_str(), err); continue; }
    stb_vorbis_info in = stb_vorbis_get_info(v);
    float secs = stb_vorbis_stream_length_in_seconds(v);
    stb_vorbis_close(v);
    gPath[i] = p; found++;
    list += fmt(" %s(%.0fs %dHz %dch)", kSlotName[i], secs, in.sample_rate, in.channels);
  }
  if (!found) { LOGI("music pack: none in %s (chiptune)", dir.c_str()); return 0; }
  LOGI("music pack: %d track(s) in %s:%s", found, dir.c_str(), list.c_str());
  gStarted = true;
  std::thread(worker).detach();
  return found;
}

bool musicPackHas(int slot) { return slot >= 0 && slot < MUSIC_SLOTS && !gPath[slot].empty(); }

void musicPackStart(int slot) {
  gReqSlot.store(slot, std::memory_order_release);
  gAudioGen = gReqGen.fetch_add(1, std::memory_order_acq_rel) + 1;
}

int musicPackRead(float* lr, int frames) {
  int n = 0;
  if (gStarted && gReadyGen.load(std::memory_order_acquire) == gAudioGen) {
    unsigned h = gHead.load(std::memory_order_acquire), t = gTail.load(std::memory_order_relaxed);
    n = (int)std::min<unsigned>(h - t, (unsigned)frames);
    for (int i = 0; i < n; i++, t++) { lr[i * 2] = gRing[(t & (RING - 1)) * 2]; lr[i * 2 + 1] = gRing[(t & (RING - 1)) * 2 + 1]; }
    gTail.store(t, std::memory_order_release);
    if (n < frames) gUnderruns.fetch_add(1, std::memory_order_relaxed);
  }
  for (int i = n; i < frames; i++) lr[i * 2] = lr[i * 2 + 1] = 0;
  return n;
}

int musicPackUnderruns() { return gUnderruns.load(std::memory_order_relaxed); }
