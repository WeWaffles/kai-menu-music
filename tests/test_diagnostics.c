#include "../source/diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static char current[131073],previous[131073];
static size_t current_size,previous_size;
static bool have_current,have_previous,available,fail_write;
static unsigned writes,opens,closes,initializations;
static u64 ticks=1000000;
u64 svcGetSystemTick(void){ticks+=1000000;return ticks;}
u64 armTicksToNs(u64 t){return t;}
Result fsInitialize(void){if(!available)return 0x1234;initializations++;return 0;}
void fsExit(void){assert(initializations);initializations--;}
Result fsOpenSdCardFileSystem(FsFileSystem *fs){fs->unused=1;return 0;}
void fsFsClose(FsFileSystem *fs){assert(fs->unused==1);}
Result fsFsCreateDirectory(FsFileSystem *fs,const char *path){(void)fs;assert(!strcmp(path,"/switch")||!strcmp(path,"/switch/kai-menu-music"));return 0;}
Result fsFsGetEntryType(FsFileSystem *fs,const char *path,FsDirEntryType *type){(void)fs;assert(!strcmp(path,MUSIC_LOG_PATH));if(!have_current)return 0x202;*type=FsDirEntryType_File;return 0;}
Result fsFsDeleteFile(FsFileSystem *fs,const char *path){(void)fs;assert(!strcmp(path,MUSIC_LOG_PREVIOUS));if(!have_previous)return 0x202;have_previous=false;previous_size=0;return 0;}
Result fsFsRenameFile(FsFileSystem *fs,const char *old,const char *new){(void)fs;assert(!strcmp(old,MUSIC_LOG_PATH)&&!strcmp(new,MUSIC_LOG_PREVIOUS)&&opens==closes);assert(have_current&&!have_previous);memcpy(previous,current,current_size);previous_size=current_size;previous[previous_size]=0;have_previous=true;have_current=false;current_size=0;current[0]=0;return 0;}
Result fsFsCreateFile(FsFileSystem *fs,const char *path,s64 size,unsigned option){(void)fs;assert(!strcmp(path,MUSIC_LOG_PATH)&&size==0&&!option);if(have_current)return 0x402;have_current=true;current_size=0;current[0]=0;return 0;}
Result fsFsOpenFile(FsFileSystem *fs,const char *path,unsigned mode,FsFile *file){(void)fs;assert(!strcmp(path,MUSIC_LOG_PATH)&&mode==FsOpenMode_Write&&have_current);file->kind=1;opens++;return 0;}
Result fsFileGetSize(FsFile *f,s64 *size){assert(f->kind==1);*size=current_size;return 0;}
Result fsFileSetSize(FsFile *f,s64 size){assert(f->kind==1&&size>=0&&(u64)size<=131072);if((size_t)size>current_size)memset(current+current_size,0,(size_t)size-current_size);current_size=(size_t)size;current[current_size]=0;return 0;}
Result fsFileWrite(FsFile *f,s64 offset,const void *data,u64 length,unsigned option){assert(f->kind==1&&offset>=0&&(u64)offset+length<=current_size&&option==FsWriteOption_Flush);if(fail_write)return 0x5678;memcpy(current+offset,data,length);current[current_size]=0;writes++;return 0;}
void fsFileClose(FsFile *f){assert(f->kind==1);closes++;}

int main(void){
 strcpy(current,"previous boot\n");current_size=strlen(current);have_current=true;
 music_log("startup",0,"before ready");assert(!writes&&!opens);
 music_log_start();assert(!writes&&!opens); // An unavailable SD never aborts playback.
 available=true;music_log("filesystem",0,"ready");
 assert(have_previous&&!strcmp(previous,"previous boot\n"));
 assert(strstr(current,"startup result=0x00000000 before ready"));
 assert(strstr(current,"filesystem result=0x00000000 ready"));
 unsigned before=writes;
 for(unsigned i=0;i<1000;i++)music_log("focus",0,"home=0");
 assert(writes==before+1);
 music_log("focus",0,"home=1");assert(writes==before+2);
 music_log("sanitize",0,"line\nline\r\x1b[31m");
 assert(strstr(current,"line line  [31m"));
 size_t size_before=current_size;fail_write=true;
 music_log("failure",0x999,"retry me");assert(current_size==size_before);
 fail_write=false;music_log("recovered",0,"retry flushed");
 assert(strstr(current,"failure result=0x00000999 retry me"));
 assert(strstr(current,"recovered result=0x00000000 retry flushed"));
 for(unsigned i=0;i<3000;i++)music_log("heartbeat",0,"counter=%u",i);
 assert(current_size<=131072&&previous_size<=131072&&have_previous);
 assert(strstr(current,"counter=2999"));
 assert(opens==closes&&!initializations);
 puts("SD logging, retry, suppression and bounded rotation checks passed");
}
