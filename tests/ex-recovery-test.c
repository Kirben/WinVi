#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static int allocations,failPage,cancelAfter,work;
#define NewPage ProbeNewPage
#define CheckInterrupt ProbeCancel
#include "../ExRecovery.c"
#undef NewPage
#undef CheckInterrupt
LPPAGE NewPage(void);
LPPAGE ProbeNewPage(void){return ++allocations==failPage?NULL:NewPage();}
BOOL ProbeCancel(void){return cancelAfter && ++work>=cancelAfter;}
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s (error %lu)\n",__LINE__,#x,GetLastError());exit(1);}}while(0)
static void VerifyOld(LPPAGE original)
{
 int page=0;CHECK(FirstPage==original);
 for(LPPAGE p=FirstPage;p;p=p->Next,page++){
  CHECK(p->PageBuf || ReloadPage(p));CHECK(p->Fill==PAGESIZE);
  for(int i=0;i<PAGESIZE;i++)CHECK(p->PageBuf[i]==(BYTE)(page+i));
 }
 CHECK(page==32);
}
int wmain(void)
{
 WCHAR path[WV_PATH_CAP],directory[WV_PATH_CAP];BYTE bytes[65536];DWORD got;
 LPPAGE old,last=NULL,staged;WV_ULONG lines;WV_RECOVERY record={0};
 hInst=GetModuleHandleW(NULL);hwndMain=NULL;VisiLines=0;LineInfo=calloc(2,sizeof(LINEINFO));
 CHECK(GetCurrentDirectoryW(WV_PATH_CAP,directory)>0);wcscat(directory,L"\\tests\\cases");
 CHECK(CreateDirectoryW(directory,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);
 TmpDirectory=directory;DefaultTmpFlag=FALSE;PageThreshold=16;CharSet=CS_ANSI;UtfEncoding=0;
 for(int n=0;n<32;n++){
  LPPAGE p=NewPage();CHECK(p!=NULL);p->Prev=last;if(last)last->Next=p;else FirstPage=p;
  for(int i=0;i<PAGESIZE;i++)p->PageBuf[i]=(BYTE)(n+i);p->Fill=PAGESIZE;p->PageNo=n+1;last=p;
 }
 old=FirstPage;LastPage=last;
 wcscpy(path,directory);wcscat(path,L"\\recovery-stage.bin");
 record.file=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,NULL);
 CHECK(record.file!=INVALID_HANDLE_VALUE);
 for(int i=0;i<65536;i++)bytes[i]=(i%19==0)?'\n':'X';
 CHECK(WriteFile(record.file,bytes,sizeof(bytes),&got,NULL) && got==sizeof(bytes));
 record.length=sizeof(bytes);record.meta.newLine=10;
 for(failPage=1;failPage<=16;failPage++){
  CHECK(SetFilePointer(record.file,0,NULL,FILE_BEGIN)==0);allocations=0;
  CHECK(StageRecovery(&record,&last,&lines)==NULL);VerifyOld(old);
 }
 failPage=0;cancelAfter=3;work=0;CHECK(SetFilePointer(record.file,0,NULL,FILE_BEGIN)==0);
 CHECK(StageRecovery(&record,&last,&lines)==NULL);VerifyOld(old);cancelAfter=0;
 record.length=65538;CHECK(SetFilePointer(record.file,0,NULL,FILE_BEGIN)==0);
 CHECK(StageRecovery(&record,&last,&lines)==NULL);VerifyOld(old);
 record.length=65536;CHECK(SetFilePointer(record.file,0,NULL,FILE_BEGIN)==0);
 staged=StageRecovery(&record,&last,&lines);CHECK(staged!=NULL);VerifyOld(old);
 WvReplacePageChain(staged,last);CHECK(FirstPage==staged);CHECK(lines==(65535/19)+1);
 unsigned at=0;for(LPPAGE p=FirstPage;p;p=p->Next){
  CHECK(p->PageBuf || ReloadPage(p));for(unsigned i=0;i<p->Fill;i++,at++)CHECK(p->PageBuf[i]==bytes[at]);
 }
 CHECK(at==65536);WvFreePageChain(FirstPage);FirstPage=LastPage=NULL;RemoveTmpFile();
 CloseHandle(record.file);free(LineInfo);LineInfo=NULL;TmpDirectory=NULL;
 printf("PASS %u recovery staging/paging/failure checks\n",checks);return 0;
}
