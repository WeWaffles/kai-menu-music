// SPDX-License-Identifier: MIT
#include "songs.h"
#include <stdio.h>
#include <string.h>

static Result bad_input(void) { return MAKERESULT(Module_Libnx,LibnxError_BadInput); }
static bool join_path(const char *dir,const char *name,char path[FS_MAX_PATH]) {
    size_t d=strlen(dir),n=strlen(name);
    if (d+1+n>=FS_MAX_PATH) return false;
    memcpy(path,dir,d); path[d]='/'; memcpy(path+d+1,name,n+1); return true;
}
static bool song_path(const char *dir,const char *name,char path[FS_MAX_PATH]) {
    return dir && wave_filename(name) && join_path(dir,name,path);
}
static void setting_path(const char *dir,const char *file,char path[FS_MAX_PATH]) {
    if (!join_path(dir,file,path)) path[0]=0;
}
const char *song_menu_directory(int menu) {
    static const char *directories[MENU_COUNT]={MUSIC_DIRECTORY,MUSIC_DIRECTORY "/settings",MUSIC_DIRECTORY "/gameshare",
        MUSIC_DIRECTORY "/profile",MUSIC_DIRECTORY "/controllers",MUSIC_DIRECTORY "/album"};
    return menu>=0 && menu<MENU_COUNT ? directories[menu] : NULL;
}
int song_menu_playlist(FsFileSystem *sd,int menu) {
    if (menu<=MENU_HOME || menu>=MENU_COUNT) return menu==MENU_HOME ? MENU_HOME : MENU_NONE;
    char path[FS_MAX_PATH],name[FS_MAX_PATH];
    return R_SUCCEEDED(song_resolve(sd,song_menu_directory(menu),NULL,path,name)) ? menu : MENU_HOME;
}
Result song_read_selection(FsFileSystem *sd,const char *dir,char name[FS_MAX_PATH]) {
    if (!dir) return bad_input();
    char setting[FS_MAX_PATH]; setting_path(dir,MUSIC_SELECTION_FILE,setting);
    FsFile file; Result rc=fsFsOpenFile(sd,setting,FsOpenMode_Read,&file);
    if (R_FAILED(rc)) return rc;
    s64 size=0; u64 got=0; char text[FS_MAX_PATH+2];
    rc=fsFileGetSize(&file,&size);
    if (R_SUCCEEDED(rc) && (size<1 || size>(s64)sizeof text)) rc=bad_input();
    if (R_SUCCEEDED(rc)) rc=fsFileRead(&file,0,text,(u64)size,FsReadOption_None,&got);
    if (R_SUCCEEDED(rc) && (got!=(u64)size || !selection_parse(text,(size_t)size,name,FS_MAX_PATH))) rc=bad_input();
    fsFileClose(&file); return rc;
}
static Result save_text(FsFileSystem *sd,const char *path,const char *text,size_t length) {
    Result rc=fsFsCreateFile(sd,path,(s64)length,0);
    if (rc==0x402) rc=0;
    FsFile file;
    if (R_SUCCEEDED(rc)) rc=fsFsOpenFile(sd,path,FsOpenMode_Write,&file);
    if (R_SUCCEEDED(rc)) {
        rc=fsFileSetSize(&file,length);
        if (R_SUCCEEDED(rc)) rc=fsFileWrite(&file,0,text,(u64)length,FsWriteOption_Flush);
        fsFileClose(&file);
    }
    return rc;
}
Result song_save_selection(FsFileSystem *sd,const char *dir,const char *name) {
    char path[FS_MAX_PATH],text[FS_MAX_PATH+1];
    if (!song_path(dir,name,path)) return bad_input();
    int length=snprintf(text,sizeof text,"%s\n",name);
    setting_path(dir,MUSIC_SELECTION_FILE,path);
    return save_text(sd,path,text,(size_t)length);
}
Result song_save_shuffle(FsFileSystem *sd,const char *dir,bool enabled) {
    if (!dir) return bad_input();
    char path[FS_MAX_PATH]; setting_path(dir,MUSIC_SHUFFLE_FILE,path);
    return save_text(sd,path,enabled ? "1\n" : "0\n",2);
}
Result song_read_shuffle(FsFileSystem *sd,const char *dir,bool *enabled) {
    *enabled=false;
    if (!dir) return bad_input();
    char setting[FS_MAX_PATH]; setting_path(dir,MUSIC_SHUFFLE_FILE,setting);
    FsFile file; Result rc=fsFsOpenFile(sd,setting,FsOpenMode_Read,&file);
    if (R_FAILED(rc)) return rc;
    s64 size=0; u64 got=0; char text[2];
    rc=fsFileGetSize(&file,&size);
    if (R_SUCCEEDED(rc) && size!=2) rc=bad_input();
    if (R_SUCCEEDED(rc)) rc=fsFileRead(&file,0,text,2,FsReadOption_None,&got);
    if (R_SUCCEEDED(rc)) {
        if (got!=2 || text[1]!='\n' || (text[0]!='0' && text[0]!='1')) rc=bad_input();
        else *enabled=text[0]=='1';
    }
    fsFileClose(&file); return rc;
}
Result song_resolve(FsFileSystem *sd,const char *dir,const char *explicit_name,char path[FS_MAX_PATH],char name[FS_MAX_PATH]) {
    if (!dir) return bad_input();
    if (explicit_name) {
        if (!song_path(dir,explicit_name,path)) return bad_input();
        strcpy(name,explicit_name); return 0;
    }
    if (R_SUCCEEDED(song_read_selection(sd,dir,name)) && song_path(dir,name,path)) {
        FsDirEntryType type;
        if (R_SUCCEEDED(fsFsGetEntryType(sd,path,&type)) && type==FsDirEntryType_File) return 0;
    }
    FsDir listing; Result rc=fsFsOpenDirectory(sd,dir,FsDirOpenMode_ReadFiles,&listing);
    if (R_FAILED(rc)) return rc;
    name[0]=0;
    for (;;) {
        FsDirectoryEntry entry; s64 count=0; char candidate[FS_MAX_PATH];
        rc=fsDirRead(&listing,&count,1,&entry);
        if (R_FAILED(rc) || count==0) break;
        if (count!=1 || !memchr(entry.name,0,sizeof entry.name)) { rc=bad_input(); break; }
        if (!song_path(dir,entry.name,candidate)) continue;
        if (!name[0] || strcmp(entry.name,name)<0) strcpy(name,entry.name);
    }
    fsDirClose(&listing);
    if (R_SUCCEEDED(rc) && !name[0]) return MAKERESULT(Module_Libnx,LibnxError_NotFound);
    if (R_SUCCEEDED(rc)) song_path(dir,name,path);
    return rc;
}
static bool file_read(void *context,size_t offset,void *output,size_t length) {
    u64 got=0;
    return R_SUCCEEDED(fsFileRead(context,(s64)offset,output,length,FsReadOption_None,&got)) && got==length;
}
Result song_validate(FsFileSystem *sd,const char *dir,const char *name,Wave *wave) {
    char path[FS_MAX_PATH]; if (!song_path(dir,name,path)) return bad_input();
    FsFile file; Result rc=fsFsOpenFile(sd,path,FsOpenMode_Read,&file);
    if (R_FAILED(rc)) return rc;
    s64 size=0; rc=fsFileGetSize(&file,&size);
    if (R_SUCCEEDED(rc) && (size<12 || size>MAX_TRACK_BYTES || !wave_probe(file_read,&file,(size_t)size,wave))) rc=bad_input();
    fsFileClose(&file); return rc;
}
Result song_choose_shuffle(FsFileSystem *sd,const char *dir,const char *previous,SongRandom random,void *context,char name[FS_MAX_PATH]) {
    if (!random || !dir) return bad_input();
    FsDir listing; Result rc=fsFsOpenDirectory(sd,dir,FsDirOpenMode_ReadFiles,&listing);
    if (R_FAILED(rc)) return rc;
    name[0]=0; size_t eligible=0;
    for (;;) {
        FsDirectoryEntry entry; s64 count=0; char path[FS_MAX_PATH]; Wave wave;
        rc=fsDirRead(&listing,&count,1,&entry);
        if (R_FAILED(rc) || !count) break;
        if (count!=1 || !memchr(entry.name,0,sizeof entry.name)) { rc=bad_input(); break; }
        if (!song_path(dir,entry.name,path) || (previous && !strcmp(entry.name,previous))) continue;
        if (R_FAILED(song_validate(sd,dir,entry.name,&wave))) continue;
        ++eligible;
        if ((uint64_t)random(context)%eligible==0) strcpy(name,entry.name);
    }
    fsDirClose(&listing);
    if (R_FAILED(rc)) return rc;
    if (!name[0] && previous && R_SUCCEEDED(song_validate(sd,dir,previous,&(Wave){0}))) strcpy(name,previous);
    return name[0] ? 0 : MAKERESULT(Module_Libnx,LibnxError_NotFound);
}
