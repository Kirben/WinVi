#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int allocations,insertionCalls;static BOOL allowAlloc;
static void *FailSplitAlloc(size_t n){allocations++;return allowAlloc?malloc(n):NULL;}
#define malloc FailSplitAlloc
#define WvAutoSplitLine ProbeSplit
#define InsertBuffer RejectSplitBuffer
#define NewPosition QuietPosition
#define InvalidateArea QuietInvalidate
#define FindValidPosition QuietFind
#define NewScrollPos QuietScroll
#define ShowEditCaret QuietCaret
#define GetXPos QuietColumn
#include "../AutoSplit.c"
#undef malloc
#undef WvAutoSplitLine
BOOL RejectSplitBuffer(LPBYTE data,WV_ULONG size,WORD flags){(void)data;(void)size;(void)flags;insertionCalls++;LinesInFile++;return FALSE;}
WV_LONG QuietPosition(PPOSITION p){(void)p;return 0;}
void QuietInvalidate(PPOSITION p,WV_LONG n,UINT flags){(void)p;(void)n;(void)flags;}
void QuietFind(PPOSITION p,WORD flags){(void)p;(void)flags;}
void QuietScroll(void){}
void QuietCaret(void){}
void QuietColumn(PCOLUMN c){(void)c;}
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int ProfileRoundtrip(void){
 WCHAR name[128];HKEY root=NULL,key=NULL;DWORD type,size,width;CHAR flag[16];int failed=1;BOOL overridden=FALSE;
 _snwprintf(name,128,L"Software\\WinViAutoSplitTest-%lu",GetCurrentProcessId());
 if(RegCreateKeyExW(HKEY_CURRENT_USER,name,0,NULL,0,KEY_ALL_ACCESS,NULL,&root,NULL)!=ERROR_SUCCESS)goto done;
 if(RegOverridePredefKey(HKEY_CURRENT_USER,root)!=ERROR_SUCCESS)goto done;overridden=TRUE;
 if(!SwitchProfile(ProfileAdd,"dummy-profile"))goto done;
 AutoSplitFlag=TRUE;AutoSplitWidth=123;SaveEditSettings();
 if(RegOpenKeyExA(root,"Software\\Ramo\\WinVi\\dummy-profile",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)goto done;
 size=sizeof(flag);if(RegQueryValueExA(key,"AutoSplit",NULL,&type,(BYTE*)flag,&size)!=ERROR_SUCCESS || type!=REG_SZ || strcmp(flag,"Yes"))goto done;
 size=sizeof(width);if(RegQueryValueExA(key,"AutoSplitWidth",NULL,&type,(BYTE*)&width,&size)!=ERROR_SUCCESS || type!=REG_DWORD || width!=123)goto done;
 AutoSplitFlag=FALSE;AutoSplitWidth=80;SaveEditSettings();
 size=sizeof(flag);if(RegQueryValueExA(key,"AutoSplit",NULL,&type,(BYTE*)flag,&size)!=ERROR_SUCCESS || strcmp(flag,"No"))goto done;
 failed=0;
done:
 if(key)RegCloseKey(key);
 if(overridden){SwitchProfile(ProfileClose,NULL);RegOverridePredefKey(HKEY_CURRENT_USER,NULL);}
 if(root){RegDeleteTreeW(root,NULL);RegCloseKey(root);RegDeleteKeyW(HKEY_CURRENT_USER,name);}
 return failed;
}
int main(void){
 PAGE page={0};BYTE bytes[64]="alpha betaX",original[64];POSITION p;int i;
 memcpy(original,bytes,sizeof(bytes));page.PageBuf=bytes;page.Fill=11;page.PageNo=1;
 FirstPage=LastPage=CurrPos.p=&page;CurrPos.i=11;CurrPos.f=0;
 Mode=InsertMode;AutoSplitFlag=TRUE;AutoSplitWidth=10;TabStop=8;DefaultNewLine=C_CRLF;
 for(i=0;i<10000;i++)ProbeSplit('X');
 CHECK(allocations==10000 && CurrPos.i==11 && !memcmp(bytes,original,sizeof(bytes)) && page.Fill==11);
 allowAlloc=TRUE;LinesInFile=0;
 for(i=0;i<1000;i++)ProbeSplit('X');
 CHECK(insertionCalls==1000 && LinesInFile==0 && CurrPos.i==11 && !memcmp(bytes,original,sizeof(bytes)) && page.Fill==11);allowAlloc=FALSE;
 allocations=0;Mode=ReplaceMode;ProbeSplit('X');Mode=InsertMode;
 ViewOnlyFlag=TRUE;ProbeSplit('X');ViewOnlyFlag=FALSE;
 HexEditMode=TRUE;ProbeSplit('X');HexEditMode=FALSE;AutoSplitFlag=FALSE;ProbeSplit('X');AutoSplitFlag=TRUE;
 AutoSplitWidth=0;ProbeSplit('X');AutoSplitWidth=1001;ProbeSplit('X');AutoSplitWidth=10;
 ProbeSplit(' ');ProbeSplit('\t');CHECK(!allocations);
 UtfEncoding=8;p.p=&page;p.i=p.f=0;
 {BYTE valid[]={0xf0,0x9f,0x98,0x80,0xc3,0xa9,0xe4,0xb8,0xad};memcpy(bytes,valid,sizeof(valid));page.Fill=sizeof(valid);
  CHECK(SplitScalar(&p)==0x1f600 && p.i==4);CHECK(SplitScalar(&p)==0xe9 && p.i==6);CHECK(SplitScalar(&p)==0x4e2d && p.i==9);}
 {BYTE invalid[][4]={{0xf4,0x90,0x80,0x80},{0xed,0xa0,0x80,0},{0xe0,0x80,0x80,0},{0xc0,0x80,0,0},{0xf0,0x9f,0,0}};
  for(i=0;i<5;i++){memcpy(bytes,invalid[i],4);page.Fill=4;p.i=p.f=0;CHECK(SplitScalar(&p)==C_UNDEF && p.i==0);}}
 UtfEncoding=16;UtfLsbFirst=TRUE;{BYTE pair[]={0x3d,0xd8,0,0xde};memcpy(bytes,pair,4);page.Fill=4;p.i=p.f=0;CHECK(SplitScalar(&p)==0x1f600 && p.i==4);}
 CHECK(!ProfileRoundtrip());
 puts("PASS 10000 planning allocation failures and 1000 storage rejections preserve text/cursor; mode/width guards; scalar decoding; isolated profile persistence");return 0;
}
