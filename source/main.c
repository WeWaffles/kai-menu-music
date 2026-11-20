// SPDX-License-Identifier: MIT
#include <switch.h>
#include <kaihen/plugin.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"

u32 __nx_applet_type = AppletType_None;
size_t __nx_heap_size = 0x6800000; // bounded track + runtime + audio buffers
void __appInit(void) {}
void __appExit(void) {}
#define MAX_TRACK (96u*1024u*1024u)
#define BLOCK 4096u // 1024 stereo frames, about 21 ms
static unsigned char pcm[2][BLOCK] __attribute__((aligned(4096)));
static AudioOutBuffer buffers[2];

static Result initialize_version(void) {
    Result rc=setsysInitialize();
    if (R_FAILED(rc)) return rc;
    SetSysFirmwareVersion fw;
    rc=setsysGetFirmwareVersion(&fw);
    if (R_SUCCEEDED(rc)) hosversionSet(MAKEHOSVERSION(fw.major,fw.minor,fw.micro));
    setsysExit();
    return rc;
}
static Result bad_input(void) { return MAKERESULT(Module_Libnx, LibnxError_BadInput); }
// Pick one visible WAV deterministically without mounting a writable devoptab.
static Result find_track(FsFileSystem *sd,char path[FS_MAX_PATH]) {
    static const char directory[]="/switch/kai-menu-music";
    FsDir dir;
    Result rc=fsFsOpenDirectory(sd,directory,FsDirOpenMode_ReadFiles,&dir);
    if (R_FAILED(rc)) return rc;
    char selected[FS_MAX_PATH]={0};
    for (;;) {
        FsDirectoryEntry entry;
        s64 count=0;
        rc=fsDirRead(&dir,&count,1,&entry);
        if (R_FAILED(rc) || count==0) break;
        if (count!=1 || !memchr(entry.name,0,sizeof entry.name)) { rc=bad_input(); break; }
        if (!wave_filename(entry.name)) continue;
        if (strlen(directory)+1+strlen(entry.name)>=FS_MAX_PATH) continue;
        if (!selected[0] || strcmp(entry.name,selected)<0) strcpy(selected,entry.name);
    }
    fsDirClose(&dir);
    if (R_SUCCEEDED(rc) && !selected[0]) return MAKERESULT(Module_Libnx,LibnxError_NotFound);
    if (R_SUCCEEDED(rc)) snprintf(path,FS_MAX_PATH,"%s/%s",directory,selected);
    return rc;
}
// No devoptab mount, no stdio, no write-capable file handle.
static Result load_track(unsigned char **bytes, Wave *wave) {
    FsFileSystem sd;
    FsFile file;
    Result rc=fsOpenSdCardFileSystem(&sd);
    if (R_FAILED(rc)) return rc;
    char path[FS_MAX_PATH];
    rc=find_track(&sd,path);
    if (R_SUCCEEDED(rc)) rc=fsFsOpenFile(&sd,path,FsOpenMode_Read,&file);
    if (R_SUCCEEDED(rc)) {
        s64 size=0;
        rc=fsFileGetSize(&file,&size);
        if (R_SUCCEEDED(rc) && (size<12 || size>MAX_TRACK)) rc=bad_input();
        if (R_SUCCEEDED(rc)) {
            *bytes=malloc((size_t)size);
            if (!*bytes) rc=MAKERESULT(Module_Libnx,LibnxError_OutOfMemory);
        }
        if (R_SUCCEEDED(rc)) {
            u64 got=0;
            rc=fsFileRead(&file,0,*bytes,(u64)size,FsReadOption_None,&got);
            if (R_SUCCEEDED(rc) && (got!=(u64)size || !wave_parse(*bytes,(size_t)size,wave))) rc=bad_input();
        }
        fsFileClose(&file);
    }
    fsFsClose(&sd);
    return rc;
}
// Read the recent raw event window; any error or unrecognized state is silent.
// No writes or notifications are sent to PDM.
static bool home_focused(void) {
    s32 total=0, first=0, last=0, count=0;
    if (R_FAILED(pdmqryGetAvailablePlayEventRange(&total,&first,&last)) || total<=0) return false;
    s32 start=last>63 ? last-63 : 0;
    if (start<first) start=first;
    PdmPlayEvent events[64];
    if (R_FAILED(pdmqryQueryPlayEvent(start,events,64,&count)) || count<=0 || count>64) return false;
    bool home=false;
    for (s32 i=0;i<count;i++) {
        PdmPlayEvent *e=&events[i];
        home=focus_step(home,e->play_event_type,e->event_data.applet.applet_id,e->event_data.applet.event_type);
    }
    return home;
}
static void fill(unsigned i, const unsigned char *data, size_t size, size_t *cursor) {
    size_t done=0;
    while (done<BLOCK) {
        size_t n=size-*cursor;
        if (n>BLOCK-done) n=BLOCK-done;
        memcpy(pcm[i]+done,data+*cursor,n);
        // Half volume, entirely in the owned PCM buffer.
        done+=n; *cursor=(*cursor+n)%size;
    }
    for (size_t p=0;p<BLOCK;p+=2) {
        int16_t sample;
        memcpy(&sample,pcm[i]+p,2); sample/=2; memcpy(pcm[i]+p,&sample,2);
    }
    armDCacheFlush(pcm[i],BLOCK);
}
int main(void) {
    Handle ready=INVALID_HANDLE;
    Result rc=pluginRuntimeInitialize(&ready);
    if (R_FAILED(rc)) return rc;
    unsigned char *track=NULL;
    Wave wave={0};
    bool fs=false, pdm=false, audio=false, started=false;
    rc=svcSignalEvent(ready); // acknowledge runtime promptly; loading can take time
    if (R_SUCCEEDED(rc)) rc=initialize_version();
    if (R_SUCCEEDED(rc)) { rc=fsInitialize(); fs=R_SUCCEEDED(rc); }
    if (R_SUCCEEDED(rc)) rc=load_track(&track,&wave);
    if (fs) { fsExit(); fs=false; } // track stays in RAM for the rest of the boot
    if (R_SUCCEEDED(rc)) { rc=pdmqryInitialize(); pdm=R_SUCCEEDED(rc); }
    if (R_SUCCEEDED(rc)) { rc=audoutInitialize(); audio=R_SUCCEEDED(rc); }
    if (R_SUCCEEDED(rc) && (audoutGetSampleRate()!=48000 || audoutGetChannelCount()!=2 ||
        audoutGetPcmFormat()!=PcmFormat_Int16)) rc=bad_input();
    size_t cursor=0;
    bool queued[2]={false,false};
    for (unsigned i=0;i<2;i++) buffers[i]=(AudioOutBuffer){.buffer=pcm[i],.buffer_size=BLOCK,.data_size=BLOCK};
    bool home=false;
    unsigned focus_ticks=0;
    while (R_SUCCEEDED(rc)) {
        if (!focus_ticks) { home=home_focused(); focus_ticks=10; }
        --focus_ticks;
        if (!home) {
            if (started) { rc=audoutStopAudioOut(); started=false; }
            // Stopped buffers remain owned by audout; never overwrite them.
            focus_ticks=0;
            svcSleepThread(50000000);
            continue;
        }
        AudioOutBuffer *released=NULL;
        u32 count=0;
        do {
            rc=audoutGetReleasedAudioOutBuffer(&released,&count);
            if (R_FAILED(rc) || !count) break;
            if (released==&buffers[0]) queued[0]=false;
            else if (released==&buffers[1]) queued[1]=false;
            else { rc=bad_input(); break; }
        } while (count);
        if (R_FAILED(rc)) break;
        for (unsigned i=0;i<2 && R_SUCCEEDED(rc);i++) {
            if (queued[i]) continue;
            fill(i,track+wave.offset,wave.size,&cursor);
            rc=audoutAppendAudioOutBuffer(&buffers[i]);
            if (R_SUCCEEDED(rc)) queued[i]=true;
        }
        if (R_SUCCEEDED(rc) && !started) { rc=audoutStartAudioOut(); started=R_SUCCEEDED(rc); }
        svcSleepThread(5000000);
    }
    if (audio) { if (started) audoutStopAudioOut(); audoutExit(); }
    if (pdm) pdmqryExit();
    free(track);
    pluginRuntimeExit();
    return rc;
}
