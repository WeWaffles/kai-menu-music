// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { size_t offset, size; } Wave;
typedef bool (*WaveReader)(void *context,size_t offset,void *output,size_t length);
bool wave_probe(WaveReader read,void *context,size_t size,Wave *wave);
// Accept only bounded RIFF/WAVE PCM16 stereo 48 kHz. Unknown chunks are skipped.
bool wave_parse(const uint8_t *bytes, size_t size, Wave *wave);
// A power transition invalidates focus until a fresh, explicit HOME focus event.
bool focus_step(bool home, unsigned kind, unsigned applet, unsigned event);

// Match visible WAV filenames, excluding hidden metadata and path separators.
bool wave_filename(const char *name);
bool selection_parse(const char *text,size_t length,char *name,size_t capacity);
size_t wave_copy_block(void *output,size_t capacity,const void *data,size_t size,size_t *cursor,bool loop);
