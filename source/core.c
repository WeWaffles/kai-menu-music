// SPDX-License-Identifier: MIT
#include "core.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1]<<8; }
bool wave_parse(const uint8_t *b, size_t n, Wave *w) {
    if (!b || !w || n < 12 || memcmp(b,"RIFF",4) || memcmp(b+8,"WAVE",4)) return false;
    uint64_t end = (uint64_t)le32(b+4)+8;
    if (end < 12 || end > n) return false;
    bool fmt = false, data = false;
    Wave found = {0};
    for (size_t p=12; p < end;) {
        if (end-p < 8) return false;
        size_t len=le32(b+p+4), start=p+8;
        if (len > end-start) return false;
        if (!memcmp(b+p,"fmt ",4)) {
            if (fmt || len < 16 || le16(b+start)!=1 || le16(b+start+2)!=2 ||
                le32(b+start+4)!=48000 || le32(b+start+8)!=192000 ||
                le16(b+start+12)!=4 || le16(b+start+14)!=16) return false;
            fmt=true;
        } else if (!memcmp(b+p,"data",4)) {
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
    if (n<5 || name[n-4]!='.') return false;
    const char *ext="wav";
    for (unsigned i=0;i<3;i++) {
        unsigned char c=(unsigned char)name[n-3+i];
        if (c>='A' && c<='Z') c=(unsigned char)(c+('a'-'A'));
        if (c!=(unsigned char)ext[i]) return false;
    }
    return true;
}
