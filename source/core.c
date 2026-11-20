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
bool focus_step(bool home, unsigned kind, unsigned applet, unsigned event) {
    if (kind==2 || kind==4) return false; // power or log initialization
    if (kind!=0) return home;
    if (event==2) return applet==3; // explicit SystemAppletMenu focus only
    if (applet==3 && (event==1 || event==3 || event==4 || event==5 || event==6)) return false;
    return home;
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
