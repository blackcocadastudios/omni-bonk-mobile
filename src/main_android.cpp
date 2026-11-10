// Android platform layer: NativeActivity + EGL (GLES 3.0) + AAudio + multi-touch
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/window.h>
#include <aaudio/AAudio.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <time.h>
#include <unistd.h>
#include <atomic>
#include "platform.h"
#include "audio.h"

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

struct Engine {
  android_app* app = nullptr;
  EGLDisplay display = EGL_NO_DISPLAY; EGLSurface surface = EGL_NO_SURFACE; EGLContext context = EGL_NO_CONTEXT; EGLConfig config = nullptr;
  int width = 0, height = 0;
  bool focused = false, resumed = false, glReady = false, appInit = false;
  AAudioStream* audio = nullptr;
  double last = 0, lastFrame = 0;
  int insetPoll = 0;
  bool jniAttached = false;
};
static Engine E;
static std::atomic<bool> gAudioRestart{false};

static double now() { timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

// ---------------------------------------------------------------- JNI helpers
static JNIEnv* jniEnv() {
  JNIEnv* env = nullptr;
  JavaVM* vm = E.app->activity->vm;
  if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    E.jniAttached = true;
  }
  return env;
}
void plat_vibrate(int ms) {
  if (!E.app) return;
  JNIEnv* env = jniEnv(); if (!env) return;
  jobject act = E.app->activity->clazz;
  jclass cls = env->GetObjectClass(act);
  jmethodID m = env->GetMethodID(cls, "vibrate", "(I)V");
  if (m) env->CallVoidMethod(act, m, (jint)ms);
  if (env->ExceptionCheck()) env->ExceptionClear();
  env->DeleteLocalRef(cls);
}
void plat_quit() { if (E.app) ANativeActivity_finish(E.app->activity); }
static void readInsets() {
  JNIEnv* env = jniEnv(); if (!env) return;
  jobject act = E.app->activity->clazz;
  jclass cls = env->GetObjectClass(act);
  auto get = [&](const char* n) { jfieldID f = env->GetStaticFieldID(cls, n, "I"); if (!f) { env->ExceptionClear(); return 0; } return (int)env->GetStaticIntField(cls, f); };
  int l = get("insetL"), r = get("insetR"), t = get("insetT"), b = get("insetB");
  if (env->ExceptionCheck()) env->ExceptionClear();
  env->DeleteLocalRef(cls);
  app_set_insets((float)l, (float)r, (float)t, (float)b);
}

static std::string readLaunchArgs() {
  JNIEnv* env = jniEnv(); if (!env) return "";
  jobject act = E.app->activity->clazz;
  jclass cls = env->GetObjectClass(act);
  std::string out;
  jfieldID f = env->GetStaticFieldID(cls, "launchArgs", "Ljava/lang/String;");
  if (f) {
    jstring js = (jstring)env->GetStaticObjectField(cls, f);
    if (js) {
      const char* c = env->GetStringUTFChars(js, nullptr);
      if (c) { out = c; env->ReleaseStringUTFChars(js, c); }
      env->DeleteLocalRef(js);
    }
  }
  if (env->ExceptionCheck()) env->ExceptionClear();
  env->DeleteLocalRef(cls);
  return out;
}

