#pragma once
#include <stdint.h>
// Version 2: Reload takes an optional u32 menu index (see music_menus); none means HOME.
#define MUSIC_CONTROL_VERSION 2
enum { MusicCommand_State=0,MusicCommand_Reload=1 };
typedef struct { uint32_t version,requested,completed,result; } MusicState;
