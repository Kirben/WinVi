#define NDEBUG
#define WINVI_PLATFORM_IMPLEMENTATION
#include "../Page.c"
BOOL SessionEnding=FALSE,DefaultTmpFlag=TRUE;
PWSTR TmpDirectory=NULL;
WV_LONG PageThreshold=16;
HINSTANCE hInst=NULL;
HWND hwndMain=NULL;
void NewStatus(INT field,PWSTR text,INT flags) {(void)field;(void)text;(void)flags;}
UINT Language=0,CharSet=0;
BOOL HexEditMode=FALSE,UtfLsbFirst=FALSE,AdditionalNullByte=FALSE;
INT VisiLines=0,CurrLine=0,UtfEncoding=0;
WV_LONG FirstVisible=0;
WV_ULONG LinesInFile=0,SelectCount=0;
LPPAGE FirstPage=NULL,LastPage=NULL;
POSITION ScreenStart={0},CurrPos={0},SelectStart={0};
INT AnsiCodePage=0,OemCodePage=0;
PLINEINFO LineInfo=NULL;
BYTE CharFlags(INT c){(void)c;return 0;}
BOOL EnterDeleteForUndo(PPOSITION p,WV_ULONG n,WORD f){(void)p;(void)n;(void)f;return TRUE;}
BOOL EnterInsertForUndo(PPOSITION p,WV_ULONG n){(void)p;(void)n;return TRUE;}
#define CHECK(x) do {if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void Drop(PAGE *p){if(p->PageBuf){HGLOBAL h=GLOBALHANDLE(p->PageBuf);UnlinkResident(p);GlobalUnlock(h);GlobalFree(h);p->PageBuf=NULL;--PagesOccupied;}}
int main(void) {
 PAGE p={0},a={0},b={0},c={0},d={0};BYTE data[PAGESIZE];DWORD n;int i;
 {PAGE x={0},y={0};LINEINFO lines[1]={0};LineInfo=lines;FirstPage=LastPage=&x;x.Fill=20;
  MarkPos[1].p=MarkPos[2].p=MarkPos[3].p=&x;MarkPos[1].i=1;MarkPos[2].i=2;MarkPos[3].i=5;
  CorrectLineInfo(&x,3,&x,7,2,4);CHECK(MarkPos[1].i==1 && MarkPos[2].i==6 && MarkPos[3].i==9);
  MarkPos[1].i=5;MarkPos[1].f=2;MarkPos[2].i=7;MarkPos[3].i=11;
  CorrectLineInfo(&x,8,&x,4,4,-4);CHECK(MarkPos[1].i==4 && !MarkPos[1].f && MarkPos[2].i==4 && MarkPos[3].i==7);
  CorrectLineInfo(&x,6,&y,0,0,0);CHECK(MarkPos[3].p==&y && MarkPos[3].i==1);
  memset(MarkPos,0,sizeof(MarkPos));x.Fill=7;MarkPos[1].p=&x;MarkPos[1].i=4;CorrectLineInfo(&x,5,&x,8,4,3);CHECK(MarkPos[1].i==7);
  x.Fill=10;y.Fill=10;x.Next=&y;y.Prev=&x;MarkPos[1].p=&x;MarkPos[1].i=8;MarkPos[2].p=&y;MarkPos[2].i=2;
  CorrectLineInfo(&x,10,&x,5,5,-5);CHECK(MarkPos[1].p==&x && MarkPos[1].i==5 && MarkPos[2].p==&y && MarkPos[2].i==2);
  x.Fill=5;CorrectLineInfo(&y,4,&y,0,5,-4);CHECK(MarkPos[2].p==&y && MarkPos[2].i==0);
  memset(MarkPos,0,sizeof(MarkPos));x.Fill=1;y.Fill=6;MarkPos[1].p=&x;MarkPos[1].i=1;MarkPos[2].p=&y;
  CorrectLineInfo(&y,1,&y,2,1,1);CHECK(MarkPos[1].p==&y && MarkPos[1].i==1 && MarkPos[2].i==1);
  memset(MarkPos,0,sizeof(MarkPos));LineInfo=NULL;FirstPage=LastPage=NULL;
  puts("PASS mark tracking: insertion boundaries, deletion clamping, page splits, append and multi-page deletion");
 }
 CHECK(!Reserve(NULL,PAGESIZE+1,1) && LinesInFile==0);
 CHECK(!Reserve(NULL,(-9223372036854775807LL-1),0));
 for(i=0;i<PAGESIZE;i++)data[i]=(BYTE)i;
 TmpFile=CreateFileW(L"page-fault.tmp",GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
 CHECK(TmpFile!=INVALID_HANDLE_VALUE);
 CHECK(WriteFile(TmpFile,data,PAGESIZE,&n,NULL) && n==PAGESIZE);
 p.PageFilePos=0;p.Fill=PAGESIZE;
 CHECK(ReloadPage(&p) && memcmp(p.PageBuf,data,PAGESIZE)==0 && PagesOccupied==1);
 Drop(&p);CHECK(PagesOccupied==0);
 CHECK(_llseek((HFILE)TmpFile,0,0)==0 && SetEndOfFile(TmpFile));
 CHECK(!ReloadPage(&p) && p.PageBuf==NULL && PagesOccupied==0 && ResidentHead==NULL);
 CHECK(_llseek((HFILE)TmpFile,0,0)==0 && WriteFile(TmpFile,data,PAGESIZE,&n,NULL));
 CHECK(ReloadPage(&p) && !memcmp(p.PageBuf,data,PAGESIZE));
 a.PageFilePos=b.PageFilePos=c.PageFilePos=d.PageFilePos=-1;
 CHECK(AllocPageMem(&a) && AllocPageMem(&b) && AllocPageMem(&c) && PagesOccupied==4);
 p.PageFilePos=-1;LastTmpOffset=(WV_LONG)(WV_MAX_POSITION-PAGESIZE+1);
 CHECK(!AllocPageMem(&d) && d.PageBuf==NULL && p.PageBuf!=NULL && PagesOccupied==4);
 LastTmpOffset=0;
 CHECK(AllocPageMem(&d) && p.PageBuf==NULL && PagesOccupied==4);
 Drop(&a);Drop(&b);Drop(&c);Drop(&d);CloseHandle(TmpFile);DeleteFileW(L"page-fault.tmp");
 puts("PASS actual pager: successful reload, truncated-store failure cleanup, temporary-offset overflow and eviction");return 0;
}
