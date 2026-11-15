/* Windows 7 SP1+ integration. GPL-2.0-or-later. */
#define WINVI_PLATFORM_IMPLEMENTATION
#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <string.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include "Platform.h"

/* Use absolute extended paths independently of the machine's long-path policy.
   Every call owns its buffer, including calls made by the file-watch thread. */
static SRWLOCK DirectoryLock=SRWLOCK_INIT;
static WCHAR *LogicalDirectory;
static LPWSTR NativePath(LPCWSTR name)
{
 DWORD n,error;LPWSTR joined=NULL,full=NULL,result=NULL;size_t len,i;
 if(!name){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
 AcquireSRWLockShared(&DirectoryLock);
 if(LogicalDirectory && name[0] && name[0]!=L'\\' && name[0]!=L'/' && name[1]!=L':') {
  len=wcslen(LogicalDirectory)+wcslen(name)+2;
  if(len<WV_PATH_CAP-16){joined=(LPWSTR)malloc(len*sizeof(WCHAR));if(joined){WvCopyString(joined,LogicalDirectory);wcscat(joined,L"\\");wcscat(joined,name);}}
  if(!joined){ReleaseSRWLockShared(&DirectoryLock);SetLastError(ERROR_FILENAME_EXCED_RANGE);return NULL;}
 }
 ReleaseSRWLockShared(&DirectoryLock);
 if(joined)name=joined;
 len=wcslen(name);
 if(len>=WV_PATH_CAP-16){SetLastError(ERROR_FILENAME_EXCED_RANGE);goto done;}
 if(!wcsncmp(name,L"\\\\?\\",4) || !wcsncmp(name,L"\\\\.\\",4)) {
  result=(LPWSTR)malloc((len+1)*sizeof(WCHAR));if(result)WvCopyString(result,name);else SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;
 }
 n=GetFullPathNameW(name,0,NULL,NULL);
 if(!n || n>=WV_PATH_CAP-16){SetLastError(ERROR_FILENAME_EXCED_RANGE);goto done;}
 full=(LPWSTR)malloc(n*sizeof(WCHAR));result=(LPWSTR)malloc((n+8)*sizeof(WCHAR));
 if(!full || !result){free(result);result=NULL;SetLastError(ERROR_NOT_ENOUGH_MEMORY);goto done;}
 error=GetFullPathNameW(name,n,full,NULL);if(!error || error>=n){free(result);result=NULL;if(error>=n)SetLastError(ERROR_FILENAME_EXCED_RANGE);goto done;}
 for(i=0;full[i];i++)if(full[i]==L'/')full[i]=L'\\';
 if(!wcsncmp(full,L"\\\\?\\",4) || !wcsncmp(full,L"\\\\.\\",4))WvCopyString(result,full);
 else if(full[0]==L'\\' && full[1]==L'\\'){WvCopyString(result,L"\\\\?\\UNC\\");wcscat(result,full+2);}
 else{WvCopyString(result,L"\\\\?\\");wcscat(result,full);}
 done:error=GetLastError();free(full);free(joined);SetLastError(error);return result;
}
LPWSTR WvAbsolutePath(LPCWSTR name){return NativePath(name);}
HANDLE WvCreateFileW(LPCWSTR n,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD c,DWORD f,HANDLE t) {
 LPWSTR p=NativePath(n);HANDLE h;DWORD e;if(!p)return INVALID_HANDLE_VALUE;h=CreateFileW(p,a,s,sa,c,f,t);e=GetLastError();free(p);SetLastError(e);return h;
}
BOOL WvDeleteFileW(LPCWSTR n) {LPWSTR p=NativePath(n);BOOL r;DWORD e;if(!p)return FALSE;r=DeleteFileW(p);e=GetLastError();free(p);SetLastError(e);return r;}
DWORD WvGetFileAttributesW(LPCWSTR n) {LPWSTR p=NativePath(n);DWORD r,e;if(!p)return INVALID_FILE_ATTRIBUTES;r=GetFileAttributesW(p);e=GetLastError();free(p);SetLastError(e);return r;}
BOOL WvGetFileAttributesExW(LPCWSTR n,GET_FILEEX_INFO_LEVELS l,LPVOID o) {LPWSTR p=NativePath(n);BOOL r;DWORD e;if(!p)return FALSE;r=GetFileAttributesExW(p,l,o);e=GetLastError();free(p);SetLastError(e);return r;}
BOOL WvSetFileAttributesW(LPCWSTR n,DWORD a) {LPWSTR p=NativePath(n);BOOL r;DWORD e;if(!p)return FALSE;r=SetFileAttributesW(p,a);e=GetLastError();free(p);SetLastError(e);return r;}
HANDLE WvFindFirstFileW(LPCWSTR n,LPWIN32_FIND_DATAW d) {LPWSTR p=NativePath(n);HANDLE r;DWORD e;if(!p)return INVALID_HANDLE_VALUE;r=FindFirstFileW(p,d);e=GetLastError();free(p);SetLastError(e);return r;}
BOOL WvSetCurrentDirectoryW(LPCWSTR n)
{
 LPWSTR p=NativePath(n),copy;DWORD attr,error;BOOL nativeResult;
 if(!p)return FALSE;
 attr=GetFileAttributesW(p);
 if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY)){free(p);SetLastError(ERROR_PATH_NOT_FOUND);return FALSE;}
 copy=(LPWSTR)malloc((wcslen(p)+1)*sizeof(WCHAR));if(!copy){free(p);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 if(!wcsncmp(p,L"\\\\?\\UNC\\",8)){WvCopyString(copy,L"\\\\");wcscat(copy,p+8);}else WvCopyString(copy,!wcsncmp(p,L"\\\\?\\",4)?p+4:p);
 nativeResult=SetCurrentDirectoryW(p);error=GetLastError();free(p);
 if(!nativeResult && error!=ERROR_FILENAME_EXCED_RANGE){free(copy);SetLastError(error);return FALSE;}
 /* Windows' process CWD can still reject extended paths with system policy
    disabled. The editor keeps its own directory so relative I/O remains valid. */
 AcquireSRWLockExclusive(&DirectoryLock);free(LogicalDirectory);LogicalDirectory=copy;ReleaseSRWLockExclusive(&DirectoryLock);
 return TRUE;
}
DWORD WvGetCurrentDirectoryW(DWORD cap,LPWSTR out)
{
 DWORD n;AcquireSRWLockShared(&DirectoryLock);
 if(!LogicalDirectory){ReleaseSRWLockShared(&DirectoryLock);return GetCurrentDirectoryW(cap,out);}
 n=(DWORD)wcslen(LogicalDirectory);if(cap>n && out)WvCopyString(out,LogicalDirectory);else n++;
 ReleaseSRWLockShared(&DirectoryLock);return n;
}
BOOL WvCreateProcessW(LPCWSTR app,LPWSTR cmd,LPSECURITY_ATTRIBUTES processSecurity,
 LPSECURITY_ATTRIBUTES threadSecurity,BOOL inherit,DWORD flags,LPVOID environment,
 LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION process)
{
 LPWSTR logical=NULL;BOOL result;DWORD error;
 if(!directory) {
  AcquireSRWLockShared(&DirectoryLock);
  if(LogicalDirectory){logical=(LPWSTR)malloc((wcslen(LogicalDirectory)+1)*sizeof(WCHAR));if(logical)WvCopyString(logical,LogicalDirectory);else{ReleaseSRWLockShared(&DirectoryLock);SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}}
  ReleaseSRWLockShared(&DirectoryLock);directory=logical;
 }
 result=CreateProcessW(app,cmd,processSecurity,threadSecurity,inherit,flags,environment,directory,startup,process);
 error=GetLastError();free(logical);SetLastError(error);return result;
}
BOOL WvResolveShellExecutable(LPCSTR configured,LPWSTR out,DWORD cap)
{
 WCHAR name[WV_PATH_CAP],system[WV_PATH_CAP],*path=NULL,*part,*next;DWORD n;size_t len;
 if(!configured || !MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,configured,-1,name,WV_PATH_CAP))return FALSE;
 len=wcslen(name);if(len>=2 && name[0]==L'"' && name[len-1]==L'"'){memmove(name,name+1,(len-2)*sizeof(WCHAR));name[len-2]=0;}
 if(!*name || wcschr(name,L'"')){SetLastError(ERROR_INVALID_NAME);return FALSE;}
 n=GetSystemDirectoryW(system,WV_PATH_CAP);if(!n || n>=WV_PATH_CAP)return FALSE;
 if(!_wcsicmp(name,L"cmd") || !_wcsicmp(name,L"cmd.exe")) {
  if(n+9>=cap){SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}
  WvCopyString(out,system);wcscat(out,L"\\cmd.exe");return TRUE;
 }
 if(wcschr(name,L'\\') || wcschr(name,L'/') || wcschr(name,L':')) {
  LPWSTR full=NativePath(name);BOOL ok=FALSE;
  if(!full)return FALSE;
  if(wcslen(full)<cap){WvCopyString(out,full);ok=TRUE;}else SetLastError(ERROR_INSUFFICIENT_BUFFER);
  free(full);return ok;
 }
 /* Configured bare shell names search System32 and absolute PATH entries, never implicit CWD. */
 n=SearchPathW(system,name,L".exe",cap,out,NULL);if(n && n<cap)return TRUE;
 n=GetEnvironmentVariableW(L"PATH",NULL,0);if(!n || n>32768){SetLastError(ERROR_FILE_NOT_FOUND);return FALSE;}
 path=(WCHAR*)malloc(n*sizeof(WCHAR));if(!path){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 {DWORD actual=GetEnvironmentVariableW(L"PATH",path,n);if(!actual || actual>=n){free(path);SetLastError(ERROR_INSUFFICIENT_BUFFER);return FALSE;}}
 part=path;
 while(part) {
  next=wcschr(part,L';');if(next)*next++=0;len=wcslen(part);
  if(len>=2 && part[0]==L'"' && part[len-1]==L'"'){part[len-1]=0;part++;len-=2;}
  if((len>=3 && part[1]==L':' && (part[2]==L'\\'||part[2]==L'/')) || (len>=2 && part[0]==L'\\'&&part[1]==L'\\')) {
   n=SearchPathW(part,name,L".exe",cap,out,NULL);if(n && n<cap){free(path);return TRUE;}
  }
  part=next;
 }
 free(path);SetLastError(ERROR_FILE_NOT_FOUND);return FALSE;
}
/* VerifyVersionInfo compares major/minor/service-pack hierarchically. A newer
   OS does not need Windows 7's service pack, and no update is detected by KB
   inventory: the required API must actually be available and succeed. */
