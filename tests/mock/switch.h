#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t Result;
#define R_FAILED(x) ((x)!=0)
#define R_SUCCEEDED(x) ((x)==0)
#define MAKERESULT(m,d) ((Result)((m)|((d)<<9)))
enum {Module_Libnx=345,LibnxError_BadInput=2,LibnxError_NotFound=3,FS_MAX_PATH=0x301,
 FsOpenMode_Read=1,FsOpenMode_Write=2,FsReadOption_None=0,FsWriteOption_Flush=1,FsDirOpenMode_ReadFiles=1};
typedef enum {FsDirEntryType_Dir=0,FsDirEntryType_File=1} FsDirEntryType;
typedef struct {unsigned kind;} FsFile;
typedef struct {unsigned unused;} FsFileSystem;
typedef struct {unsigned cursor;} FsDir;
typedef struct {char name[FS_MAX_PATH];} FsDirectoryEntry;
Result fsFsOpenFile(FsFileSystem*,const char*,unsigned,FsFile*);
Result fsFileGetSize(FsFile*,s64*);
Result fsFileRead(FsFile*,s64,void*,u64,unsigned,u64*);
void fsFileClose(FsFile*);
Result fsFsCreateFile(FsFileSystem*,const char*,s64,unsigned);
Result fsFileSetSize(FsFile*,s64);
Result fsFileWrite(FsFile*,s64,const void*,u64,unsigned);
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*);
Result fsFsOpenDirectory(FsFileSystem*,const char*,unsigned,FsDir*);
Result fsDirRead(FsDir*,s64*,s64,FsDirectoryEntry*);
void fsDirClose(FsDir*);
