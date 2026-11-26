// SPDX-License-Identifier: MIT
#include <switch.h>
#include <kaihen/plugin.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "songs.h"
#include "control.h"
#include "diagnostics.h"

u32 __nx_applet_type = AppletType_None;
size_t __nx_heap_size = 0x6800000; // bounded track + runtime + audio buffers
void __appInit(void) {}
void __appExit(void) {}
#define BLOCK 4096u // 1024 stereo frames, about 21 ms
static unsigned char pcm[2][BLOCK] __attribute__((aligned(4096)));
static AudioOutBuffer buffers[2];

static Result initialize_version(void) {
    Result rc=setsysInitialize();
    music_log("firmware.open",rc,"system settings service");
    if (R_FAILED(rc)) return rc;
    SetSysFirmwareVersion fw;
    rc=setsysGetFirmwareVersion(&fw);
    if (R_SUCCEEDED(rc)) {
        hosversionSet(MAKEHOSVERSION(fw.major,fw.minor,fw.micro));
        music_log("firmware.version",rc,"%u.%u.%u",fw.major,fw.minor,fw.micro);
    } else music_log("firmware.version",rc,"could not read firmware");
    setsysExit();
    return rc;
}
static Result bad_input(void) { return MAKERESULT(Module_Libnx, LibnxError_BadInput); }
typedef struct { Mutex lock; MusicState state; u32 menus,last_menu; } MusicControl;
static MusicControl control={.state={.version=MUSIC_CONTROL_VERSION}};