// ---------------------------------------------------------------- audio
static aaudio_data_callback_result_t audioCb(AAudioStream* s, void*, void* data, int32_t frames) {
  app_audio((float*)data, frames, AAudioStream_getSampleRate(s));
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
static void startAudio();
static void audioErr(AAudioStream*, void*, aaudio_result_t err) {
  // headphones unplugged / device switched: the stream is dead; reopen it from the main loop
  if (err == AAUDIO_ERROR_DISCONNECTED) gAudioRestart = true;
}
static void startAudio() {
  if (E.audio) return;
  audioFlush();   // drop commands queued while the stream was stopped (they would all play at once)
  AAudioStreamBuilder* b = nullptr;
  if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
  AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
  AAudioStreamBuilder_setChannelCount(b, 2);
  AAudioStreamBuilder_setSampleRate(b, 48000);
  AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setDataCallback(b, audioCb, nullptr);
  AAudioStreamBuilder_setErrorCallback(b, audioErr, nullptr);
  AAudioStream* s = nullptr;
  if (AAudioStreamBuilder_openStream(b, &s) == AAUDIO_OK && s) { E.audio = s; AAudioStream_requestStart(s); }
  AAudioStreamBuilder_delete(b);
}
static void stopAudio() {
  if (!E.audio) return;
  AAudioStream_requestStop(E.audio); AAudioStream_close(E.audio); E.audio = nullptr;
}

// ---------------------------------------------------------------- EGL
static bool initDisplay() {
  if (E.display == EGL_NO_DISPLAY) {
    E.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(E.display, nullptr, nullptr);
  }
  if (!E.config) {
    const EGLint attribs24[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    const EGLint attribs16[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE};
    EGLint n = 0;
    if (!eglChooseConfig(E.display, attribs24, &E.config, 1, &n) || n == 0) eglChooseConfig(E.display, attribs16, &E.config, 1, &n);
    if (n == 0) { LOGE("No GLES3 EGL config"); return false; }
  }
  if (E.context == EGL_NO_CONTEXT) {
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    E.context = eglCreateContext(E.display, E.config, EGL_NO_CONTEXT, ctxAttr);
    if (E.context == EGL_NO_CONTEXT) { LOGE("eglCreateContext failed"); return false; }
    E.glReady = false;
  }
  return true;
}
static bool initSurface() {
  if (!E.app->window) return false;
  if (!initDisplay()) return false;
  if (E.surface == EGL_NO_SURFACE) {
    E.surface = eglCreateWindowSurface(E.display, E.config, E.app->window, nullptr);
    if (E.surface == EGL_NO_SURFACE) { LOGE("eglCreateWindowSurface failed"); return false; }
  }
  if (!eglMakeCurrent(E.display, E.surface, E.surface, E.context)) {
    EGLint err = eglGetError();
    LOGE("eglMakeCurrent failed 0x%x", err);
    if (err == EGL_CONTEXT_LOST) {
      if (E.glReady) app_gl_lost();
      eglDestroyContext(E.display, E.context); E.context = EGL_NO_CONTEXT; E.glReady = false;
      initDisplay();
      if (!eglMakeCurrent(E.display, E.surface, E.surface, E.context)) return false;
    } else return false;
  }
  eglSwapInterval(E.display, 1);
  eglQuerySurface(E.display, E.surface, EGL_WIDTH, &E.width);
  eglQuerySurface(E.display, E.surface, EGL_HEIGHT, &E.height);
  if (!E.glReady) {
    LOGI("GL %s | %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    app_gl_init(); E.glReady = true;
  }
  app_resize(E.width, E.height);
  return true;
}
static void termSurface() {
  if (E.display != EGL_NO_DISPLAY) {
    eglMakeCurrent(E.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (E.surface != EGL_NO_SURFACE) eglDestroySurface(E.display, E.surface);
  }
  E.surface = EGL_NO_SURFACE;
}
static void termAll() {
  if (E.glReady) {
    if (E.surface != EGL_NO_SURFACE) eglMakeCurrent(E.display, E.surface, E.surface, E.context);
    app_gl_lost();
  }
  termSurface();
  if (E.display != EGL_NO_DISPLAY) {
    if (E.context != EGL_NO_CONTEXT) eglDestroyContext(E.display, E.context);
    eglTerminate(E.display);
  }
  E.display = EGL_NO_DISPLAY; E.context = EGL_NO_CONTEXT; E.config = nullptr; E.glReady = false;
}

// ---------------------------------------------------------------- input
static int32_t onInput(android_app*, AInputEvent* ev) {
  int type = AInputEvent_getType(ev);
  if (type == AINPUT_EVENT_TYPE_KEY) {
    int code = AKeyEvent_getKeyCode(ev);
    if (code == AKEYCODE_BACK) {
      if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) { InputEv e{EV_BACK, 0, 0, 0}; app_event(e); }
      return 1;
    }
    return 0;
  }
  if (type != AINPUT_EVENT_TYPE_MOTION) return 0;
  int32_t action = AMotionEvent_getAction(ev);
  int act = action & AMOTION_EVENT_ACTION_MASK;
  int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
  size_t n = AMotionEvent_getPointerCount(ev);
  auto send = [&](EvType t, size_t i) { InputEv e{t, (int)AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i)}; app_event(e); };
  switch (act) {
    case AMOTION_EVENT_ACTION_DOWN: case AMOTION_EVENT_ACTION_POINTER_DOWN: send(EV_DOWN, (size_t)idx); break;
    case AMOTION_EVENT_ACTION_UP: case AMOTION_EVENT_ACTION_POINTER_UP: send(EV_UP, (size_t)idx); break;
    case AMOTION_EVENT_ACTION_MOVE: for (size_t i = 0; i < n; i++) send(EV_MOVE, i); break;
    case AMOTION_EVENT_ACTION_CANCEL: for (size_t i = 0; i < n; i++) send(EV_CANCEL, i); break;
    default: break;
  }
  return 1;
}

// ---------------------------------------------------------------- lifecycle
static void onCmd(android_app* app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW: initSurface(); E.last = now(); break;
    case APP_CMD_TERM_WINDOW: termSurface(); break;
    case APP_CMD_WINDOW_RESIZED: case APP_CMD_CONFIG_CHANGED: case APP_CMD_CONTENT_RECT_CHANGED:
      if (E.surface != EGL_NO_SURFACE) { eglQuerySurface(E.display, E.surface, EGL_WIDTH, &E.width); eglQuerySurface(E.display, E.surface, EGL_HEIGHT, &E.height); app_resize(E.width, E.height); }
      E.insetPoll = 30;
      break;
    case APP_CMD_GAINED_FOCUS: E.focused = true; startAudio(); app_resume(); E.last = now(); E.insetPoll = 60; break;
    case APP_CMD_LOST_FOCUS: E.focused = false; app_pause(); stopAudio(); break;
    case APP_CMD_RESUME: E.resumed = true; break;
    case APP_CMD_PAUSE: E.resumed = false; app_pause(); app_save(); stopAudio(); break;
    case APP_CMD_SAVE_STATE: app_save(); break;
    case APP_CMD_LOW_MEMORY: break;
    case APP_CMD_DESTROY: app_save(); break;
    default: break;
  }
  (void)app;
}

void android_main(android_app* app) {
  E = Engine();
  E.app = app;
  app->onAppCmd = onCmd;
  app->onInputEvent = onInput;
  ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
  PlatformInfo pi;
  pi.android = true;
  pi.dataDir = app->activity->internalDataPath ? app->activity->internalDataPath : ".";
  int dpi = AConfiguration_getDensity(app->config);
  pi.density = (dpi > 0 && dpi < 2000) ? dpi / 160.f : 2.f;
  pi.width = 1280; pi.height = 720;
  pi.args = readLaunchArgs();
  if (!pi.args.empty()) LOGI("launch args: %s", pi.args.c_str());
  app_init(pi);
  E.last = now();
  while (true) {
    int events = 0; android_poll_source* src = nullptr;
    bool active = E.focused && E.surface != EGL_NO_SURFACE;
    int ident;
    while ((ident = ALooper_pollOnce(active ? 0 : 250, nullptr, &events, (void**)&src)) >= 0 || ident == ALOOPER_POLL_CALLBACK) {
      if (src) src->process(app, src);
      src = nullptr;
      if (app->destroyRequested) break;
      active = E.focused && E.surface != EGL_NO_SURFACE;
    }
    if (app->destroyRequested) break;
    if (gAudioRestart.exchange(false) && E.focused) { stopAudio(); startAudio(); }
    if (!(E.focused && E.surface != EGL_NO_SURFACE)) continue;
    double t = now();
    int cap = app_fps_cap();
    if (cap > 0 && cap < 120) {
      double minDt = 1.0 / cap - 0.002;
      double wait = minDt - (t - E.lastFrame);
      if (wait > 0.001) { usleep((useconds_t)(wait * 1e6)); t = now(); }
    }
    E.lastFrame = t;
    float dt = (float)(t - E.last); E.last = t;
    if (dt > 1.0f) dt = 1.0f; // app_frame clamps the simulation step; the raw value feeds the FPS counter
    if (E.insetPoll > 0 && (E.insetPoll-- % 15) == 0) readInsets();
    EGLint w = 0, h = 0;
    eglQuerySurface(E.display, E.surface, EGL_WIDTH, &w); eglQuerySurface(E.display, E.surface, EGL_HEIGHT, &h);
    if (w != E.width || h != E.height) { E.width = w; E.height = h; app_resize(w, h); }
    app_frame(dt);
    if (!eglSwapBuffers(E.display, E.surface)) {
      EGLint err = eglGetError();
      if (err == EGL_BAD_SURFACE || err == EGL_BAD_NATIVE_WINDOW) { termSurface(); initSurface(); }
      else if (err == EGL_CONTEXT_LOST || err == EGL_BAD_CONTEXT) { termAll(); initSurface(); }
    }
  }
  // activity destroyed: release audio + GL, detach from the JVM before the glue thread exits
  stopAudio();
  app_save();
  termAll();
  if (E.jniAttached) { E.app->activity->vm->DetachCurrentThread(); E.jniAttached = false; }
}
