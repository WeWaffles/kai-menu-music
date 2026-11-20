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
 puts("WAV parser, filename selection and HOME focus tests passed");
}
