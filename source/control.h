#pragma once
#include <stdint.h>
enum { MusicCommand_State=0,MusicCommand_Reload=1 };
typedef struct { uint32_t version,requested,completed,result; } MusicState;
