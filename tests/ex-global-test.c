/* Native failure and target-tracking probes; no external commands or files. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int FailAllocate,Allocations,Notices,CancelAfter,Checks,DialogsFrozen;
static void *ProbeAllocate(size_t n){if(FailAllocate && ++Allocations==FailAllocate)return NULL;return malloc(n);}
static void *ProbeResize(void *p,size_t n){if(FailAllocate && ++Allocations==FailAllocate)return NULL;return realloc(p,n);}
#define malloc ProbeAllocate
#define realloc ProbeResize
#define NewStatus ProbeNotice
#define Disable ProbeDisable
#define Enable ProbeEnable
#define FindValidPosition ProbePosition
#define ShowEditCaret ProbeCaret
#define CheckInterrupt ProbeInterrupt
#define NewPosition ProbeNewPosition
#define SubstituteCmd ProbeSubstitute
#define Jump ProbeJump
#include "../ExGlobal.c"
#undef malloc
#undef realloc
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void ProbeNotice(INT field,PWSTR text,INT flags){Notices++;}
void ProbeDisable(WORD flags){Disabled=1;}
void ProbeEnable(void){Disabled=0;}
void ProbePosition(PPOSITION p,WORD flags){}
void ProbeCaret(VOID){}
BOOL ProbeInterrupt(VOID){if(hwndSearch && hwndSettings && !IsWindowEnabled(hwndSearch) && !IsWindowEnabled(hwndSettings))DialogsFrozen=1;if(CancelAfter && ++Checks>=CancelAfter)Interrupted=TRUE;return Interrupted;}
WV_LONG ProbeNewPosition(PPOSITION p){CurrPos=*p;return 0;}
BOOL ProbeSubstitute(WV_LONG first,WV_LONG last,PWSTR args){CurrPos.p->PageBuf[CurrPos.i]='X';return TRUE;}
void ProbeJump(PPOSITION p){ScreenStart=*p;}
static void Document(void)
{
 static PAGE page;static BYTE bytes[]="hit\nkeep\nhit\n";
 memcpy(bytes,"hit\nkeep\nhit\n",13);
 memset(&page,0,sizeof(page));page.PageBuf=bytes;page.Fill=sizeof(bytes)-1;page.PageNo=1;page.Newlines=3;
 FirstPage=LastPage=&page;LinesInFile=3;CurrPos.p=&page;CurrPos.i=CurrPos.f=0;ScreenStart=CurrPos;
 HexEditMode=ViewOnlyFlag=ReadOnlyFile=ReadOnlyFlag=FALSE;UtfEncoding=0;MagicFlag=TRUE;
 WvExGlobalActive=WvExGlobalUndo=Interrupted=Disabled=FALSE;
 Notices=Allocations=FailAllocate=CancelAfter=Checks=0;
}
int main(void)
{
 WV_ULONG marks[]={0x100000010ULL,0x100000020ULL,0x100000030ULL};int i;
 Targets=marks;TargetCount=3;NextTarget=0;TargetShift=0;Tracking=TRUE;TrackingError=FALSE;
 WvExGlobalEdit(0,0,7);CHECK(TargetShift==7 && NextTarget==0);
 WvExGlobalEdit(0x100000010ULL+7,17,0);CHECK(NextTarget==2 && TargetShift==-10);
 CHECK(Targets[NextTarget]-(WV_ULONG)(-TargetShift)==0x100000026ULL);
 Tracking=FALSE;Targets=NULL;TargetCount=NextTarget=0;TargetShift=0;
 puts("PASS original-line invalidation and lazy 64-bit shifts beyond 4 GiB");
 for(i=1;i<=2;i++){
  WCHAR args[]=L"/hit/d";Document();FailAllocate=i;
  CHECK(!WvExGlobal((WV_ULONG)-1,(WV_ULONG)-1,args,FALSE));
  CHECK(FirstPage->Fill==13 && !memcmp(FirstPage->PageBuf,"hit\nkeep\nhit\n",13));
  CHECK(!WvExGlobalActive && !WvExGlobalUndo && !WvExGlobalEditHook && !Targets && !Disabled);
 }
 puts("PASS pattern/target allocation failures leave the document unchanged and release state");
 {WCHAR args[]=L"/hit/d";Document();CancelAfter=1;
  CHECK(!WvExGlobal((WV_ULONG)-1,(WV_ULONG)-1,args,FALSE));CHECK(FirstPage->Fill==13 && !WvExGlobalActive && !Targets);}
 puts("PASS marking cancellation leaves the original buffer untouched");
 {WCHAR args[]=L"/hit/d";Document();CancelAfter=1;DialogsFrozen=0;
  hwndSearch=CreateWindowW(L"STATIC",L"dummy search",WS_OVERLAPPED,0,0,10,10,NULL,NULL,GetModuleHandle(NULL),NULL);
  hwndSettings=CreateWindowW(L"STATIC",L"dummy settings",WS_OVERLAPPED,0,0,10,10,NULL,NULL,GetModuleHandle(NULL),NULL);
  CHECK(hwndSearch && hwndSettings);
  CHECK(!WvExGlobal((WV_ULONG)-1,(WV_ULONG)-1,args,FALSE));
  CHECK(DialogsFrozen && IsWindowEnabled(hwndSearch) && IsWindowEnabled(hwndSettings));
  DestroyWindow(hwndSearch);DestroyWindow(hwndSettings);hwndSearch=hwndSettings=NULL;}
 puts("PASS modeless search/settings controls frozen during execution and restored after cancellation");
 {WCHAR args[]=L"/hit/s/hit/X/";Document();CancelAfter=12;
  CHECK(!WvExGlobal((WV_ULONG)-1,(WV_ULONG)-1,args,FALSE));
  CHECK(!memcmp(FirstPage->PageBuf,"Xit\nkeep\nhit\n",13));
  CHECK(!Targets && !WvExGlobalActive && !WvExGlobalUndo && !WvExGlobalEditHook);}
 puts("PASS execution cancellation stops before the next original target and releases state (body mocked)");
 for(i=0;i<10000;i++){
  WCHAR args[100];Document();_snwprintf(args,100,L"/hit/unsupported%u",i);
  CHECK(!WvExGlobal((WV_ULONG)-1,(WV_ULONG)-1,args,FALSE));CHECK(!Targets && !WvExGlobalActive && FirstPage->Fill==13);
 }
 puts("PASS 10000 unsupported command bodies rejected without dispatch");return 0;
}
