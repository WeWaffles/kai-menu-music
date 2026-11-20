// SPDX-License-Identifier: MIT
#include "../source/core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char wav[] = {
 'R','I','F','F',40,0,0,0,'W','A','V','E',
 'f','m','t',' ',16,0,0,0,1,0,2,0,0x80,0xbb,0,0,0,0xee,2,0,4,0,16,0,
 'd','a','t','a',4,0,0,0,0,0,0,0
};
typedef struct { size_t reads,largest; bool fail; } Probe;
static bool probe_reader(void *context,size_t offset,void *output,size_t length) {
 Probe *p=context;
 p->reads++; if (length>p->largest) p->largest=length;
 if (p->fail || offset>sizeof wav || length>sizeof wav-offset || offset+length>44) return false;
 memcpy(output,wav+offset,length); return true;
}
int main(void) {
 Wave w={0};
 assert(wave_parse(wav,sizeof wav,&w) && w.offset==44 && w.size==4);
 for (size_t i=0;i<sizeof wav;i++) assert(!wave_parse(wav,i,&w));
 unsigned char b[sizeof wav];
 memcpy(b,wav,sizeof b); b[24]=0x44; assert(!wave_parse(b,sizeof b,&w));
 memcpy(b,wav,sizeof b); b[40]=3; assert(!wave_parse(b,sizeof b,&w));
 memcpy(b,wav,sizeof b); memset(b+40,255,4); assert(!wave_parse(b,sizeof b,&w));
 memcpy(b,wav,sizeof b); b[20]=3; assert(!wave_parse(b,sizeof b,&w));
 memcpy(b,wav,sizeof b); b[4]=0; assert(!wave_parse(b,sizeof b,&w));
 unsigned char odd[58];
 memcpy(odd,wav,12); odd[4]=50;
 memcpy(odd+12,"JUNK\1\0\0\0\0\0",10);
 memcpy(odd+22,wav+12,36);
 assert(wave_parse(odd,sizeof odd,&w) && w.offset==54);
 // Odd chunk without padding is invalid.
 odd[4]=49; assert(!wave_parse(odd,57,&w));
 assert(!focus_step(false,0,1,3)); // game's loss of focus alone proves nothing
 assert(focus_step(false,0,3,2));
 assert(!focus_step(true,0,1,2));
 assert(!focus_step(true,0,3,3));
 assert(!focus_step(true,2,0,0)); // sleep/wake invalidates HOME
 assert(!focus_step(true,4,0,0));
 assert(focus_step(true,3,0,0)); // docking preserves known focus
 assert(focus_step(false,0,3,2));
 assert(wave_filename("yourfile.wav"));
 assert(wave_filename("Menu Music.WaV"));
 assert(!wave_filename("._menu.wav"));
 assert(!wave_filename(".wav"));
 assert(!wave_filename("music.mp3"));
 assert(!wave_filename("song.wav.tmp"));
 assert(!wave_filename("../song.wav"));
 assert(!wave_filename("folder/song.wav"));
 assert(!wave_filename("folder\\song.wav"));
 assert(!wave_filename(NULL));
 assert(!wave_filename(""));
 char selected[128];
 assert(selection_parse("Song One.wav\n",13,selected,sizeof selected));
 assert(!strcmp(selected,"Song One.wav"));
 assert(selection_parse("Song One.wav\r\n",14,selected,sizeof selected));
 assert(!selection_parse("../song.wav\n",12,selected,sizeof selected));
 assert(!selection_parse("song.wav\nother.wav",18,selected,sizeof selected));
 assert(!selection_parse("song.wav\0evil.wav",17,selected,sizeof selected));
 assert(!selection_parse("song.wav",8,selected,8));
 assert(!selection_parse("\n",1,selected,sizeof selected));
 Probe probe={0};
 assert(wave_probe(probe_reader,&probe,sizeof wav,&w));
 assert(w.offset==44 && w.size==4 && probe.largest<=16 && probe.reads==4);
 probe.fail=true; assert(!wave_probe(probe_reader,&probe,sizeof wav,&w));
 unsigned char data[12]={1,2,3,4,5,6,7,8,9,10,11,12},block[16]; size_t position=0;
 assert(wave_copy_block(block,8,data,12,&position,false)==8 && position==8);
 assert(!memcmp(block,data,8));
 assert(wave_copy_block(block,8,data,12,&position,false)==4 && position==12);
 assert(!memcmp(block,data+8,4) && !memcmp(block+4,"\0\0\0\0",4));
 assert(wave_copy_block(block,8,data,12,&position,false)==0 && position==12);
 assert(wave_copy_block(block,16,data,12,&position,true)==16 && position==4);
 assert(!memcmp(block,data,12) && !memcmp(block+12,data,4));
 puts("WAV parser, filename selection and HOME focus tests passed");
}
