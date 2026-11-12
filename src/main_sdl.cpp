// SDL2 platform layer: Linux handhelds (R36S and other RK3326 / PortMaster devices) and desktop testing.
//   video : SDL2 window + OpenGL ES 3.0 context (KMSDRM on the handheld), entry points loaded at runtime
//   input : every connected SDL game controller is merged into one PadState; keyboard + mouse on desktop
//   audio : SDL audio callback, float stereo 48 kHz
// Diagnostics go to stdout (the launch script tees them into log.txt next to the binary).
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "platform.h"
#include "gl.h"

#define X(ret, name, args) ret (*omni_##name) args = nullptr;
OMNI_GL_FUNCS(X)
#undef X

#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#endif

static SDL_Window* gWin = nullptr;
static SDL_GLContext gCtx = nullptr;
static volatile sig_atomic_t gRunning = 1;
static std::vector<SDL_GameController*> gPads;
static std::vector<SDL_Joystick*> gRawJoys;   // joysticks SDL has no mapping for (generic layout fallback)
static SDL_AudioDeviceID gAudio = 0;
static int gAudioRate = 48000;
static bool gHandheld = false;

void plat_quit() { gRunning = 0; }
void plat_vibrate(int ms) {
  for (SDL_GameController* c : gPads) SDL_GameControllerRumble(c, 0x5000, 0x7000, (Uint32)ms);
}
static void onSignal(int) { gRunning = 0; }

// ---------------------------------------------------------------- GL
static bool loadGL() {
  int missing = 0;
#define X(ret, name, args) omni_##name = (ret (*) args)SDL_GL_GetProcAddress(#name); if (!omni_##name) { LOGE("GL: missing entry point %s", #name); missing++; }
  OMNI_GL_FUNCS(X)
#undef X
  return missing == 0;
}

static bool createWindow(int w, int h, bool fullscreen) {
  struct Try { int rgb, depth; };
  const Try tries[] = {{8, 24}, {8, 16}, {5, 16}};
  for (const Try& t : tries) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, t.rgb); SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, t.rgb == 5 ? 6 : 8); SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, t.rgb);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, t.depth);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE);
    gWin = SDL_CreateWindow("Omni Bonk", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
    if (!gWin) { LOGE("SDL_CreateWindow (rgb%d depth%d) failed: %s", t.rgb, t.depth, SDL_GetError()); continue; }
    gCtx = SDL_GL_CreateContext(gWin);
    if (gCtx) { LOGI("window: rgb%d depth%d %s", t.rgb, t.depth, fullscreen ? "fullscreen" : "windowed"); return true; }
    LOGE("SDL_GL_CreateContext (GLES 3.0, rgb%d depth%d) failed: %s", t.rgb, t.depth, SDL_GetError());
    SDL_DestroyWindow(gWin); gWin = nullptr;
  }
  return false;
}

// ---------------------------------------------------------------- controllers
static void openPad(int index) {
  char guid[40] = "";
  SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
  const char* nm = SDL_JoystickNameForIndex(index);
  if (SDL_IsGameController(index)) {
    SDL_GameController* c = SDL_GameControllerOpen(index);
    if (!c) { LOGE("controller %d open failed: %s", index, SDL_GetError()); return; }
    for (SDL_GameController* o : gPads) if (o == c) return;
    gPads.push_back(c);
    char* map = SDL_GameControllerMapping(c);
    LOGI("controller %d: \"%s\" guid %s", index, nm ? nm : "?", guid);
    LOGI("  mapping: %s", map ? map : "(none)");
    if (map) SDL_free(map);
  } else {
    SDL_Joystick* j = SDL_JoystickOpen(index);
    if (!j) return;
    for (SDL_Joystick* o : gRawJoys) if (o == j) return;
    gRawJoys.push_back(j);
    LOGI("joystick %d: \"%s\" guid %s has NO SDL mapping (axes %d, buttons %d, hats %d): using the generic layout", index, nm ? nm : "?", guid,
         SDL_JoystickNumAxes(j), SDL_JoystickNumButtons(j), SDL_JoystickNumHats(j));
  }
}
static void closePad(SDL_JoystickID id) {
  for (size_t i = 0; i < gPads.size(); i++)
    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gPads[i])) == id) { SDL_GameControllerClose(gPads[i]); gPads.erase(gPads.begin() + i); LOGI("controller removed"); return; }
  for (size_t i = 0; i < gRawJoys.size(); i++)
    if (SDL_JoystickInstanceID(gRawJoys[i]) == id) { SDL_JoystickClose(gRawJoys[i]); gRawJoys.erase(gRawJoys.begin() + i); return; }
}
static void maxAxis(float& dst, float v) { if (fabsf(v) > fabsf(dst)) dst = v; }