static BOOL WindowsAtLeast(DWORD major,DWORD minor,WORD servicePack)
{
 OSVERSIONINFOEXW version={sizeof(version)};ULONGLONG mask=0;
 version.dwMajorVersion=major;version.dwMinorVersion=minor;version.wServicePackMajor=servicePack;
 mask=VerSetConditionMask(mask,VER_MAJORVERSION,VER_GREATER_EQUAL);
 mask=VerSetConditionMask(mask,VER_MINORVERSION,VER_GREATER_EQUAL);
 mask=VerSetConditionMask(mask,VER_SERVICEPACKMAJOR,VER_GREATER_EQUAL);
 return VerifyVersionInfoW(&version,VER_MAJORVERSION|VER_MINORVERSION|VER_SERVICEPACKMAJOR,mask);
}
BOOL InitializeWindowsPlatform(void)
{
 typedef BOOL (WINAPI *SET_CONTEXT)(HANDLE);
 typedef BOOL (WINAPI *SET_DIRECTORIES)(DWORD);
 SET_CONTEXT setContext;SET_DIRECTORIES setDirectories;
 if(!WindowsAtLeast(6,1,1))return FALSE;
 setDirectories=(SET_DIRECTORIES)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"SetDefaultDllDirectories");
 if(!setDirectories){SetLastError(ERROR_PROC_NOT_FOUND);return FALSE;}
 if(!setDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS))return FALSE;
 setContext=(SET_CONTEXT)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext");
 /* Enhanced GDI scaling on newer systems; system DPI awareness on Windows 7/8. */
 if(!setContext || !setContext((HANDLE)(INT_PTR)-5))SetProcessDPIAware();
 return TRUE;
}