static Result control_command(u32 command,const void *input,size_t input_size,void *output,size_t *output_size,void *context) {
    (void)context;
    // A reload names the menu whose playlist changed; no input means HOME.
    u32 menu=MENU_HOME;
    if (command==MusicCommand_Reload && input && input_size>=sizeof menu) memcpy(&menu,input,sizeof menu);
    if (menu>=MENU_COUNT) return bad_input();
    mutexLock(&control.lock);
    if (command==MusicCommand_State) {
        memcpy(output,&control.state,sizeof control.state); *output_size=sizeof control.state;
    } else if (command==MusicCommand_Reload) {
        control.menus|=1u<<menu; control.last_menu=menu;
        if (++control.state.requested==0) ++control.state.requested;
        memcpy(output,&control.state.requested,sizeof control.state.requested); *output_size=sizeof control.state.requested;
    } else { mutexUnlock(&control.lock); return bad_input(); }
    mutexUnlock(&control.lock); return 0;
}
static void completed(u32 generation,Result result) {
    mutexLock(&control.lock); control.state.completed=generation; control.state.result=result; mutexUnlock(&control.lock);
}
static u32 requested(u32 *menus,u32 *last_menu) {
    mutexLock(&control.lock);
    u32 result=control.state.requested;
    if (menus) { *menus=control.menus; *last_menu=control.last_menu; control.menus=0; }
    mutexUnlock(&control.lock); return result;
}
// Music and settings are read-only here; diagnostics writes only its SD log.
static Result load_track(const char *dir,const char *requested_name,unsigned char **bytes,Wave *wave,char active_name[FS_MAX_PATH]) {
    *bytes=NULL; *wave=(Wave){0};
    FsFileSystem sd; FsFile file;
    Result rc=fsOpenSdCardFileSystem(&sd);
    music_log("track.sd_open",rc,"opening SD filesystem");
    if (R_FAILED(rc)) return rc;
    char path[FS_MAX_PATH];
    rc=song_resolve(&sd,dir,requested_name,path,active_name);
    music_log("track.resolve",rc,"folder=%s song=%s",dir,R_SUCCEEDED(rc) ? active_name : "not found");
    if (R_SUCCEEDED(rc)) {
        rc=fsFsOpenFile(&sd,path,FsOpenMode_Read,&file);
        music_log("track.open",rc,"path=%s",path);
    }
    if (R_SUCCEEDED(rc)) {
        s64 size=0;
        rc=fsFileGetSize(&file,&size);
        music_log("track.size",rc,"bytes=%lld limit=%u",(long long)size,MAX_TRACK_BYTES);
        if (R_SUCCEEDED(rc) && (size<12 || size>MAX_TRACK_BYTES)) { rc=bad_input(); music_log("track.size_rejected",rc,"outside supported size range"); }
        if (R_SUCCEEDED(rc)) {
            *bytes=malloc((size_t)size);
            if (!*bytes) rc=MAKERESULT(Module_Libnx,LibnxError_OutOfMemory);
            music_log("track.allocate",rc,"requested_bytes=%lld",(long long)size);
        }
        if (R_SUCCEEDED(rc)) {
            u64 got=0; rc=fsFileRead(&file,0,*bytes,(u64)size,FsReadOption_None,&got);
            music_log("track.read",rc,"read=%llu expected=%lld",(unsigned long long)got,(long long)size);
            if (R_SUCCEEDED(rc) && (got!=(u64)size || !wave_parse(*bytes,(size_t)size,wave))) rc=bad_input();
            music_log("wav.parse",rc,"PCM16 stereo 48000Hz required; data_offset=%zu data_bytes=%zu",wave->offset,wave->size);
            if (R_SUCCEEDED(rc)) {
                unsigned peak=0; size_t sample_bytes=wave->size<192000 ? wave->size : 192000;
                for (size_t i=0;i<sample_bytes;i+=2) {
                    int16_t sample; memcpy(&sample,*bytes+wave->offset+i,2);
                    unsigned amplitude=sample<0 ? (unsigned)(-(int)sample) : (unsigned)sample;
                    if (amplitude>peak) peak=amplitude;
                }
                music_log("track.loaded",0,"song=%s duration_ms=%llu first_second_peak=%u plugin_gain=0.5",active_name,(unsigned long long)wave->size*1000/192000,peak);
            }
        }
        fsFileClose(&file);
    }
    fsFsClose(&sd);
    if (R_FAILED(rc)) { free(*bytes); *bytes=NULL; *wave=(Wave){0}; }
    return rc;
}
static uint32_t shuffle_random(void *context) { (void)context; return (uint32_t)randomGet64(); }
static Result choose_playback(const char *dir,bool *shuffle,const char *previous,char name[FS_MAX_PATH]) {
    FsFileSystem sd; Result rc=fsOpenSdCardFileSystem(&sd);
    if (R_FAILED(rc)) return rc;
    Result mode=song_read_shuffle(&sd,dir,shuffle);
    music_log("selection.shuffle",mode,"folder=%s enabled=%u; missing setting defaults to off",dir,*shuffle);
    char path[FS_MAX_PATH]; Wave wave;
    if (*shuffle) rc=song_choose_shuffle(&sd,dir,previous,shuffle_random,NULL,name);
    else rc=song_resolve(&sd,dir,NULL,path,name);
    music_log("selection.resolve",rc,"shuffle=%u song=%s",*shuffle,R_SUCCEEDED(rc) ? name : "not found");
    if (R_SUCCEEDED(rc)) {
        rc=song_validate(&sd,dir,name,&wave);
        music_log("wav.validate",rc,"song=%s; requires PCM16 stereo 48000Hz and <=96MiB",name);
    }
    fsFsClose(&sd); return rc;
}
static Result audio_initialize(void) {
    Result rc=audoutInitialize();
    music_log("audio.open",rc,"audout service");
    if (R_SUCCEEDED(rc)) music_log("audio.format",0,"rate=%u channels=%u format=%u expected_format=%u",audoutGetSampleRate(),audoutGetChannelCount(),(unsigned)audoutGetPcmFormat(),(unsigned)PcmFormat_Int16);
    if (R_SUCCEEDED(rc)) {
        float volume=0; Result vr=audoutGetAudioOutVolume(&volume);
        music_log("audio.volume",vr,"output_stream_volume=%.3f",(double)volume);
    }
    if (R_SUCCEEDED(rc) && (audoutGetSampleRate()!=48000 || audoutGetChannelCount()!=2 || audoutGetPcmFormat()!=PcmFormat_Int16)) {
        audoutExit(); rc=bad_input(); music_log("audio.format_rejected",rc,"unsupported audio output format");
    }
    return rc;
}
// Read the recent raw event window; any error or unrecognized state is silent.
// No writes or notifications are sent to PDM. Returns the focused menu or MENU_NONE.
static int focused_menu(void) {
    s32 total=0, first=0, last=0, count=0;
    Result rc=pdmqryGetAvailablePlayEventRange(&total,&first,&last);
    music_log("pdm.range",rc,total>0 ? "play events available" : "no play events available");
    if (R_FAILED(rc) || total<=0) return MENU_NONE;
    s32 start=last>63 ? last-63 : 0;
    if (start<first) start=first;
    PdmPlayEvent events[64];
    rc=pdmqryQueryPlayEvent(start,events,64,&count);
    music_log("pdm.query",rc,count>0 && count<=64 ? "read events" : "empty or invalid event count");
    if (R_FAILED(rc) || count<=0 || count>64) return MENU_NONE;
    static s32 logged=-1;
    if (last<logged) logged=-1;
    for (s32 i=count>8 ? count-8 : 0;i<count;i++) {
        if (start+i<=logged) continue;
        PdmPlayEvent *event=&events[i];
        unsigned applet=event->play_event_type==0 ? event->event_data.applet.applet_id : 0;
        unsigned action=event->play_event_type==0 ? event->event_data.applet.event_type : 0;
        unsigned long long program=event->play_event_type==0 ? (unsigned long long)event->event_data.applet.program_id[1]<<32|event->event_data.applet.program_id[0] : 0;
        music_log("pdm.event",0,"index=%d kind=%u applet=%u event=%u program=%016llx",start+i,(unsigned)event->play_event_type,applet,action,program);
    }
    logged=last;
    MenuFocus focus={0};
    for (s32 i=0;i<count;i++) {
        PdmPlayEvent *e=&events[i];
        menu_focus_step(&focus,e->play_event_type,e->event_data.applet.applet_id,e->event_data.applet.event_type);
    }
    int menu=menu_focus_menu(&focus);
    static int reported=-2;
    if (menu!=reported) {
        music_log("focus.menu",0,"menu=%s applet=%u",menu>=0 ? music_menus[menu].key : "none",focus.current);
        reported=menu;
    }
    return menu;
}
// One track is in RAM at a time. Each playlist remembers where it stopped so
// returning to a menu resumes its song instead of restarting it.
typedef struct { char name[FS_MAX_PATH]; size_t cursor; } Resume;
enum { MENU_RECHECK=-2 }; // forces the next focus check to pick a playlist again
static Resume resume[MENU_COUNT];
static unsigned char *track; static Wave wave; static char active_name[FS_MAX_PATH];
static bool audio,started,shuffle,queued[2];
static size_t cursor; static int playing=MENU_NONE;

