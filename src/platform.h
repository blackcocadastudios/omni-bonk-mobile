// Platform <-> App interface
#pragma once
#include "base.h"

enum EvType { EV_DOWN, EV_MOVE, EV_UP, EV_CANCEL, EV_BACK };
struct InputEv { EvType type; int id; float x, y; };

struct PlatformInfo {
  int width = 1280, height = 720;
  float density = 2.f;       // px per dp
  std::string dataDir = "."; // writable dir for saves
  bool android = false;
  float safeLeft = 0, safeRight = 0, safeTop = 0, safeBottom = 0; // cutout insets (px)
  std::string args;          // launch arguments (Android intent extra "args"), e.g. "demo=1,0"
  bool handheld = false;     // gamepad-only device (R36S & co.): pad-driven UI, no touch controls
  std::string deviceName;    // shown on the title screen (handheld builds)
};

// gamepad state, sent by the platform layer once per frame (SDL button names: A = east on Nintendo-style pads)
enum PadBtn : unsigned {
  PB_A = 1, PB_B = 2, PB_X = 4, PB_Y = 8, PB_L1 = 16, PB_R1 = 32, PB_L2 = 64, PB_R2 = 128,
  PB_SELECT = 256, PB_START = 512, PB_UP = 1024, PB_DOWN = 2048, PB_LEFT = 4096, PB_RIGHT = 8192, PB_L3 = 16384, PB_R3 = 32768
};
struct PadState { float lx = 0, ly = 0, rx = 0, ry = 0; unsigned buttons = 0; bool connected = false; };

// provided by the platform layer
void plat_vibrate(int ms);
void plat_quit();

// provided by the app
void app_init(const PlatformInfo& info);
void app_gl_init();          // GL context (re)created: (re)build GPU resources
void app_gl_lost();          // GL context destroyed
void app_resize(int w, int h);
void app_event(const InputEv& e);
void app_gamepad(const PadState& s); // before app_frame
void app_frame(float dt);    // update + render
void app_pause();
void app_resume();
void app_save();
void app_audio(float* stereo, int frames, int rate); // audio thread
int app_fps_cap();
std::string app_status();    // one-line state summary for platform logs
void app_set_insets(float l, float r, float t, float b);
