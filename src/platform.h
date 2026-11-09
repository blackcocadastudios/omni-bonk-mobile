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
};

// provided by the platform layer
void plat_vibrate(int ms);
void plat_quit();

// provided by the app
void app_init(const PlatformInfo& info);
void app_gl_init();          // GL context (re)created: (re)build GPU resources
void app_gl_lost();          // GL context destroyed
void app_resize(int w, int h);
void app_event(const InputEv& e);
void app_frame(float dt);    // update + render
void app_pause();
void app_resume();
void app_save();
void app_audio(float* stereo, int frames, int rate); // audio thread
int app_fps_cap();
void app_set_insets(float l, float r, float t, float b);
