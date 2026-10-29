#include "text.h"

#include <stdint.h>
#include <string.h>

void foldAscii(const char *in, char *out, size_t cap) {
  static const char *from[] = {"á", "ä", "à", "â", "č", "ď", "é", "ě", "è", "ê", "í", "ĺ", "ľ", "ň", "ó", "ô", "ö", "ŕ",
                               "ř", "š", "ť", "ú", "ů", "ü", "ý", "ž", "Á", "Ä", "Č", "Ď", "É", "Ě", "Í", "Ĺ", "Ľ", "Ň",
                               "Ó", "Ô", "Ö", "Ŕ", "Ř", "Š", "Ť", "Ú", "Ů", "Ü", "Ý", "Ž", "ß", "–", "—", "’", "“", "”", "…"};
  static const char *to[] = {"a", "a", "a", "a", "c", "d", "e", "e", "e", "e", "i", "l", "l", "n", "o", "o", "o", "r",
                             "r", "s", "t", "u", "u", "u", "y", "z", "A", "A", "C", "D", "E", "E", "I", "L", "L", "N",
                             "O", "O", "O", "R", "R", "S", "T", "U", "U", "U", "Y", "Z", "ss", "-", "-", "'", "\"", "\"", "..."};
  size_t o = 0;
  while (*in && o + 4 < cap) {
    if ((uint8_t)*in < 0x80) {
      out[o++] = *in++;
      continue;
    }
    bool hit = false;
    for (size_t k = 0; k < sizeof from / sizeof *from; k++) {
      const size_t len = strlen(from[k]);
      if (!strncmp(in, from[k], len)) {
        for (const char *t = to[k]; *t && o + 1 < cap;) out[o++] = *t++;
        in += len;
        hit = true;
        break;
      }
    }
    if (!hit) {  // unknown character: skip the whole UTF-8 sequence
      out[o++] = '?';
      for (in++; ((uint8_t)*in & 0xC0) == 0x80; in++) {
      }
    }
  }
  out[o] = 0;
}
