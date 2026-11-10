// Procedural audio: synthesized SFX + chiptune music, lock-free command queue
#pragma once
enum Sfx {
  SFX_JUMP, SFX_SLIDE, SFX_HIT, SFX_KILL, SFX_XP, SFX_COIN, SFX_LEVELUP, SFX_CHEST, SFX_SHRINE, SFX_DENY, SFX_PORTAL,
  SFX_BOSS, SFX_SWARM, SFX_EXPLODE, SFX_POWERUP, SFX_HEAL, SFX_BREAK, SFX_HURT, SFX_DEATH, SFX_VICTORY, SFX_SHOOT,
  SFX_SLASH, SFX_ZAP, SFX_FIRE, SFX_CLICK, SFX_COUNT
};
void audioInit(int sampleRate);
void audioPlay(int sfx, float vol = 1.f, float pitch = 1.f);
void audioAttract(bool on);            // main thread: SFX queued while on come from the background demo run (quieter, rate-limited)
void audioFlush();                     // drop queued commands and silence voices (call when the output stream stops)
void audioMusic(int track);            // 0 menu, 1 forest, 2 desert, 3 graveyard, 4 boss, -1 stop
void audioSetVolume(float music, float sfx);
void audioSetIntensity(float x);       // 0..1 adds layers
void audioRender(float* stereo, int frames); // audio thread
void plat_vibrate_safe(int ms);