static void close_audio(const char *why) {
    if (audio) {
        Result stopped=started ? audoutStopAudioOut() : 0;
        music_log("audio.state",stopped,"closed for %s",why); audoutExit();
    }
    audio=started=false;
}
static void remember_position(void) {
    if (playing<0 || !track) return;
    strcpy(resume[playing].name,active_name); resume[playing].cursor=cursor;
}
// Loads a song from `playlist`. A fresh load picks per the folder's settings;
// otherwise the playlist's remembered song and position are tried first.
// `previous` is the song shuffle should avoid. Requires fsInitialize.
static Result open_playlist(int playlist,bool fresh,const char *previous) {
    const char *dir=song_menu_directory(playlist);
    bool next_shuffle=false; char chosen[FS_MAX_PATH]; size_t position=0;
    Result load=MAKERESULT(Module_Libnx,LibnxError_NotFound);
    Resume *saved=&resume[playlist];
    if (!fresh && saved->name[0]) {
        FsFileSystem sd; load=fsOpenSdCardFileSystem(&sd);
        if (R_SUCCEEDED(load)) {
            song_read_shuffle(&sd,dir,&next_shuffle);
            load=song_validate(&sd,dir,saved->name,&(Wave){0});
            fsFsClose(&sd);
        }
        music_log("track.resume",load,"folder=%s song=%s cursor=%zu",dir,saved->name,saved->cursor);
        if (R_SUCCEEDED(load)) { strcpy(chosen,saved->name); position=saved->cursor; }
    }
    if (R_FAILED(load)) load=choose_playback(dir,&next_shuffle,previous,chosen);
    if (R_FAILED(load)) return load;
    char previous_name[FS_MAX_PATH]; strcpy(previous_name,active_name);
    int previous_playlist=playing; size_t previous_cursor=cursor;
    if (previous_playlist!=playlist) remember_position();
    close_audio("track change");
    free(track); track=NULL; playing=MENU_NONE; active_name[0]=0;
    load=load_track(dir,chosen,&track,&wave,active_name);
    if (R_SUCCEEDED(load)) playing=playlist;
    else if (previous_playlist>=0 && previous_name[0]) {
        Result rollback=load_track(song_menu_directory(previous_playlist),previous_name,&track,&wave,active_name);
        music_log("track.rollback",rollback,"restoring previous song");
        if (R_SUCCEEDED(rollback)) { playing=previous_playlist; position=previous_cursor; }
    }
    if (track) {
        Result ar=audio_initialize(); audio=R_SUCCEEDED(ar);
        if (R_SUCCEEDED(load)) load=ar;
    }
    if (R_SUCCEEDED(load)) shuffle=next_shuffle;
    else if (playing!=previous_playlist) shuffle=false;
    cursor=position<wave.size && !(position%4) ? position : 0;
    queued[0]=queued[1]=false;
    return load;
}
// Validates a playlist without replacing the song in RAM (for selector requests
// about a menu that is not playing). Requires fsInitialize.
static Result check_playlist(int playlist) {
    bool next_shuffle=false; char chosen[FS_MAX_PATH];
    return choose_playback(song_menu_directory(playlist),&next_shuffle,NULL,chosen);
}
static int playlist_for(int menu) {
    if (menu<0) return MENU_NONE;
    FsFileSystem sd; if (R_FAILED(fsOpenSdCardFileSystem(&sd))) return MENU_HOME;
    int playlist=song_menu_playlist(&sd,menu); fsFsClose(&sd);
    music_log("menu.playlist",0,"menu=%s folder=%s",music_menus[menu].key,song_menu_directory(playlist));
    return playlist;
}
static size_t fill(unsigned i,const unsigned char *data,size_t size,size_t *position,bool loop) {
    size_t done=wave_copy_block(pcm[i],BLOCK,data,size,position,loop);
    for (size_t p=0;p<done;p+=2) {
        int16_t sample;
        memcpy(&sample,pcm[i]+p,2); sample/=2; memcpy(pcm[i]+p,&sample,2);
    }
    armDCacheFlush(pcm[i],BLOCK); return done;
}
int main(void) {
    Handle ready=INVALID_HANDLE,session=INVALID_HANDLE;
    PluginService service={0};
    Result rc=pluginRuntimeInitializeWithService(&ready,&session);
    music_log("plugin.start",rc,"Menu Music 1.2.0 diagnostic=1");
    if (R_FAILED(rc)) return rc;
    rc=pluginServiceStart(&service,session,control_command,NULL);
    music_log("control.service",rc,"song-selector IPC");
    if (R_SUCCEEDED(rc)) { rc=svcSignalEvent(ready); music_log("plugin.ready",rc,"loader ready signal"); }
    if (R_SUCCEEDED(rc)) rc=initialize_version();
    music_log_start();
    bool pdm=false,advance=false;
    if (R_SUCCEEDED(rc)) { rc=pdmqryInitialize(); pdm=R_SUCCEEDED(rc); music_log("pdm.open",rc,"play-event query service"); }
    if (R_SUCCEEDED(rc)) {
        // HOME's song is loaded up front so the first HOME visit starts instantly.
        Result load=fsInitialize();
        music_log("filesystem.open",load,"file service");
        if (R_SUCCEEDED(load)) { load=open_playlist(MENU_HOME,true,NULL); fsExit(); }
        music_log("startup.track",load,"track=%u audio=%u shuffle=%u",track!=NULL,audio,shuffle);
        completed(0,load); // Missing music stays idle; the selector can load it later.
    }
    // menu: focused menu; wanted: the playlist it plays; failed: a playlist that
    // would not load, left alone until focus moves or the selector asks again.
    int menu=MENU_NONE,wanted=MENU_NONE,failed=MENU_NONE;
    unsigned focus_ticks=0; u32 generation=0;
    u64 boot_tick=svcGetSystemTick(),last_heartbeat=boot_tick,queued_total=0,released_total=0;
    for (unsigned i=0;i<2;i++) buffers[i]=(AudioOutBuffer){.buffer=pcm[i],.buffer_size=BLOCK,.data_size=BLOCK};
    while (R_SUCCEEDED(rc)) {
        u64 now=svcGetSystemTick();
        if (armTicksToNs(now-last_heartbeat)>=30000000000ULL) {
            music_log("heartbeat",0,"seconds=%llu menu=%d playing=%d track=%u audio=%u started=%u shuffle=%u cursor=%zu queued=%llu released=%llu",(unsigned long long)(armTicksToNs(now-boot_tick)/1000000000ULL),menu,playing,track!=NULL,audio,started,shuffle,cursor,(unsigned long long)queued_total,(unsigned long long)released_total);
            if (audio) {
                AudioOutState state=0; Result state_result=audoutGetAudioOutState(&state);
                music_log("audio.driver_state",state_result,"state=%u",(unsigned)state);
            }
            last_heartbeat=now;
        }
        if (requested(NULL,NULL)!=generation) {
            u32 menus=0,last_menu=MENU_HOME,next=requested(&menus,&last_menu);
            music_log("track.reload",0,"request=%u menus=0x%x",next,menus);
            Result load=fsInitialize();
            if (R_SUCCEEDED(load)) {
                // Changed folders start over; the playing one reloads now.
                bool reload_playing=playing<0;
                int reported=playlist_for((int)last_menu);
                for (int i=0;i<MENU_COUNT;i++) {
                    if (!(menus&(1u<<i))) continue;
                    int playlist=playlist_for(i);
                    resume[playlist].name[0]=0;
                    if (playlist==playing) reload_playing=true;
                }
                if (reload_playing) {
                    int target=playing>=0 ? playing : reported;
                    Result opened=open_playlist(target,true,NULL);
                    load=reported==target ? opened : check_playlist(reported);
                } else load=check_playlist(reported);
                menu=MENU_RECHECK; wanted=failed=MENU_NONE; focus_ticks=0;
                fsExit();
            }
            music_log("track.reload_complete",load,"request=%u track=%u audio=%u playing=%d",next,track!=NULL,audio,playing);
            generation=next; completed(generation,load);
        }
        if (advance) {
            music_log("track.reload",0,"automatic shuffle advance");
            Result load=fsInitialize();
            if (R_SUCCEEDED(load)) {
                char previous[FS_MAX_PATH]; strcpy(previous,active_name);
                resume[playing].name[0]=0;
                load=open_playlist(playing,true,previous);
                fsExit();
            }
            music_log("track.reload_complete",load,"automatic track=%u audio=%u",track!=NULL,audio);
            if (R_FAILED(load)) { shuffle=false; cursor=0; }
            advance=false;
        }
        if (!focus_ticks) {
            int now_menu=focused_menu();
            if (now_menu!=menu) {
                menu=now_menu; failed=MENU_NONE;
                if (menu>=0 && R_SUCCEEDED(fsInitialize())) { wanted=playlist_for(menu); fsExit(); }
                else wanted=menu>=0 ? MENU_HOME : MENU_NONE;
            }
            focus_ticks=10;
        }
        --focus_ticks;
        if (wanted>=0 && wanted!=playing && wanted!=failed) {
            // Entering a menu with its own playlist (or returning to one).
            if (started) { audoutStopAudioOut(); started=false; }
            Result load=fsInitialize();
            if (R_SUCCEEDED(load)) { load=open_playlist(wanted,false,NULL); fsExit(); }
            music_log("menu.switch",load,"menu=%s playlist=%s playing=%d",menu>=0 ? music_menus[menu].key : "none",song_menu_directory(wanted),playing);
            if (R_FAILED(load) || playing!=wanted) failed=wanted;
        }
        if (!track || !audio || wanted<0 || wanted!=playing) {
            if (started) { rc=audoutStopAudioOut(); music_log("audio.state",rc,"paused: no music menu focused"); started=false; }
            focus_ticks=0; svcSleepThread(50000000); continue;
        }
        AudioOutBuffer *released=NULL; u32 count=0;
        do {
            rc=audoutGetReleasedAudioOutBuffer(&released,&count);
            if (R_FAILED(rc)) music_log("audio.release_error",rc,"query released buffers");
            if (R_FAILED(rc) || !count) break;
            ++released_total;
            if (released==&buffers[0]) queued[0]=false;
            else if (released==&buffers[1]) queued[1]=false;
            else { rc=bad_input(); break; }
        } while (count);
        if (R_FAILED(rc)) break;
        if (shuffle && cursor>=wave.size && !queued[0] && !queued[1]) { advance=true; continue; }
        for (unsigned i=0;i<2 && R_SUCCEEDED(rc);i++) {
            if (queued[i]) continue;
            size_t length=fill(i,track+wave.offset,wave.size,&cursor,!shuffle);
            if (!length) continue;
            buffers[i].data_size=length;
            rc=audoutAppendAudioOutBuffer(&buffers[i]);
            if (R_SUCCEEDED(rc)) {
                queued[i]=true; ++queued_total;
                if (queued_total<=2) music_log("audio.first_queue",0,"buffer=%u bytes=%zu",i,length);
            } else music_log("audio.append_error",rc,"buffer=%u bytes=%zu",i,length);
        }
        if (R_SUCCEEDED(rc) && !started) { rc=audoutStartAudioOut(); started=R_SUCCEEDED(rc); music_log("audio.state",rc,"started"); }
        svcSleepThread(5000000);
    }
    music_log("plugin.exit",rc,"playback loop ended");
    close_audio("exit");
    if (pdm) pdmqryExit();
    if (service.started) pluginServiceStop(&service);
    free(track); pluginRuntimeExit(); return rc;
}