/* Return a suspended child contained in the caller's kill-on-close job.
   On failure, any remaining process handles belong to the caller for cleanup.
   Windows 7 cannot nest jobs: retry only a denied assignment in an inherited
   job, using the launcher's explicit permission to break away. Never resume
   an uncontained child or retry unrelated failures. */
BOOL WvCreateJobProcessW(LPCWSTR app,LPWSTR command,LPSTARTUPINFOW startup,
 HANDLE job,LPPROCESS_INFORMATION process)
{
 DWORD flags=CREATE_SUSPENDED|CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT;
 DWORD error;BOOL inJob=FALSE;LPWSTR retryLine;size_t length;
 length=wcslen(command)+1;retryLine=malloc(length*sizeof(WCHAR));
 if(!retryLine){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return FALSE;}
 memcpy(retryLine,command,length*sizeof(WCHAR));
 if(!WvCreateProcessW(app,command,NULL,NULL,TRUE,flags,NULL,NULL,startup,process)){error=GetLastError();free(retryLine);SetLastError(error);return FALSE;}
 if(AssignProcessToJobObject(job,process->hProcess)){free(retryLine);return TRUE;}
 error=GetLastError();
 if(error!=ERROR_ACCESS_DENIED || WindowsAtLeast(6,2,0) || GetLastError()!=ERROR_OLD_WIN_VERSION ||
    !IsProcessInJob(GetCurrentProcess(),NULL,&inJob) || !inJob){free(retryLine);SetLastError(error);return FALSE;}
 if(!TerminateProcess(process->hProcess,error)){error=GetLastError();free(retryLine);SetLastError(error);return FALSE;}
 CloseHandle(process->hThread);CloseHandle(process->hProcess);ZeroMemory(process,sizeof(*process));
 if(!WvCreateProcessW(app,retryLine,NULL,NULL,TRUE,flags|CREATE_BREAKAWAY_FROM_JOB,NULL,NULL,startup,process)){
  error=GetLastError();free(retryLine);SetLastError(error);return FALSE;
 }
 free(retryLine);return AssignProcessToJobObject(job,process->hProcess);
}

