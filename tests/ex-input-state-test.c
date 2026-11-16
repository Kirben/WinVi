/* Production command-bar acceptance logic with dummy controls. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static int checks,failAlloc,allocations;
static WCHAR pending[8192],title[256],clipboard[8192];
static void *Allocate(size_t n){return ++allocations==failAlloc?NULL:malloc(n);}
static void *Reallocate(void *p,size_t n){return ++allocations==failAlloc?NULL:realloc(p,n);}
static int WINAPI TextLength(HWND h){(void)h;return (int)wcslen(pending);}
static int WINAPI ReadText(HWND h,LPWSTR text,int n){(void)h;wcsncpy(text,pending,n);return (int)wcslen(text);}
static BOOL WINAPI WriteText(HWND h,LPCWSTR text);
static BOOL WINAPI WindowEnable(HWND h,BOOL on){(void)h;(void)on;return TRUE;}
static HWND WINAPI WindowFocus(HWND h){return h;}
static BOOL WINAPI ClipOpen(HWND h){(void)h;return TRUE;}
static BOOL WINAPI ClipClose(void){return TRUE;}
static HANDLE WINAPI ClipData(UINT format){(void)format;return (HANDLE)4;}
static SIZE_T WINAPI ClipSize(HGLOBAL h){(void)h;return sizeof(clipboard);}
static LPVOID WINAPI ClipLock(HGLOBAL h){(void)h;return clipboard;}
static BOOL WINAPI ClipUnlock(HGLOBAL h){(void)h;return TRUE;}
static LRESULT WINAPI EditMessage(HWND h,UINT message,WPARAM w,LPARAM l){(void)h;(void)w;if(message==EM_REPLACESEL)wcscat(pending,(LPCWSTR)l);return 0;}
#define OpenClipboard ClipOpen
#define CloseClipboard ClipClose
#define GetClipboardData ClipData
#define GlobalSize ClipSize
#define GlobalLock ClipLock
#define GlobalUnlock ClipUnlock
#define SendMessageW EditMessage
#define GetWindowTextLengthW TextLength
#define GetWindowTextW ReadText
#define SetWindowTextW WriteText
#define EnableWindow WindowEnable
#define SetFocus WindowFocus
#define malloc Allocate
#define realloc Reallocate
#include "../ExInput.c"
#undef malloc
#undef realloc
static BOOL WINAPI WriteText(HWND h,LPCWSTR text){wcscpy(h==hwndMain?title:pending,text);return TRUE;}
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void Reset(EX_INPUT *input){
 memset(input,0,sizeof(*input));input->text=calloc(1,sizeof(WCHAR));input->capacity=1;ActiveInput=input;
 hwndMain=(HWND)1;hwndCmd=(HWND)2;pending[0]=title[0]=0;failAlloc=allocations=0;
 UtfEncoding=8;CharSet=CS_UTF8;Interrupted=FALSE;
}
int main(void){
 EX_INPUT input;int k;
 Reset(&input);CHECK(WvExInputDone(FALSE));CHECK(!wcscmp(input.text,L"\r\n") && input.lines==1 && !input.done);
 wcscpy(pending,L".");CHECK(WvExInputDone(FALSE));CHECK(input.done && input.lines==1);free(input.text);
 Reset(&input);wcscpy(pending,L"..");CHECK(WvExInputDone(FALSE));wcscpy(pending,L" .");CHECK(WvExInputDone(FALSE));
 wcscpy(pending,L"unfinished");CHECK(WvExInputDone(TRUE));CHECK(input.done && input.lines==2 && !wcscmp(input.text,L"..\r\n .\r\n"));free(input.text);
 for(k=1;k<=2;k++){
  Reset(&input);wcscpy(pending,L"accepted");CHECK(WvExInputDone(FALSE));
  input.capacity=input.used+1;allocations=0;failAlloc=k;wcscpy(pending,L"pending");CHECK(WvExInputDone(FALSE));
  CHECK(!input.done && input.lines==1 && !wcscmp(pending,L"pending") && !wcscmp(input.text,L"accepted\r\n"));
  CHECK(WvExInputDone(TRUE));CHECK(input.done && input.lines==1);free(input.text);
 }
 Reset(&input);wcscpy(pending,L"\xd800");CHECK(WvExInputDone(FALSE));CHECK(!input.used && !input.done && pending[0]==0xd800);
 wcscpy(pending,L"\xdc00");CHECK(WvExInputDone(FALSE));CHECK(!input.used && !input.done);
 wcscpy(pending,L"\xd83d\xde00");CHECK(WvExInputDone(FALSE));CHECK(!wcscmp(input.text,L"\xd83d\xde00\r\n"));free(input.text);
 Reset(&input);wcscpy(pending,L"no\nembedded line");CHECK(WvExInputDone(FALSE));CHECK(!input.used && !input.done);free(input.text);
 Reset(&input);for(k=0;k<1000;k++){swprintf(pending,8192,L"line %d",k);CHECK(WvExInputDone(FALSE));CHECK(!input.done && !pending[0] && input.lines==(WV_ULONG)k+1);}
 CHECK(wcsstr(input.text,L"line 999\r\n")!=NULL);CHECK(WvExInputDone(TRUE));CHECK(input.lines==1000);free(input.text);
 Reset(&input);input.busy=TRUE;wcscpy(pending,L"nested");CHECK(WvExInputDone(FALSE));CHECK(!input.used);CHECK(WvExInputDone(TRUE));CHECK(input.done);free(input.text);
 Reset(&input);wcscpy(clipboard,L"a\r\nb\nc\r.\r\nignored");InputPaste(hwndCmd);
 CHECK(input.done && input.lines==3 && !wcscmp(input.text,L"a\r\nb\r\nc\r\n"));free(input.text);
 Reset(&input);wcscpy(pending,L"prefix");wcscpy(clipboard,L".\n\nunfinished");InputPaste(hwndCmd);
 CHECK(!input.done && input.lines==2 && !wcscmp(input.text,L"prefix.\r\n\r\n") && !wcscmp(pending,L"unfinished"));
 CHECK(WvExInputDone(TRUE));CHECK(input.lines==2);free(input.text);
 Reset(&input);wcscpy(clipboard,L"ok\n\xd800\nignored\n");InputPaste(hwndCmd);
 CHECK(!input.done && input.lines==1 && pending[0]==0xd800 && !wcscmp(input.text,L"ok\r\n"));free(input.text);
 Reset(&input);wcscpy(clipboard,L"one\ntwo\n");failAlloc=1;InputPaste(hwndCmd);CHECK(!input.used && !pending[0]);free(input.text);
 Reset(&input);input.used=SIZE_MAX/sizeof(WCHAR)-1;wcscpy(pending,L"X");CHECK(WvExInputDone(FALSE));CHECK(!input.done && !wcscmp(pending,L"X"));free(input.text);
 Reset(&input);{MSG message={0};message.hwnd=hwndMain;message.message=WM_CHAR;message.wParam=27;InputMessage(&message);CHECK(input.done);}free(input.text);
 ActiveInput=NULL;CHECK(!WvExInputDone(FALSE));CHECK(!WvExInputDone(TRUE));
 printf("PASS %d ex input acceptance/failure checks\n",checks);return 0;
}
