/* Ex preservation and staged buffer recovery. GPL-2.0-or-later. */
#include <windows.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"
#include "map.h"
#include "Recovery.h"
#include "exec.h"
extern HWND hwndSearch,hwndSettings;
extern FILETIME LastTime;
extern BOOL NotEdited;
static BOOL RecoveryError(PCWSTR message)
{NewStatus(0,(PWSTR)message,NS_ERROR);return FALSE;}
static PWSTR RecoveryDirectory(void)
{
 PWSTR base=NULL,root=NULL;WCHAR local[MAX_PATH];size_t n;
 if(!DefaultTmpFlag && TmpDirectory && *TmpDirectory)base=WvAbsolutePath(TmpDirectory);
 else if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,local)))base=WvAbsolutePath(local);
 if(!base)return NULL;n=wcslen(base);
 if(n<WV_PATH_CAP-64){root=calloc(WV_PATH_CAP,sizeof(WCHAR));if(root){wcscpy(root,base);wcscat(root,L"\\WinVi-recovery");}}
 free(base);return root;
}
static PWSTR RecoveryName(PWSTR args)
{
 size_t n;PWSTR raw,name;
 while(*args==L' ' || *args==L'\t')args++;n=wcslen(args);
 while(n && (args[n-1]==L' ' || args[n-1]==L'\t'))n--;
 if(n>=2 && args[0]==L'"' && args[n-1]==L'"'){args++;n-=2;}
 if(!n){if(CurrFile && *CurrFile)return WvAbsolutePath(CurrFile);return calloc(1,sizeof(WCHAR));}
 raw=malloc((n+1)*sizeof(WCHAR));if(!raw)return NULL;
 memcpy(raw,args,n*sizeof(WCHAR));raw[n]=0;name=WvAbsolutePath(raw);free(raw);return name;
}
typedef struct {HWND windows[2];BOOL enabled[2];} FREEZE;
static void BeginRecovery(FREEZE *freeze)
{
 freeze->windows[0]=hwndSearch;freeze->windows[1]=hwndSettings;
 for(int i=0;i<2;i++)if(freeze->windows[i] && IsWindowEnabled(freeze->windows[i])){
  freeze->enabled[i]=TRUE;EnableWindow(freeze->windows[i],FALSE);
 }
 WvExGlobalActive=TRUE;StartInterruptCheck();Disable(1);
}
static void EndRecovery(FREEZE *freeze)
{
 WvExGlobalActive=FALSE;Enable();
 for(int i=0;i<2;i++)if(freeze->enabled[i] && IsWindow(freeze->windows[i]))EnableWindow(freeze->windows[i],TRUE);
}
static BOOL RecoveryCancelled(void *unused)
{(void)unused;return CheckInterrupt();}
static BOOL DocumentRead(void *context,BYTE *buffer,DWORD capacity,DWORD *got)
{
 POSITION *p=context;*got=0;
 while(*got<capacity && p->p){
  if(p->i>p->p->Fill){SetLastError(ERROR_INVALID_DATA);return FALSE;}
  DWORD n=p->p->Fill-p->i;if(n>capacity-*got)n=capacity-*got;
  if(n){
   if(!p->p->PageBuf && !ReloadPage(p->p)){SetLastError(ERROR_READ_FAULT);return FALSE;}
   memcpy(buffer+*got,p->p->PageBuf+p->i,n);p->i+=n;*got+=n;
  }
  if(p->i==p->p->Fill){p->p=p->p->Next;p->i=0;}
 }
 return TRUE;
}
BOOL WvExPreserve(void)
{
 PWSTR root=NULL,name=NULL;WV_RECOVERY_META meta={0};POSITION position={FirstPage,0,0};
 WV_ULONG length=0;FREEZE freeze={0};BOOL ok=FALSE;
 if(WvExGlobalActive || ExecRunning())return RecoveryError(L"Preserve requires no active global/input or external command.");
 if(!WvDocumentComplete())return RecoveryError(L"Cannot preserve a document that was not fully loaded.");
 if(!(root=RecoveryDirectory()) || !(name=RecoveryName(L"")))goto done;
 for(LPPAGE page=FirstPage;page;page=page->Next)if(!WvSizeAdd(length,page->Fill,&length))goto done;
 meta.encoding=UtfEncoding;meta.characterSet=CharSet;meta.littleEndian=UtfLsbFirst;
 meta.newLine=DefaultNewLine;meta.cursor=CountBytes(&CurrPos);meta.sourceTime=LastTime;
 meta.newFile=NotEdited;
 BeginRecovery(&freeze);
 ok=WvRecoverySave(root,name,&meta,length,DocumentRead,&position,RecoveryCancelled,NULL,NULL);
 EndRecovery(&freeze);
done:free(root);free(name);
 if(!ok)return RecoveryError(Interrupted?L"Preserve cancelled; earlier snapshots retained.":L"Cannot preserve the document; earlier snapshots retained.");
 NewStatus(0,L"Document preserved. The original file and modified state are unchanged.",NS_NORMAL);return TRUE;
}
static LPPAGE StageRecovery(WV_RECOVERY *record,LPPAGE *last,WV_ULONG *lines)
{
 LPPAGE first=NULL,page=NULL;WV_ULONG remaining=record->length,number=0;DWORD flags=0,got;
 *last=NULL;*lines=0;
 do{
  if(CheckInterrupt())goto fail;
  LPPAGE next=NewPage();if(!next)goto fail;
  next->Prev=page;if(page)page->Next=next;else first=next;page=next;page->PageNo=++number;
  page->Fill=remaining>PAGESIZE?PAGESIZE:(UINT)remaining;
  DWORD at=0;
  while(at<page->Fill){if(!ReadFile(record->file,page->PageBuf+at,page->Fill-at,&got,NULL) || !got)goto fail;at+=got;}
  INT oldUtf=UtfEncoding;BOOL oldLittle=UtfLsbFirst;UINT oldSet=CharSet;
  UtfEncoding=record->meta.encoding;UtfLsbFirst=record->meta.littleEndian;CharSet=record->meta.characterSet;
  INT count=CountNewlinesInBuf(page->PageBuf,page->Fill,NULL,&flags);
  UtfEncoding=oldUtf;UtfLsbFirst=oldLittle;CharSet=oldSet;
  if(count<0 || !WvSizeAdd(*lines,count,lines))goto fail;
  page->Newlines=*lines;remaining-=page->Fill;
 }while(remaining);
 *last=page;return first;
fail:WvFreePageChain(first);return NULL;
}
BOOL WvExRecover(PWSTR args,BOOL force)
{
 PWSTR root=NULL,name=NULL;WV_RECOVERY record={0};FREEZE freeze={0};LPPAGE first=NULL,last=NULL;
 WV_ULONG lines=0;BOOL ok=FALSE;
 if(WvExGlobalActive || ExecRunning())return RecoveryError(L"Recover requires no active global/input or external command.");
 if(Unsafe && !force)return RecoveryError(L"Unsaved changes: use recover! to replace the current buffer.");
 if(IsViewOnly())return FALSE;
 if(!(root=RecoveryDirectory()) || !(name=RecoveryName(args)))goto done;
 BeginRecovery(&freeze);
 if(!WvRecoveryFind(root,name,&record,RecoveryCancelled,NULL))goto thaw;
 first=StageRecovery(&record,&last,&lines);if(!first || CheckInterrupt())goto thaw;
 /* The old document remains intact until every recovered page is available.
    Adoption and undo reset require no further allocations. */
 StopMap(TRUE);DisableUndo();WvReplacePageChain(first,last);first=NULL;LinesInFile=lines;
 SetCharSet(record.meta.characterSet,2|8);
 UtfEncoding=record.meta.encoding;UtfLsbFirst=record.meta.littleEndian;DefaultNewLine=record.meta.newLine;
 WvRecoveredFileName(record.original,&record.meta.sourceTime,record.meta.newFile);SetUnsafe();
 CurrPos.p=FirstPage;CurrPos.i=CurrPos.f=0;Advance(&CurrPos,record.meta.cursor);
 FindValidPosition(&CurrPos,0);NewPosition(&CurrPos);InvalidateText();NewScrollPos();ok=TRUE;
thaw:if(first)WvFreePageChain(first);EndRecovery(&freeze);
done:WvRecoveryClose(&record);free(root);free(name);
 if(!ok)return RecoveryError(Interrupted?L"Recover cancelled; the current buffer is unchanged.":L"No intact matching snapshot could be recovered; the current buffer is unchanged.");
 NewStatus(0,L"Recovered the latest valid preserved snapshot. Use write to save the recovered text.",NS_NORMAL);return TRUE;
}
