#include <windows.h>
int wmain(void){
 WCHAR path[32768];DWORD n,written;HANDLE f;
 n=GetEnvironmentVariableW(L"WINVI_TEST_SHELL_RECORD",path,32768);if(!n || n>=32768)return 2;
 f=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 3;
 n=(DWORD)lstrlenW(GetCommandLineW())*sizeof(WCHAR);
 if(!WriteFile(f,GetCommandLineW(),n,&written,NULL) || written!=n){CloseHandle(f);return 4;}
 CloseHandle(f);return 0;
}
