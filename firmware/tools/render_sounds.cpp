// Renders every sound from src/synth.cpp to docs/sounds/<name>.wav, to listen on a computer.
//   c++ -std=c++17 -O2 -I src tools/render_sounds.cpp src/synth.cpp -o /tmp/render_sounds && /tmp/render_sounds
#include <stdio.h>
#include <string.h>

#include <vector>

#include "synth.h"

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "../docs/sounds";
  const Sound extra[] = {Sound::BOOP, Sound::WOBBLE, Sound::YAWN};
  const char *extra_names[] = {"boop", "wobble", "yawn"};
  auto write = [&](Sound s, const char *name) {
    std::vector<int16_t> pcm;
    int16_t chunk[256];
    for (int n = 0;; n += 256) {
      const bool more = synth::render(s, n, 256, chunk);
      pcm.insert(pcm.end(), chunk, chunk + 256);
      if (!more || n > synth::kRate * 5) break;
    }
    char path[256];
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    const uint32_t bytes = uint32_t(pcm.size() * 2);
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 1); put32(f, synth::kRate); put32(f, synth::kRate * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes); fwrite(pcm.data(), 2, pcm.size(), f);
    fclose(f);
    printf("wrote %s\n", path);
  };
  for (int i = 0; i < kSoundCount; i++)
    if (kSoundNames[i].sound != Sound::CUSTOM && kSoundNames[i].sound != Sound::NONE) write(kSoundNames[i].sound, kSoundNames[i].name);
  for (int i = 0; i < 3; i++) write(extra[i], extra_names[i]);
  return 0;
}
