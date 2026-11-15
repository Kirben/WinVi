#include <windows.h>
#include <winnetwk.h>
static DWORD WINAPI SecurityNoNetworkOpen(DWORD a,DWORD b,DWORD c,LPNETRESOURCEW d,LPHANDLE e) {(void)a;(void)b;(void)c;(void)d;(void)e;return ERROR_ACCESS_DENIED;}
#define WNetOpenEnumW SecurityNoNetworkOpen
#include "../PathExp.c"
#undef WNetOpenEnumW
int SecurityUNC(void) {
 WCHAR name[]=L"\\\\server\\share-pattern";FIND_DATA data;
 /* Two leading slashes are needed for a UNC path; EnumShares accepts the split directly. */
 return EnumShares(name,name+8,&data)==INVALID_HANDLE_VALUE ? 0 : 1;
}

