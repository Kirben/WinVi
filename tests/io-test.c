#include <windows.h>
#include <stdio.h>
#include <string.h>
/* Capture the real legacy call before compatibility macros replace it. */
static UINT (WINAPI *LegacyWrite)(HFILE,LPCCH,UINT) = _lwrite;
#include "../Win64Compat.h"
#define CHECK(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);result=1;goto done;}}while(0)
int main(void) {
 int result=0; char buf[16]={0}; DWORD length; HANDLE raw=INVALID_HANDLE_VALUE;
 HFILE h=HFILE_ERROR;
 CHECK(sizeof(HFILE)==sizeof(void *));
 raw=CreateFileA("native-io.tmp",GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
 CHECK(raw!=INVALID_HANDLE_VALUE);h=(HFILE)raw;
 CHECK(Win64Write(h,"native64",8)==8);
 CHECK(Win64Seek(h,0,FILE_BEGIN)==0);
 CHECK(Win64Read(h,buf,8)==8 && memcmp(buf,"native64",8)==0);
 CHECK(Win64Read(h,buf,8)==0);
 CHECK(Win64Seek(h,-2,FILE_END)==6);
 CHECK(Win64Read(h,buf,2)==2 && memcmp(buf,"64",2)==0);
 CHECK(Win64Read(HFILE_ERROR,buf,1)==(UINT)-1);
 CHECK(Win64Write(HFILE_ERROR,buf,1)==(UINT)-1);
 CHECK(Win64Seek(HFILE_ERROR,0,FILE_BEGIN)==-1);
 CHECK(Win64Seek(h,4,FILE_BEGIN)==4);
 /* A zero-byte legacy write truncates at the current offset. */
 CHECK(LegacyWrite((int)(INT_PTR)raw,"",0)==0);
 CHECK(GetFileSize(raw,NULL)==4);
 CHECK(Win64Write(h,"ABCD",4)==4);
 CHECK(Win64Seek(h,2,FILE_BEGIN)==2);
 CHECK(Win64Write(h,"",0)==0);
 length=GetFileSize(raw,NULL);
 CHECK(length==2);
  {LARGE_INTEGER size;DWORD ignored;char b;
 CHECK(DeviceIoControl(raw,0x900c4,NULL,0,NULL,0,&ignored,NULL));
 CHECK(Win64Seek(h,4294967312LL,FILE_BEGIN)==4294967312LL);
 CHECK(Win64Write(h,"Z",1)==1);CHECK(GetFileSizeEx(raw,&size) && size.QuadPart==4294967313LL);
 CHECK(Win64Seek(h,-1,FILE_END)==4294967312LL);CHECK(Win64Read(h,&b,1)==1 && b=='Z');}
 {const WV_LONG offsets[]={17179869184LL,34359738368LL,1099511627776LL};int k;
 for(k=0;k<3;k++){LARGE_INTEGER size;char b=0;
 CHECK(Win64Seek(h,offsets[k],FILE_BEGIN)==offsets[k]);CHECK(Win64Write(h,"Q",1)==1);
 CHECK(GetFileSizeEx(raw,&size) && size.QuadPart==offsets[k]+1);
 CHECK(Win64Seek(h,-1,FILE_END)==offsets[k]);CHECK(Win64Read(h,&b,1)==1 && b=='Q');
 }
 CHECK(Win64Seek(h,-1,FILE_BEGIN)==-1);
 CHECK(Win64Seek(h,0,FILE_BEGIN)==0);CHECK(Win64Write(h,"",0)==0);
 {LARGE_INTEGER z;CHECK(GetFileSizeEx(raw,&z) && z.QuadPart==0);}
 }
 puts("PASS native handles, read/write, EOF, relative seeks, errors, zero-byte truncation");
 done: if(raw!=INVALID_HANDLE_VALUE)CloseHandle(raw);DeleteFileA("native-io.tmp");return result;
}

