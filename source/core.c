// SPDX-License-Identifier: MIT
#include "core.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1]<<8; }
bool wave_probe(WaveReader read,void *context,size_t n,Wave *w) {
    uint8_t header[16];
    if (!read || !w || n<12 || !read(context,0,header,12) || memcmp(header,"RIFF",4) || memcmp(header+8,"WAVE",4)) return false;
    uint64_t end = (uint64_t)le32(header+4)+8;
    if (end < 12 || end > n) return false;
    bool fmt = false, data = false;
    Wave found = {0};
    for (size_t p=12; p < end;) {
        if (end-p < 8) return false;
        if (!read(context,p,header,8)) return false;
        size_t len=le32(header+4), start=p+8;
        if (len > end-start) return false;
        if (!memcmp(header,"fmt ",4)) {
            if (fmt || len < 16 || !read(context,start,header,16) || le16(header)!=1 || le16(header+2)!=2 ||
                le32(header+4)!=48000 || le32(header+8)!=192000 ||
                le16(header+12)!=4 || le16(header+14)!=16) return false;
            fmt=true;
        } else if (!memcmp(header,"data",4)) {
            if (data || !len || len%4) return false;
            found=(Wave){start,len}; data=true;
        }
        uint64_t next=(uint64_t)start+len+(len&1);
        if (next>end) return false;
        p=(size_t)next;
    }
    if (!fmt || !data) return false;
    *w=found;
    return true;
}
typedef struct { const uint8_t *bytes; size_t size; } MemoryWave;
static bool memory_read(void *context,size_t offset,void *output,size_t length) {
    MemoryWave *memory=context;
    if (offset>memory->size || length>memory->size-offset) return false;
    memcpy(output,memory->bytes+offset,length); return true;
}
bool wave_parse(const uint8_t *b,size_t n,Wave *w) {
    if (!b) return false;
    MemoryWave memory={b,n}; return wave_probe(memory_read,&memory,n,w);
}
// Applet ids follow libnx AppletId; PDM event types follow PdmAppletEventType.
enum { APPLET_HOME=0x03, APPLET_LIBRARY_FIRST=0x0A };
enum { EVENT_EXIT=1, EVENT_IN_FOCUS=2, EVENT_OUT_OF_FOCUS=3, EVENT_OUT_OF_FOCUS4=4, EVENT_EXIT5=5, EVENT_EXIT6=6 };
const MusicMenu music_menus[MENU_COUNT]={
    {"home","HOME Menu",APPLET_HOME},
    {"settings","System Settings",0x16},   // LibraryAppletSet
    // Switch 2 GameShare applet 0x01000024 (libounce); PDM keeps only the low byte. Unverified.
    {"gameshare","GameShare",0x24},
    {"profile","User Page",0x1A},          // LibraryAppletMyPage
    {"controllers","Controllers",0x0C},    // LibraryAppletController
    {"album","Album",0x15},                // LibraryAppletPhotoViewer
};
int menu_from_applet(unsigned applet) {
    for (int i=0;i<MENU_COUNT;i++) if (music_menus[i].applet==applet) return i;
    return MENU_NONE;
}
static bool library_applet(unsigned applet) { return applet>=APPLET_LIBRARY_FIRST; }
static int stack_find(const MenuFocus *f,unsigned applet) {
    for (unsigned i=0;i<f->depth;i++) if (f->stack[i]==applet) return (int)i;
    return -1;
}
void menu_focus_step(MenuFocus *f,unsigned kind,unsigned applet,unsigned event) {
    if (!f) return;
    if (kind==2 || kind==4) { *f=(MenuFocus){0}; return; } // power or log initialization
    if (kind!=0) return;
    if (event==EVENT_IN_FOCUS) {
        if (applet==APPLET_HOME) { f->home=true; f->depth=0; }
        else if (library_applet(applet)) {
            int at=stack_find(f,applet);
            if (at>=0) f->depth=(unsigned)at+1;
            else if (f->depth<sizeof f->stack/sizeof f->stack[0]) f->stack[f->depth++]=applet;
        } else { f->home=false; f->depth=0; } // a game or other applet took over
        f->current=applet; return;
    }
    bool exit=event==EVENT_EXIT || event==EVENT_EXIT5 || event==EVENT_EXIT6;
    bool out=event==EVENT_OUT_OF_FOCUS || event==EVENT_OUT_OF_FOCUS4;
    if (exit && library_applet(applet)) {
        int at=stack_find(f,applet);
        if (at>=0) f->depth=(unsigned)at;
        // Closing a menu returns to whatever opened it.
        if (applet==f->current) f->current=f->depth ? f->stack[f->depth-1] : f->home ? APPLET_HOME : 0;
        return;
    }
    if ((exit || out) && applet==f->current) {
        f->current=0;
        if (exit && applet==APPLET_HOME) f->home=false;
    }
}
int menu_focus_menu(const MenuFocus *f) {
    if (!f || !f->current) return MENU_NONE;
    if (f->current==APPLET_HOME) return f->home ? MENU_HOME : MENU_NONE;
    // A library applet only gets music when HOME opened it, never from a game.
    if (!f->home || stack_find(f,f->current)<0) return MENU_NONE;
    return menu_from_applet(f->current);
}

bool wave_filename(const char *name) {
    if (!name || !*name || name[0]=='.' || strchr(name,'/') || strchr(name,'\\')) return false;
    size_t n=strlen(name);
    for (size_t i=0;i<n;i++) if ((unsigned char)name[i]<32 || (unsigned char)name[i]==127) return false;
    if (n<5 || name[n-4]!='.') return false;
    const char *ext="wav";
    for (unsigned i=0;i<3;i++) {
        unsigned char c=(unsigned char)name[n-3+i];
        if (c>='A' && c<='Z') c=(unsigned char)(c+('a'-'A'));
        if (c!=(unsigned char)ext[i]) return false;
    }
    return true;
}
bool selection_parse(const char *text,size_t length,char *name,size_t capacity) {
    if (!text || !name || !capacity || !length) return false;
    if (text[length-1]=='\n') { --length; if (length && text[length-1]=='\r') --length; }
    if (!length || length>=capacity || memchr(text,0,length)) return false;
    memcpy(name,text,length); name[length]=0; return wave_filename(name);
}
size_t wave_copy_block(void *output,size_t capacity,const void *data,size_t size,size_t *cursor,bool loop) {
    if (!output || !data || !cursor || !size || size%4 || capacity%4 || *cursor>size || *cursor%4) return 0;
    size_t done=0; memset(output,0,capacity);
    if (loop && *cursor==size) *cursor=0;
    while (done<capacity && *cursor<size) {
        size_t length=size-*cursor;
        if (length>capacity-done) length=capacity-done;
        memcpy((unsigned char*)output+done,(const unsigned char*)data+*cursor,length);
        done+=length; *cursor+=length;
        if (loop && *cursor==size) *cursor=0;
    }
    return done;
}