BOOL WvOpenFileDialog(LPOPENFILENAMEW ofn,BOOL save)
{
 IFileDialog *dialog=NULL;IFileDialogCustomize *custom=NULL;IShellItem *item=NULL;PWSTR path=NULL;HRESULT hr,init;
 COMDLG_FILTERSPEC filters[32];UINT count=0,index=1;LPCWSTR f;FILEOPENDIALOGOPTIONS opts;
 init=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
 if(FAILED(init)) return FALSE;
 hr=CoCreateInstance(save?&CLSID_FileSaveDialog:&CLSID_FileOpenDialog,NULL,CLSCTX_INPROC_SERVER,&IID_IFileDialog,(void**)&dialog);
 if(FAILED(hr))goto done;
 IFileDialog_GetOptions(dialog,&opts);opts|=FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_NOCHANGEDIR;
 if(save)opts|=FOS_OVERWRITEPROMPT;else {opts&=~FOS_FILEMUSTEXIST;if(ofn->Flags&OFN_FILEMUSTEXIST)opts|=FOS_FILEMUSTEXIST;else if(ofn->Flags&OFN_CREATEPROMPT)opts|=FOS_CREATEPROMPT;}
 hr=IFileDialog_SetOptions(dialog,opts);if(FAILED(hr))goto done;
 if(!save && !(ofn->Flags&OFN_HIDEREADONLY) && SUCCEEDED(IFileDialog_QueryInterface(dialog,&IID_IFileDialogCustomize,(void**)&custom))) IFileDialogCustomize_AddCheckButton(custom,10001,L"Open read-only",(ofn->Flags&OFN_READONLY)!=0);
 if(ofn->lpstrTitle)IFileDialog_SetTitle(dialog,ofn->lpstrTitle);
 if(ofn->lpstrDefExt)IFileDialog_SetDefaultExtension(dialog,ofn->lpstrDefExt);
 for(f=ofn->lpstrFilter;f && *f && count<32;) {filters[count].pszName=f;f+=wcslen(f)+1;filters[count].pszSpec=f;f+=wcslen(f)+1;count++;}
 if(count){IFileDialog_SetFileTypes(dialog,count,filters);IFileDialog_SetFileTypeIndex(dialog,ofn->nFilterIndex?ofn->nFilterIndex:1);}
 if(ofn->lpstrInitialDir && *ofn->lpstrInitialDir) {
  LPCWSTR initial=ofn->lpstrInitialDir;LPWSTR native=NULL,shortName=NULL;
  if(wcslen(initial)>=MAX_PATH){native=NativePath(initial);shortName=(LPWSTR)calloc(WV_PATH_CAP,sizeof(WCHAR));if(native && shortName && GetShortPathNameW(native,shortName,WV_PATH_CAP))initial=shortName;}
  if(SUCCEEDED(SHCreateItemFromParsingName(initial,NULL,&IID_IShellItem,(void**)&item))){IFileDialog_SetDefaultFolder(dialog,item);IFileDialog_SetFolder(dialog,item);IShellItem_Release(item);item=NULL;}
  free(native);free(shortName);
 }
 if(ofn->lpstrFile && *ofn->lpstrFile) {LPCWSTR base=wcsrchr(ofn->lpstrFile,L'\\');IFileDialog_SetFileName(dialog,base?base+1:ofn->lpstrFile);}
 hr=IFileDialog_Show(dialog,ofn->hwndOwner);if(FAILED(hr))goto done;
 hr=IFileDialog_GetResult(dialog,&item);if(FAILED(hr))goto done;
 hr=IShellItem_GetDisplayName(item,SIGDN_FILESYSPATH,&path);if(FAILED(hr))goto done;
 if(wcslen(path)>=ofn->nMaxFile){hr=HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);goto done;}
 WvCopyString(ofn->lpstrFile,path);
 if(ofn->lpstrFileTitle && ofn->nMaxFileTitle){LPCWSTR base=wcsrchr(path,L'\\');lstrcpynW(ofn->lpstrFileTitle,base?base+1:path,ofn->nMaxFileTitle);}
 IFileDialog_GetFileTypeIndex(dialog,&index);ofn->nFilterIndex=index;
 if(custom){BOOL readOnly=FALSE;if(SUCCEEDED(IFileDialogCustomize_GetCheckButtonState(custom,10001,&readOnly))){if(readOnly)ofn->Flags|=OFN_READONLY;else ofn->Flags&=~OFN_READONLY;}}
 done: if(custom)IFileDialogCustomize_Release(custom);if(path)CoTaskMemFree(path);if(item)IShellItem_Release(item);if(dialog)IFileDialog_Release(dialog);CoUninitialize();return SUCCEEDED(hr);
}