static PadState readPad(bool keyboard) {
  PadState s;
  for (SDL_GameController* c : gPads) {
    s.connected = true;
    auto ax = [&](SDL_GameControllerAxis a) { return SDL_GameControllerGetAxis(c, a) / 32767.f; };
    auto bt = [&](SDL_GameControllerButton b, unsigned bit) { if (SDL_GameControllerGetButton(c, b)) s.buttons |= bit; };
    maxAxis(s.lx, ax(SDL_CONTROLLER_AXIS_LEFTX)); maxAxis(s.ly, ax(SDL_CONTROLLER_AXIS_LEFTY));
    maxAxis(s.rx, ax(SDL_CONTROLLER_AXIS_RIGHTX)); maxAxis(s.ry, ax(SDL_CONTROLLER_AXIS_RIGHTY));
    bt(SDL_CONTROLLER_BUTTON_A, PB_A); bt(SDL_CONTROLLER_BUTTON_B, PB_B); bt(SDL_CONTROLLER_BUTTON_X, PB_X); bt(SDL_CONTROLLER_BUTTON_Y, PB_Y);
    bt(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PB_L1); bt(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PB_R1);
    bt(SDL_CONTROLLER_BUTTON_BACK, PB_SELECT); bt(SDL_CONTROLLER_BUTTON_START, PB_START);
    bt(SDL_CONTROLLER_BUTTON_DPAD_UP, PB_UP); bt(SDL_CONTROLLER_BUTTON_DPAD_DOWN, PB_DOWN); bt(SDL_CONTROLLER_BUTTON_DPAD_LEFT, PB_LEFT); bt(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PB_RIGHT);
    bt(SDL_CONTROLLER_BUTTON_LEFTSTICK, PB_L3); bt(SDL_CONTROLLER_BUTTON_RIGHTSTICK, PB_R3);
    if (ax(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0.4f) s.buttons |= PB_L2;
    if (ax(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.4f) s.buttons |= PB_R2;
  }
  // unmapped joystick: the usual Linux gamepad layout (b0 south, b1 east, b2 north, b3 west, L1 R1 L2 R2, select, start)
  for (SDL_Joystick* j : gRawJoys) {
    s.connected = true;
    int na = SDL_JoystickNumAxes(j), nb = SDL_JoystickNumButtons(j);
    auto ax = [&](int a) { return a < na ? SDL_JoystickGetAxis(j, a) / 32767.f : 0.f; };
    auto bt = [&](int b, unsigned bit) { if (b < nb && SDL_JoystickGetButton(j, b)) s.buttons |= bit; };
    maxAxis(s.lx, ax(0)); maxAxis(s.ly, ax(1)); maxAxis(s.rx, ax(2)); maxAxis(s.ry, ax(3));
    bt(0, PB_B); bt(1, PB_A); bt(2, PB_X); bt(3, PB_Y); bt(4, PB_L1); bt(5, PB_R1); bt(6, PB_L2); bt(7, PB_R2); bt(8, PB_SELECT); bt(9, PB_START);
    if (SDL_JoystickNumHats(j) > 0) {
      Uint8 h = SDL_JoystickGetHat(j, 0);
      if (h & SDL_HAT_UP) s.buttons |= PB_UP;
      if (h & SDL_HAT_DOWN) s.buttons |= PB_DOWN;
      if (h & SDL_HAT_LEFT) s.buttons |= PB_LEFT;
      if (h & SDL_HAT_RIGHT) s.buttons |= PB_RIGHT;
    }
  }
  if (keyboard) { // desktop testing: WASD = left stick, IJKL = right stick, arrows = D-pad
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    auto key = [&](SDL_Scancode sc, unsigned bit) { if (k[sc]) { s.buttons |= bit; s.connected = true; } };
    if (k[SDL_SCANCODE_A]) s.lx = -1;
    if (k[SDL_SCANCODE_D]) s.lx = 1;
    if (k[SDL_SCANCODE_W]) s.ly = -1;
    if (k[SDL_SCANCODE_S]) s.ly = 1;
    if (k[SDL_SCANCODE_J]) s.rx = -1;
    if (k[SDL_SCANCODE_L]) s.rx = 1;
    if (k[SDL_SCANCODE_I]) s.ry = -1;
    if (k[SDL_SCANCODE_K]) s.ry = 1;
    key(SDL_SCANCODE_UP, PB_UP); key(SDL_SCANCODE_DOWN, PB_DOWN); key(SDL_SCANCODE_LEFT, PB_LEFT); key(SDL_SCANCODE_RIGHT, PB_RIGHT);
    key(SDL_SCANCODE_SPACE, PB_A); key(SDL_SCANCODE_RETURN, PB_A); key(SDL_SCANCODE_LSHIFT, PB_B); key(SDL_SCANCODE_BACKSPACE, PB_B);
    key(SDL_SCANCODE_E, PB_X); key(SDL_SCANCODE_M, PB_Y); key(SDL_SCANCODE_Q, PB_L1); key(SDL_SCANCODE_R, PB_R1);
    key(SDL_SCANCODE_ESCAPE, PB_START); key(SDL_SCANCODE_TAB, PB_SELECT);
  }
  return s;
}

// ---------------------------------------------------------------- audio
static void audioCb(void*, Uint8* stream, int len) { app_audio((float*)stream, len / (int)(2 * sizeof(float)), gAudioRate); }
static void startAudio() {
  SDL_AudioSpec want; SDL_zero(want);
  want.freq = 48000; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = audioCb;
  SDL_AudioSpec have; SDL_zero(have);
  gAudio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0); // SDL converts if the device wants another format
  if (!gAudio) { LOGE("audio: no device (%s), running silent", SDL_GetError()); return; }
  gAudioRate = have.freq;
  LOGI("audio: %s, %d Hz, %d ch, buffer %d", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "?", have.freq, have.channels, have.samples);
  SDL_PauseAudioDevice(gAudio, 0);
}

// ---------------------------------------------------------------- screenshots (24-bit BMP, opens anywhere)
static bool saveBmp(const std::string& path, int w, int h) {
  std::vector<unsigned char> px((size_t)w * h * 4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
  int row = (w * 3 + 3) & ~3, size = 54 + row * h;
  unsigned char hd[54] = {'B', 'M'};
  auto put = [&](int o, int v) { hd[o] = (unsigned char)v; hd[o + 1] = (unsigned char)(v >> 8); hd[o + 2] = (unsigned char)(v >> 16); hd[o + 3] = (unsigned char)(v >> 24); };
  put(2, size); put(10, 54); put(14, 40); put(18, w); put(22, h); hd[26] = 1; hd[28] = 24; put(34, row * h);
  fwrite(hd, 1, 54, f);
  std::vector<unsigned char> line((size_t)row, 0);
  for (int y = 0; y < h; y++) { // GL rows are bottom-up, like BMP
    const unsigned char* s = &px[(size_t)y * w * 4];
    for (int x = 0; x < w; x++) { line[x * 3] = s[x * 4 + 2]; line[x * 3 + 1] = s[x * 4 + 1]; line[x * 3 + 2] = s[x * 4]; }
    fwrite(line.data(), 1, (size_t)row, f);
  }
  fclose(f);
  return true;
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IOLBF, 0);
  PlatformInfo pi; pi.density = 1; pi.dataDir = "."; pi.musicDir = "music";
  int winW = 0, winH = 0, maxFrames = -1; float fixedDt = 0; std::string shotPath; bool noAudio = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--handheld") pi.handheld = true;
    else if (a == "--device") pi.deviceName = next();
    else if (a == "--data") pi.dataDir = next();
    else if (a == "--music") pi.musicDir = next();             // optional OGG pack: menu/forest/desert/graveyard/boss/final.ogg
    else if (a == "--window") { std::string s = next(); sscanf(s.c_str(), "%dx%d", &winW, &winH); }
    else if (a == "--demo") pi.args += " demo=" + next();        // bot-played run: "char,map"
    else if (a == "--lang") pi.args += " lang=" + next();         // en, pt-BR, es, fr, ru, de
    else if (a == "--frames") maxFrames = atoi(next().c_str()); // quit after N frames (CI smoke test)
    else if (a == "--dt") fixedDt = (float)atof(next().c_str());
    else if (a == "--shot") shotPath = next();                  // BMP written when --frames is reached
    else if (a == "--no-audio") noAudio = true;
    else if (a == "--help") { printf("omnibonk [--handheld] [--device NAME] [--data DIR] [--music DIR] [--window WxH] [--lang CODE] [--demo C,M] [--frames N] [--dt S] [--shot F.bmp] [--no-audio]\n"); return 0; }
  }
  if (const char* e = getenv("OMNIBONK_HANDHELD")) if (*e && *e != '0') pi.handheld = true;
  gHandheld = pi.handheld;
  if (pi.handheld && pi.deviceName.empty()) pi.deviceName = "HANDHELD";
  signal(SIGINT, onSignal); signal(SIGTERM, onSignal);

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { LOGE("SDL_Init(VIDEO) failed: %s", SDL_GetError()); return 1; }
  if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) != 0) LOGE("SDL gamecontroller init failed: %s", SDL_GetError());
  SDL_version v; SDL_GetVersion(&v);
  LOGI("Omni Bonk (SDL build) | SDL %d.%d.%d | video driver %s | handheld %d", v.major, v.minor, v.patch, SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?", (int)pi.handheld);
  SDL_DisplayMode dm; SDL_zero(dm);
  if (SDL_GetCurrentDisplayMode(0, &dm) == 0) LOGI("display: %dx%d @ %d Hz", dm.w, dm.h, dm.refresh_rate);
  bool fullscreen = winW <= 0;
  if (fullscreen) { winW = dm.w > 0 ? dm.w : 640; winH = dm.h > 0 ? dm.h : 480; }
  if (!createWindow(winW, winH, fullscreen)) {
    LOGE("FATAL: could not create an OpenGL ES 3.0 context. This port needs GLES 3.0 (Mali-G31 / Panfrost on RK3326 support it).");
    SDL_Quit(); return 2;
  }
  if (!loadGL()) { LOGE("FATAL: the GL driver lacks GLES 3.0 entry points"); SDL_Quit(); return 2; }
  LOGI("GL vendor  : %s", (const char*)glGetString(GL_VENDOR));
  LOGI("GL renderer: %s", (const char*)glGetString(GL_RENDERER));
  LOGI("GL version : %s", (const char*)glGetString(GL_VERSION));
  LOGI("GLSL       : %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
  if (SDL_GL_SetSwapInterval(1) != 0) LOGE("vsync not available: %s", SDL_GetError());
  SDL_ShowCursor(pi.handheld ? SDL_DISABLE : SDL_ENABLE);
  int dw = winW, dh = winH; SDL_GL_GetDrawableSize(gWin, &dw, &dh);
  LOGI("drawable: %dx%d, swap interval %d", dw, dh, SDL_GL_GetSwapInterval());
  // something on screen while the models and textures are generated
  for (int i = 0; i < 2; i++) { glViewport(0, 0, dw, dh); glClearColor(0.06f, 0.04f, 0.10f, 1); glClear(GL_COLOR_BUFFER_BIT); SDL_GL_SwapWindow(gWin); }

  for (int i = 0; i < SDL_NumJoysticks(); i++) openPad(i);
  if (gPads.empty() && gRawJoys.empty()) LOGI("no controller found yet (keyboard: WASD/arrows, Space, Esc)");

  pi.width = dw; pi.height = dh;
  Uint64 freq = SDL_GetPerformanceFrequency(), t0 = SDL_GetPerformanceCounter();
  app_init(pi);
  app_gl_init();
  app_resize(dw, dh);
  LOGI("init: %.2f s, data dir %s", (double)(SDL_GetPerformanceCounter() - t0) / freq, pi.dataDir.c_str());
  if (!noAudio) { if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) startAudio(); else LOGE("SDL audio init failed: %s", SDL_GetError()); }

  Uint64 last = SDL_GetPerformanceCounter(), statT = last;
  double accFrame = 0, accSwap = 0, worst = 0; int statFrames = 0; long frameNo = 0; bool mouseDown = false; int shotNo = 0; unsigned prevButtons = 0;
  while (gRunning) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: gRunning = 0; break;
        case SDL_CONTROLLERDEVICEADDED: case SDL_JOYDEVICEADDED: openPad(e.type == SDL_CONTROLLERDEVICEADDED ? e.cdevice.which : e.jdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: closePad(e.cdevice.which); break;
        case SDL_JOYDEVICEREMOVED: closePad(e.jdevice.which); break;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) { SDL_GL_GetDrawableSize(gWin, &dw, &dh); app_resize(dw, dh); }
          else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && !gHandheld) app_pause();
          break;
        case SDL_MOUSEBUTTONDOWN: if (!gHandheld && e.button.button == SDL_BUTTON_LEFT) { mouseDown = true; app_event(InputEv{EV_DOWN, 0, (float)e.button.x, (float)e.button.y}); } break;
        case SDL_MOUSEBUTTONUP: if (!gHandheld && e.button.button == SDL_BUTTON_LEFT && mouseDown) { mouseDown = false; app_event(InputEv{EV_UP, 0, (float)e.button.x, (float)e.button.y}); } break;
        case SDL_MOUSEMOTION: if (!gHandheld && mouseDown) app_event(InputEv{EV_MOVE, 0, (float)e.motion.x, (float)e.motion.y}); break;
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
          if (!gHandheld) app_event(InputEv{e.type == SDL_FINGERDOWN ? EV_DOWN : e.type == SDL_FINGERUP ? EV_UP : EV_MOVE, (int)(e.tfinger.fingerId % 1000) + 1, e.tfinger.x * dw, e.tfinger.y * dh});
          break;
        default: break;
      }
    }
    PadState pad = readPad(!gHandheld || gPads.empty());
    app_gamepad(pad);
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = fixedDt > 0 ? fixedDt : (float)((double)(now - last) / freq);
    last = now;
    if (dt > 1.f) dt = 1.f;
    app_frame(dt);
    // L3 + R3: screenshot into the data dir (handy for bug reports from the handheld)
    if ((pad.buttons & PB_L3) && (pad.buttons & PB_R3) && !((prevButtons & PB_L3) && (prevButtons & PB_R3))) {
      char nm[64]; snprintf(nm, sizeof(nm), "/screenshot_%02d.bmp", ++shotNo);
      if (saveBmp(pi.dataDir + nm, dw, dh)) LOGI("screenshot saved: %s%s", pi.dataDir.c_str(), nm);
    }
    prevButtons = pad.buttons;
    frameNo++;
    if (maxFrames > 0 && frameNo >= maxFrames) {
      if (!shotPath.empty()) LOGI("%s %s", saveBmp(shotPath, dw, dh) ? "wrote" : "FAILED to write", shotPath.c_str());
      gRunning = 0;
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    SDL_GL_SwapWindow(gWin);
    Uint64 t2 = SDL_GetPerformanceCounter();
    double fms = (double)(t1 - now) * 1000.0 / freq, sms = (double)(t2 - t1) * 1000.0 / freq;
    accFrame += fms; accSwap += sms; statFrames++; if (fms + sms > worst) worst = fms + sms;
    // software frame limiter: for the 30 FPS option, and when the driver ignores vsync
    int cap = app_fps_cap();
    if (cap > 0 && fixedDt <= 0) {
      double target = 1000.0 / cap, spent = (double)(SDL_GetPerformanceCounter() - now) * 1000.0 / freq;
      if (spent < target * 0.9) SDL_Delay((Uint32)(target - spent));
    }
    double since = (double)(t2 - statT) / freq;
    if (since >= 10.0 || (maxFrames > 0 && frameNo == maxFrames)) {
      LOGI("perf: %.1f fps | cpu+draw %.1f ms | swap %.1f ms | worst %.0f ms | %s", statFrames / std::max(since, 1e-6), accFrame / std::max(1, statFrames), accSwap / std::max(1, statFrames), worst, app_status().c_str());
      statT = t2; accFrame = accSwap = worst = 0; statFrames = 0;
    }
  }
  LOGI("shutting down");
  app_save();
  if (gAudio) SDL_CloseAudioDevice(gAudio);
  app_gl_lost();
  for (SDL_GameController* c : gPads) SDL_GameControllerClose(c);
  for (SDL_Joystick* j : gRawJoys) SDL_JoystickClose(j);
  if (gCtx) SDL_GL_DeleteContext(gCtx);
  if (gWin) SDL_DestroyWindow(gWin);
  SDL_Quit();
  return 0;
}
