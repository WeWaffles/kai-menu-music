// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { size_t offset, size; } Wave;
// Accept only bounded RIFF/WAVE PCM16 stereo 48 kHz. Unknown chunks are skipped.
bool wave_parse(const uint8_t *bytes, size_t size, Wave *wave);
// A power transition invalidates focus until a fresh, explicit HOME focus event.
bool focus_step(bool home, unsigned kind, unsigned applet, unsigned event);

// Match visible WAV filenames, excluding hidden metadata and path separators.
bool wave_filename(const char *name);
