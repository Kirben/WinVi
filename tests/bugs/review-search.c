#include <windows.h>
#include <stdlib.h>
extern UINT Interrupted;
static int BugInterruptAfter=-1;
static DWORD BugTime(void){if(BugInterruptAfter==0)Interrupted=TRUE;else if(BugInterruptAfter>0)BugInterruptAfter--;return 0;}
#undef GetCurrentTime
#define GetCurrentTime BugTime
static int BugAllocation=-1;
static void *BugCalloc(size_t n,size_t size){if(BugAllocation==0){BugAllocation=-1;return NULL;}if(BugAllocation>0)BugAllocation--;return calloc(n,size);}
#define calloc BugCalloc
#include "../../Search.c"
#undef calloc
int BugSearchOOM(void) {
 const WCHAR *patterns[]={L"/./",L"/a./",L"/[abc]/",L"/abc/",L"/a*b*[c-z]/",L"/\\(a\\|b\\)/"};
 int i,j;WCHAR pattern[80];INT error=0;
 for(i=0;i<_countof(patterns);i++)for(j=0;j<8;j++) {
  WvCopyString(pattern,patterns[i]);BugAllocation=j;BuildMatchList(pattern,L'/',&error);
  if(BugAllocation==-1 && MatchValid)return 1;
 }
 BugAllocation=-1;return 0;
}
int BugSearchDepth(void) {
 PAGE page={0};BYTE data[2]={'a','c'};WCHAR *pattern=malloc(24004*sizeof(WCHAR));POSITION pos;INT error=0,i;BOOL matched;
 if(!pattern)return 1;FirstPage=LastPage=CurrPos.p=ScreenStart.p=&page;page.PageBuf=data;page.Fill=2;page.PageNo=1;
 pattern[0]='/';for(i=0;i<12000;i++){pattern[1+2*i]='.';pattern[2+2*i]='*';}
 pattern[24001]='b';pattern[24002]='/';pattern[24003]=0;Interrupted=FALSE;BugAllocation=-1;BugInterruptAfter=-1;
 BuildMatchList(pattern,'/',&error);pos=CurrPos;matched=CheckFwd(&pos,FirstMatch);free(pattern);
 return !matched&&Interrupted?0:1;
}
int BugSearchCancel(void) {
 PAGE page={0};BYTE data[34];WCHAR pattern[100];POSITION pos;INT error=0,i;
 memset(data,'a',sizeof(data));data[33]='c';page.PageBuf=data;page.Fill=sizeof(data);page.PageNo=1;
 FirstPage=LastPage=CurrPos.p=ScreenStart.p=&page;
 pattern[0]='/';for(i=0;i<24;i++){pattern[1+2*i]='a';pattern[2+2*i]='*';}
 pattern[49]='b';pattern[50]='/';pattern[51]=0;
 BuildMatchList(pattern,L'/',&error);pos=CurrPos;StartTime=0;Interrupted=FALSE;BugInterruptAfter=5;
 if(CheckFwd(&pos,FirstMatch))return 1;
 return Interrupted?0:1;
}


