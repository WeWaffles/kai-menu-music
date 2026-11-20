#pragma once
#include <switch.h>
#include "core.h"
#define MUSIC_DIRECTORY "/switch/kai-menu-music"
#define MUSIC_SELECTION MUSIC_DIRECTORY "/selected.txt"
#define MUSIC_SHUFFLE MUSIC_DIRECTORY "/shuffle.txt"
#define MAX_TRACK_BYTES (96u*1024u*1024u)
Result song_read_selection(FsFileSystem *sd,char name[FS_MAX_PATH]);
Result song_save_selection(FsFileSystem *sd,const char *name);
Result song_resolve(FsFileSystem *sd,const char *explicit_name,char path[FS_MAX_PATH],char name[FS_MAX_PATH]);
Result song_validate(FsFileSystem *sd,const char *name,Wave *wave);
Result song_read_shuffle(FsFileSystem *sd,bool *enabled);
Result song_save_shuffle(FsFileSystem *sd,bool enabled);
typedef uint32_t (*SongRandom)(void *context);
Result song_choose_shuffle(FsFileSystem *sd,const char *previous,SongRandom random,void *context,char name[FS_MAX_PATH]);
