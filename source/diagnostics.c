// SPDX-License-Identifier: MIT
#include "diagnostics.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_LIMIT (128u*1024u)
#define PENDING_SIZE 16384
#define EVENT_CACHE 32
static char pending[PENDING_SIZE];
static size_t pending_size;
static bool enabled,prepared;
static u64 beginning;
static struct { char name[48],detail[192]; Result result; bool used; } events[EVENT_CACHE];
static unsigned replacement;

static Result directory(FsFileSystem *sd,const char *path) {
    Result rc=fsFsCreateDirectory(sd,path);
    if (R_FAILED(rc)) {
        FsDirEntryType type;
        if (R_SUCCEEDED(fsFsGetEntryType(sd,path,&type)) && type==FsDirEntryType_Dir) return 0;
    }
    return rc;
}
static Result rotate(FsFileSystem *sd) {
    FsDirEntryType type;
    if (R_SUCCEEDED(fsFsGetEntryType(sd,MUSIC_LOG_PATH,&type))) {
        if (type!=FsDirEntryType_File) return MAKERESULT(Module_Libnx,LibnxError_BadInput);
        Result rc=fsFsDeleteFile(sd,MUSIC_LOG_PREVIOUS);
        if (R_FAILED(rc) && rc!=0x202) return rc;
        return fsFsRenameFile(sd,MUSIC_LOG_PATH,MUSIC_LOG_PREVIOUS);
    }
    return 0;
}
// Logging is best-effort and cannot change the playback result.
static void flush(void) {
    if (!enabled || !pending_size || R_FAILED(fsInitialize())) return;
    FsFileSystem sd;
    if (R_SUCCEEDED(fsOpenSdCardFileSystem(&sd))) {
        Result rc=directory(&sd,"/switch");
        if (R_SUCCEEDED(rc)) rc=directory(&sd,"/switch/kai-menu-music");
        if (R_SUCCEEDED(rc) && !prepared) {
            rc=rotate(&sd);
            if (R_SUCCEEDED(rc)) prepared=true;
        }
        if (R_SUCCEEDED(rc)) {
            rc=fsFsCreateFile(&sd,MUSIC_LOG_PATH,0,0);
            if (rc==0x402) rc=0;
        }
        FsFile file;
        if (R_SUCCEEDED(rc)) rc=fsFsOpenFile(&sd,MUSIC_LOG_PATH,FsOpenMode_Write,&file);
        if (R_SUCCEEDED(rc)) {
            s64 size=0;
            rc=fsFileGetSize(&file,&size);
            if (R_SUCCEEDED(rc) && (size<0 || (u64)size+pending_size>LOG_LIMIT)) {
                fsFileClose(&file);
                rc=rotate(&sd);
                if (R_SUCCEEDED(rc)) rc=fsFsCreateFile(&sd,MUSIC_LOG_PATH,0,0);
                if (R_SUCCEEDED(rc)) rc=fsFsOpenFile(&sd,MUSIC_LOG_PATH,FsOpenMode_Write,&file);
                if (R_SUCCEEDED(rc)) {
                    rc=fsFileSetSize(&file,(s64)pending_size);
                    if (R_SUCCEEDED(rc)) rc=fsFileWrite(&file,0,pending,pending_size,FsWriteOption_Flush);
                    if (R_SUCCEEDED(rc)) pending_size=0;
                    else fsFileSetSize(&file,0);
                    fsFileClose(&file);
                }
            } else {
                if (R_SUCCEEDED(rc)) {
                    rc=fsFileSetSize(&file,size+(s64)pending_size);
                    if (R_SUCCEEDED(rc)) {
                        rc=fsFileWrite(&file,size,pending,pending_size,FsWriteOption_Flush);
                        if (R_FAILED(rc)) fsFileSetSize(&file,size);
                    }
                }
                if (R_SUCCEEDED(rc)) pending_size=0;
                fsFileClose(&file);
            }
        }
        fsFsClose(&sd);
    }
    fsExit();
}
void music_log_start(void) { enabled=true; flush(); }
void music_log(const char *event,Result result,const char *format,...) {
    if (!event || !format) return;
    if (!beginning) beginning=svcGetSystemTick();
    char detail[192]; va_list arguments;
    va_start(arguments,format); vsnprintf(detail,sizeof detail,format,arguments); va_end(arguments);
    for (size_t i=0;detail[i];i++) if ((unsigned char)detail[i]<32 || (unsigned char)detail[i]==127) detail[i]=' ';
    unsigned slot=EVENT_CACHE;
    for (unsigned i=0;i<EVENT_CACHE;i++) if (events[i].used && !strcmp(events[i].name,event)) { slot=i; break; }
    if (slot<EVENT_CACHE && events[slot].result==result && !strcmp(events[slot].detail,detail)) return;
    if (slot==EVENT_CACHE) slot=replacement++%EVENT_CACHE;
    snprintf(events[slot].name,sizeof events[slot].name,"%s",event);
    snprintf(events[slot].detail,sizeof events[slot].detail,"%s",detail);
    events[slot].result=result; events[slot].used=true;
    char line[320];
    u64 elapsed=armTicksToNs(svcGetSystemTick()-beginning)/1000000;
    int length=snprintf(line,sizeof line,"[%llu ms] %s result=0x%08x %s\n",(unsigned long long)elapsed,event,result,detail);
    if (length<0) return;
    size_t size=(size_t)length<sizeof line ? (size_t)length : sizeof line-1;
    if (pending_size+size>PENDING_SIZE) pending_size=0;
    memcpy(pending+pending_size,line,size); pending_size+=size; flush();
}
