// Optional music pack: OGG Vorbis files in a folder outside the package (never in the repo, CI or releases).
// Slots: 0 menu, 1 forest, 2 desert, 3 graveyard, 4 boss, 5 final, 6 frostpeak, 7 caldera. A missing file keeps the chiptune for that slot.
#pragma once
#include <string>

enum { MUSIC_SLOTS = 8 };
int musicPackInit(const std::string& dir, int rate); // main thread, once: returns the number of tracks found
bool musicPackHas(int slot);
void musicPackStart(int slot);                        // audio thread: restart the stream on this slot
int musicPackRead(float* lr, int frames);             // audio thread: interleaved stereo, zero-filled past what is buffered
int musicPackUnderruns();
