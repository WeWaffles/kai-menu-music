// SPDX-License-Identifier: MIT
#include "songs.h"
#include <stdio.h>
#include <string.h>

static Result bad_input(void) { return MAKERESULT(Module_Libnx,LibnxError_BadInput); }
static bool song_path(const char *name,char path[FS_MAX_PATH]) {
    if (!wave_filename(name) || strlen(MUSIC_DIRECTORY)+1+strlen(name)>=FS_MAX_PATH) return false;
    snprintf(path,FS_MAX_PATH,"%s/%s",MUSIC_DIRECTORY,name); return true;
}
Result song_read_selection(FsFileSystem *sd,char name[FS_MAX_PATH]) {
    FsFile file; Result rc=fsFsOpenFile(sd,MUSIC_SELECTION,FsOpenMode_Read,&file);
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
Result song_save_selection(FsFileSystem *sd,const char *name) {
    char path[FS_MAX_PATH],text[FS_MAX_PATH+1];
    if (!song_path(name,path)) return bad_input();
    int length=snprintf(text,sizeof text,"%s\n",name);
    return save_text(sd,MUSIC_SELECTION,text,(size_t)length);
}
Result song_save_shuffle(FsFileSystem *sd,bool enabled) {
    return save_text(sd,MUSIC_SHUFFLE,enabled ? "1\n" : "0\n",2);
}
Result song_read_shuffle(FsFileSystem *sd,bool *enabled) {
    *enabled=false;
    FsFile file; Result rc=fsFsOpenFile(sd,MUSIC_SHUFFLE,FsOpenMode_Read,&file);
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
Result song_resolve(FsFileSystem *sd,const char *explicit_name,char path[FS_MAX_PATH],char name[FS_MAX_PATH]) {
    if (explicit_name) {
        if (!song_path(explicit_name,path)) return bad_input();
        strcpy(name,explicit_name); return 0;
    }
    if (R_SUCCEEDED(song_read_selection(sd,name)) && song_path(name,path)) {
        FsDirEntryType type;
        if (R_SUCCEEDED(fsFsGetEntryType(sd,path,&type)) && type==FsDirEntryType_File) return 0;
    }
    FsDir dir; Result rc=fsFsOpenDirectory(sd,MUSIC_DIRECTORY,FsDirOpenMode_ReadFiles,&dir);
    if (R_FAILED(rc)) return rc;
    name[0]=0;
    for (;;) {
        FsDirectoryEntry entry; s64 count=0; char candidate[FS_MAX_PATH];
        rc=fsDirRead(&dir,&count,1,&entry);
        if (R_FAILED(rc) || count==0) break;
        if (count!=1 || !memchr(entry.name,0,sizeof entry.name)) { rc=bad_input(); break; }
        if (!song_path(entry.name,candidate)) continue;
        if (!name[0] || strcmp(entry.name,name)<0) strcpy(name,entry.name);
    }
    fsDirClose(&dir);
    if (R_SUCCEEDED(rc) && !name[0]) return MAKERESULT(Module_Libnx,LibnxError_NotFound);
    if (R_SUCCEEDED(rc)) song_path(name,path);
    return rc;
}
static bool file_read(void *context,size_t offset,void *output,size_t length) {
    u64 got=0;
    return R_SUCCEEDED(fsFileRead(context,(s64)offset,output,length,FsReadOption_None,&got)) && got==length;
}
Result song_validate(FsFileSystem *sd,const char *name,Wave *wave) {
    char path[FS_MAX_PATH]; if (!song_path(name,path)) return bad_input();
    FsFile file; Result rc=fsFsOpenFile(sd,path,FsOpenMode_Read,&file);
    if (R_FAILED(rc)) return rc;
    s64 size=0; rc=fsFileGetSize(&file,&size);
    if (R_SUCCEEDED(rc) && (size<12 || size>MAX_TRACK_BYTES || !wave_probe(file_read,&file,(size_t)size,wave))) rc=bad_input();
    fsFileClose(&file); return rc;
}
Result song_choose_shuffle(FsFileSystem *sd,const char *previous,SongRandom random,void *context,char name[FS_MAX_PATH]) {
    if (!random) return bad_input();
    FsDir dir; Result rc=fsFsOpenDirectory(sd,MUSIC_DIRECTORY,FsDirOpenMode_ReadFiles,&dir);
    if (R_FAILED(rc)) return rc;
    name[0]=0; size_t eligible=0;
    for (;;) {
        FsDirectoryEntry entry; s64 count=0; char path[FS_MAX_PATH]; Wave wave;
        rc=fsDirRead(&dir,&count,1,&entry);
        if (R_FAILED(rc) || !count) break;
        if (count!=1 || !memchr(entry.name,0,sizeof entry.name)) { rc=bad_input(); break; }
        if (!song_path(entry.name,path) || (previous && !strcmp(entry.name,previous))) continue;
        if (R_FAILED(song_validate(sd,entry.name,&wave))) continue;
        ++eligible;
        if ((uint64_t)random(context)%eligible==0) strcpy(name,entry.name);
    }
    fsDirClose(&dir);
    if (R_FAILED(rc)) return rc;
    if (!name[0] && previous && R_SUCCEEDED(song_validate(sd,previous,&(Wave){0}))) strcpy(name,previous);
    return name[0] ? 0 : MAKERESULT(Module_Libnx,LibnxError_NotFound);
}
