#include "../source/songs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char setting[FS_MAX_PATH+2];static size_t setting_size;
static bool have_setting,have_mode,have_a=true,have_b=true;
static char mode[2];
static unsigned writes,opens,closes;
static const unsigned char wav[]={
 'R','I','F','F',40,0,0,0,'W','A','V','E',
 'f','m','t',' ',16,0,0,0,1,0,2,0,0x80,0xbb,0,0,0,0xee,2,0,4,0,16,0,
 'd','a','t','a',4,0,0,0,0,0,0,0};
Result fsFsOpenFile(FsFileSystem *sd,const char *path,unsigned mode,FsFile *file){
 (void)sd;
 if(!strcmp(path,MUSIC_SHUFFLE)){if(!have_mode)return 0x202;file->kind=3;}
 else if(!strcmp(path,MUSIC_SELECTION)){if(!have_setting)return 0x202;file->kind=1;}
 else if(!strcmp(path,MUSIC_DIRECTORY "/broken.wav")){assert(mode==FsOpenMode_Read);file->kind=4;}
 else if((!strcmp(path,MUSIC_DIRECTORY "/a.wav")&&have_a)||(!strcmp(path,MUSIC_DIRECTORY "/b.wav")&&have_b)){assert(mode==FsOpenMode_Read);file->kind=2;}
 else return 0x202;
 opens++;return 0;
}
Result fsFileGetSize(FsFile *f,s64 *size){*size=f->kind==1?setting_size:f->kind==3?2:sizeof wav;return 0;}
Result fsFileRead(FsFile *f,s64 offset,void *out,u64 length,unsigned option,u64 *got){
 assert(!option&&offset>=0);size_t size=f->kind==1?setting_size:f->kind==3?2:sizeof wav;
 if((u64)offset>size||length>size-(u64)offset)return 0x202;
 if(f->kind==4){memset(out,0,length);*got=length;return 0;}
 const void *source=f->kind==1?(const void*)setting:f->kind==3?(const void*)mode:(const void*)wav;
 memcpy(out,(const unsigned char*)source+offset,length);*got=length;return 0;
}
void fsFileClose(FsFile *f){(void)f;closes++;}
Result fsFsCreateFile(FsFileSystem *sd,const char *path,s64 size,unsigned option){
 (void)sd;assert(!option&&size>0);
 if(!strcmp(path,MUSIC_SHUFFLE)){assert(size==2);if(have_mode)return 0x402;have_mode=true;return 0;}
 assert(!strcmp(path,MUSIC_SELECTION));if(have_setting)return 0x402;have_setting=true;setting_size=size;return 0;
}
Result fsFileSetSize(FsFile *f,s64 size){if(f->kind==3){assert(size==2);return 0;}assert(f->kind==1&&size>0&&(size_t)size<sizeof setting);setting_size=size;return 0;}
Result fsFileWrite(FsFile *f,s64 off,const void *input,u64 length,unsigned option){
 assert(!off&&option==FsWriteOption_Flush);
 if(f->kind==3){assert(length==2);memcpy(mode,input,2);}else{assert(f->kind==1&&length==setting_size);memcpy(setting,input,length);}
 writes++;return 0;
}
Result fsFsGetEntryType(FsFileSystem *sd,const char *path,FsDirEntryType *type){(void)sd;if((!strcmp(path,MUSIC_DIRECTORY "/a.wav")&&have_a)||(!strcmp(path,MUSIC_DIRECTORY "/b.wav")&&have_b)){*type=FsDirEntryType_File;return 0;}return 0x202;}
Result fsFsOpenDirectory(FsFileSystem *sd,const char *path,unsigned mode,FsDir *dir){(void)sd;assert(!strcmp(path,MUSIC_DIRECTORY)&&mode==FsDirOpenMode_ReadFiles);dir->cursor=0;return 0;}
Result fsDirRead(FsDir *dir,s64 *count,s64 capacity,FsDirectoryEntry *entry){
 static const char *names[]={"b.wav","._hidden.wav","a.wav","broken.wav","notes.txt"};assert(capacity==1);
 while(dir->cursor<5){const char *name=names[dir->cursor++];if((!have_b&&!strcmp(name,"b.wav"))||(!have_a&&!strcmp(name,"a.wav")))continue;strcpy(entry->name,name);*count=1;return 0;}
 *count=0;return 0;
}
void fsDirClose(FsDir *dir){(void)dir;}
static uint32_t random_zero(void *context){(void)context;return 0;}
int main(void){
 FsFileSystem sd={0};char path[FS_MAX_PATH],name[FS_MAX_PATH];Wave wave;
 assert(!song_resolve(&sd,NULL,path,name)&&!strcmp(name,"a.wav"));
 assert(!song_save_selection(&sd,"b.wav")&&writes==1);
 assert(!song_resolve(&sd,NULL,path,name)&&!strcmp(name,"b.wav"));
 assert(!song_validate(&sd,name,&wave)&&wave.offset==44&&wave.size==4);
 have_b=false;assert(!song_resolve(&sd,NULL,path,name)&&!strcmp(name,"a.wav"));
 strcpy(setting,"../escape.wav\n");setting_size=strlen(setting);
 assert(!song_resolve(&sd,NULL,path,name)&&!strcmp(name,"a.wav"));
 assert(song_save_selection(&sd,"../escape.wav")!=0&&writes==1);
 assert(!song_save_selection(&sd,"a.wav")&&writes==2);
 assert(!song_read_selection(&sd,name)&&!strcmp(name,"a.wav"));
 bool enabled=true;
 assert(song_read_shuffle(&sd,&enabled)!=0&&!enabled);
 assert(!song_save_shuffle(&sd,true));assert(!song_read_shuffle(&sd,&enabled)&&enabled);
 assert(!song_save_shuffle(&sd,false));assert(!song_read_shuffle(&sd,&enabled)&&!enabled);
 have_b=true;
 assert(!song_choose_shuffle(&sd,"a.wav",random_zero,NULL,name)&&!strcmp(name,"b.wav"));
 assert(!song_choose_shuffle(&sd,"b.wav",random_zero,NULL,name)&&!strcmp(name,"a.wav"));
 have_b=false;
 assert(!song_choose_shuffle(&sd,"a.wav",random_zero,NULL,name)&&!strcmp(name,"a.wav"));
 have_a=false;
 assert(song_choose_shuffle(&sd,"a.wav",random_zero,NULL,name)!=0);
 assert(opens==closes);
 puts("Selection persistence, missing-song fallback and streamed validation checks passed");
}
