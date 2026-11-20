#pragma once
#include <switch.h>
#define MUSIC_LOG_PATH "/switch/kai-menu-music/debug.log"
#define MUSIC_LOG_PREVIOUS "/switch/kai-menu-music/debug.previous.log"
void music_log_start(void);
void music_log(const char *event,Result result,const char *format,...) __attribute__((format(printf,3,4)));