BOOL OpenWinViUrlW(LPCWSTR url)
{
 HRESULT init=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
 SHELLEXECUTEINFOW info={sizeof(info)};BOOL result;DWORD error;
 if(FAILED(init) && init!=RPC_E_CHANGED_MODE)return FALSE;
 info.fMask=SEE_MASK_FLAG_NO_UI;info.lpVerb=L"open";info.lpFile=url;info.nShow=SW_SHOWNORMAL;
 result=ShellExecuteExW(&info);error=GetLastError();if(SUCCEEDED(init))CoUninitialize();SetLastError(error);return result;
}
static BOOL ReplaceHelpToken(LPWSTR text,DWORD cap,LPCWSTR token,LPCWSTR value)
{
 WCHAR *p=text;size_t a=wcslen(token),b=wcslen(value);
 while((p=wcsstr(p,token))!=NULL){size_t current=wcslen(text);if(current-a+b>=cap)return FALSE;memmove(p+b,p+a,(wcslen(p+a)+1)*sizeof(WCHAR));memcpy(p,value,b*sizeof(WCHAR));p+=b;}
 return TRUE;
}
BOOL ResolveWinViHelpUrl(LPCSTR configured,LPCWSTR language,LPCWSTR anchor,LPWSTR out,DWORD cap)
{
 WCHAR *path=(WCHAR*)calloc(WV_PATH_CAP,sizeof(WCHAR));DWORD n;WCHAR *slash;BOOL ok=FALSE;
 if(!path)return FALSE;
 /* The default uses the bundled manual; unsupported translations use English. */
 if(!configured || !*configured || !_stricmp(configured,"file://%EXEPATH/WinVi-%LANG.htm#") || !_stricmp(configured,"file://%EXEPATH/WinVi-%LANG.htm") || !_stricmp(configured,"file://c:/Bin/WinViNfo.htm")) {
  n=GetModuleFileNameW(NULL,path,WV_PATH_CAP);if(!n || n>=WV_PATH_CAP)goto done;
  slash=wcsrchr(path,L'\\');if(!slash)goto done;slash[1]=0;
  if(wcslen(path)+32>=WV_PATH_CAP)goto done;
  wcscat(path,!wcscmp(language,L"de")?L"WinVi-de.htm":L"WinVi-en.htm");
  if(WvGetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)goto done;
  n=cap;if(FAILED(UrlCreateFromPathW(path,out,&n,0)))goto done;
  } else {
  if(!MultiByteToWideChar(CP_ACP,0,configured,-1,out,cap))goto done;
  if(wcsstr(out,L"%EXEPATH")) {
   size_t i;n=GetModuleFileNameW(NULL,path,WV_PATH_CAP);if(!n || n>=WV_PATH_CAP)goto done;slash=wcsrchr(path,L'\\');if(!slash)goto done;*slash=0;
   for(i=0;path[i];i++)if(path[i]==L'\\')path[i]=L'/';
   if(!ReplaceHelpToken(out,cap,L"%EXEPATH",path))goto done;
  }
  if(!ReplaceHelpToken(out,cap,L"%LANG",language))goto done;
 }
 {WCHAR *fragment=wcschr(out,L'#');if(fragment && !fragment[1])*fragment=0;}
 if(anchor && *anchor && !wcschr(out,L'#')){size_t len=wcslen(out);if(len+wcslen(anchor)+2>=cap)goto done;wcscat(out,L"#");wcscat(out,anchor);}
 ok=TRUE;
 done:free(path);return ok;
}

