#pragma once
#include <switch.h>
#include "core.h"
#define MUSIC_DIRECTORY "/switch/kai-menu-music"
#define MUSIC_SELECTION_FILE "selected.txt"
#define MUSIC_SHUFFLE_FILE "shuffle.txt"
#define MUSIC_SELECTION MUSIC_DIRECTORY "/" MUSIC_SELECTION_FILE
#define MUSIC_SHUFFLE MUSIC_DIRECTORY "/" MUSIC_SHUFFLE_FILE
#define MAX_TRACK_BYTES (96u*1024u*1024u)
// HOME uses MUSIC_DIRECTORY itself; other menus use MUSIC_DIRECTORY/<key>.
// Every function below takes one of these playlist folders as `dir`.
const char *song_menu_directory(int menu);
// Which folder a menu plays: its own folder when it holds a WAV, otherwise HOME's.
int song_menu_playlist(FsFileSystem *sd,int menu);
Result song_read_selection(FsFileSystem *sd,const char *dir,char name[FS_MAX_PATH]);
Result song_save_selection(FsFileSystem *sd,const char *dir,const char *name);
Result song_resolve(FsFileSystem *sd,const char *dir,const char *explicit_name,char path[FS_MAX_PATH],char name[FS_MAX_PATH]);
Result song_validate(FsFileSystem *sd,const char *dir,const char *name,Wave *wave);
Result song_read_shuffle(FsFileSystem *sd,const char *dir,bool *enabled);
Result song_save_shuffle(FsFileSystem *sd,const char *dir,bool enabled);
typedef uint32_t (*SongRandom)(void *context);
Result song_choose_shuffle(FsFileSystem *sd,const char *dir,const char *previous,SongRandom random,void *context,char name[FS_MAX_PATH]);
