#include <windows.h>
#include "../Platform.h"
#include <stdio.h>
#include <wchar.h>
#include <string.h>
static WCHAR status[WV_TEXT_CAP], wideBox[1024];
static char narrowBox[1024];
int WINAPI TestLoadW(HINSTANCE h,UINT id,LPWSTR buf,int n) { (void)h; (void)id; lstrcpynW(buf,L"number=%d text=%s",n);return lstrlenW(buf); }
int WINAPI TestLoadA(HINSTANCE h,UINT id,LPSTR buf,int n) { (void)h; (void)id; lstrcpynA(buf,"number=%d text=%s",n);return lstrlenA(buf); }
int WINAPI TestBoxW(HWND h,LPCWSTR text,LPCWSTR title,UINT flags) { (void)h;(void)title;(void)flags;lstrcpynW(wideBox,text,1024);return IDOK; }
int WINAPI TestBoxA(HWND h,LPCSTR text,LPCSTR title,UINT flags) { (void)h;(void)title;(void)flags;lstrcpynA(narrowBox,text,1024);return IDYES; }
#define LoadStringW TestLoadW
#define LoadStringA TestLoadA
#define MessageBoxW TestBoxW
#define MessageBoxA TestBoxA
#include "../Misc.c"
UINT Language=0;
HINSTANCE hInst=NULL;
HWND hwndErrorParent=NULL;
WCHAR BufferW[WV_TEXT_CAP];
void NewStatus(INT i,PWSTR s,INT mode) { (void)i;(void)mode;lstrcpynW(status,s,WV_TEXT_CAP); }
int main(void) {
 Error(1,42,L"native64");
 if(wcscmp(status,L"number=42 text=native64")) {puts("FAIL Error formatting");return 1;}
 ErrorBox(0,1,73,L"dialog64");
 if(wcscmp(wideBox,L"number=73 text=dialog64")) {puts("FAIL ErrorBox formatting");return 1;}
 if(!Confirm(NULL,1,91,"confirm64") || strcmp(narrowBox,"number=91 text=confirm64")) {puts("FAIL Confirm formatting");return 1;}
  {WCHAR large[1200];char narrow[1200];int i;for(i=0;i<1199;i++){large[i]=L'x';narrow[i]='x';}large[1199]=0;narrow[1199]=0;
 Error(1,12,large);if(wcslen(status)>WV_TEXT_CAP-1){puts("FAIL bounded Error");return 1;}
 ErrorBox(0,1,12,large);if(wcslen(wideBox)>279){puts("FAIL bounded ErrorBox");return 1;}
 Confirm(NULL,1,12,narrow);if(strlen(narrowBox)>255){puts("FAIL bounded Confirm");return 1;}}
 puts("PASS Error, ErrorBox, Confirm x64 variadic formatting and truncation");return 0;
}
char QueryString[150]; DWORD QueryTime; LPSTR HelpURL;
BOOL AllocStringA(LPSTR *p,LPCSTR s) {(void)p;(void)s;return FALSE;}


BOOL OpenWinViUrlW(LPCWSTR p){(void)p;return TRUE;}
BOOL ResolveWinViHelpUrl(LPCSTR c,LPCWSTR l,LPCWSTR a,LPWSTR o,DWORD n){(void)c;(void)l;(void)a;(void)o;(void)n;return FALSE;}

