/* Cancellable, independently owned file-watch operations. GPL-2.0-or-later. */
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>
#include "winvi.h"
extern FILETIME LastTime;
typedef struct {
 volatile LONG References;
 HANDLE Stop,Thread;
 WCHAR *Name;
 FILETIME Expected;
 HWND Window;
 ULONG_PTR Generation;
} WV_WATCH;
static WV_WATCH *CurrentWatch;
static ULONG_PTR WatchGeneration;
static void ReleaseWatch(WV_WATCH *w)
{
 if(InterlockedDecrement(&w->References)==0){if(w->Thread)CloseHandle(w->Thread);CloseHandle(w->Stop);free(w->Name);free(w);}
}
static DWORD WINAPI WatchWorker(LPVOID parameter)
{
 WV_WATCH *w=parameter;WCHAR *directory=_wcsdup(w->Name),*slash;HANDLE change=INVALID_HANDLE_VALUE;BOOL flash=FALSE,initial=TRUE;
 if(!directory)goto done;
 slash=wcsrchr(directory,L'\\');if(!slash)goto done;slash[1]=0;
 if(WaitForSingleObject(w->Stop,0)==WAIT_OBJECT_0)goto done;
 change=FindFirstChangeNotificationW(directory,FALSE,FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_LAST_WRITE);
 if(change==INVALID_HANDLE_VALUE)goto done;
 for(;;){
  HANDLE events[2]={w->Stop,change};HANDLE file=INVALID_HANDLE_VALUE;FILETIME time;BOOL ok;int i;
  if(!initial && WaitForMultipleObjects(2,events,FALSE,INFINITE)!=WAIT_OBJECT_0+1)break;
  initial=FALSE;
  for(i=0;i<3;i++){
   if(WaitForSingleObject(w->Stop,0)==WAIT_OBJECT_0)goto done;
   file=CreateFileW(w->Name,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
   if(file!=INVALID_HANDLE_VALUE)break;
   if(WaitForSingleObject(w->Stop,200)==WAIT_OBJECT_0)goto done;
  }
  if(file!=INVALID_HANDLE_VALUE){
   ok=GetFileTime(file,NULL,NULL,&time);CloseHandle(file);
   if(ok && CompareFileTime(&time,&w->Expected)){
    WCHAR *message;
    while(!CaretActive){
     PostMessage(w->Window,WM_USER+1237,w->Generation,flash=!flash);
     if(WaitForSingleObject(w->Stop,100)==WAIT_OBJECT_0)goto done;
    }
    if(WaitForSingleObject(w->Stop,0)==WAIT_OBJECT_0)goto done;
    message=_wcsdup(wcsrchr(w->Name,L'\\')+1);
    if(message && !PostMessage(w->Window,WM_USER+1236,w->Generation,(LPARAM)message))free(message);
    break;
   }
  }
  if(WaitForSingleObject(w->Stop,200)==WAIT_OBJECT_0 || !FindNextChangeNotification(change))break;
 }
done:
 if(flash)PostMessage(w->Window,WM_USER+1237,w->Generation,FALSE);
 if(change!=INVALID_HANDLE_VALUE)FindCloseChangeNotification(change);
 free(directory);ReleaseWatch(w);return 0;
}
void WvWatchMessage(WPARAM generation,LPARAM payload)
{
 WCHAR *message=(WCHAR*)payload;
 if(message && CurrentWatch && generation==WatchGeneration)WatchMessage(message);
 free(message);
}
BOOL WatchFile(const WCHAR *name)
{
 WV_WATCH *old=CurrentWatch,*w;MSG message;
 CurrentWatch=NULL;++WatchGeneration;
 if(old){
  SetEvent(old->Stop);CancelSynchronousIo(old->Thread);
  /* If a network provider stalls, its private context survives until it returns. */
  WaitForSingleObject(old->Thread,1000);ReleaseWatch(old);FlashWindow(hwndMain,FALSE);
 }
 if(hwndMain)while(PeekMessage(&message,hwndMain,WM_USER+1236,WM_USER+1236,PM_REMOVE))free((void*)message.lParam);
 if(!name || (!LastTime.dwHighDateTime && !LastTime.dwLowDateTime))return TRUE;
 w=calloc(1,sizeof(*w));if(!w)return FALSE;
 w->References=1;w->Name=WvAbsolutePath(name);w->Stop=CreateEvent(NULL,TRUE,FALSE,NULL);
 w->Window=hwndMain;w->Expected=LastTime;w->Generation=WatchGeneration;
 if(!w->Name || !w->Stop){if(w->Stop)CloseHandle(w->Stop);free(w->Name);free(w);return FALSE;}
 InterlockedIncrement(&w->References);
 w->Thread=CreateThread(NULL,0,WatchWorker,w,0,NULL);
 if(!w->Thread){InterlockedDecrement(&w->References);ReleaseWatch(w);return FALSE;}
 CurrentWatch=w;return TRUE;
}


void WvWatchFlash(WPARAM generation,LPARAM state){if(CurrentWatch && generation==WatchGeneration)FlashWindow(hwndMain,(BOOL)state);}
