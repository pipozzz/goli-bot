#pragma once

#include <stddef.h>

// The built-in fonts are ASCII only: fold common accented letters (Slovak, Czech, German...).
void foldAscii(const char *in, char *out, size_t cap);
