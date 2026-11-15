#define _WIN32_WINNT 0x0A00
#define _WIN32_IE 0x0A00
#include "../Platform.c"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s (error %lu)\n",__LINE__,#x,GetLastError());return 1;}}while(0)
int main(void) {
 WCHAR url[WV_TEXT_CAP*3],cwd[WV_PATH_CAP],longDir[WV_PATH_CAP],file[WV_PATH_CAP];HANDLE h;int i;DWORD bytes;char read[5]={0};WIN32_FILE_ATTRIBUTE_DATA data;
 CHECK(ResolveWinViHelpUrl("file://%EXEPATH/WinVi-%LANG.htm#",L"en",L"EditTextmode",url,WV_TEXT_CAP*3));
 CHECK(wcsstr(url,L"WinVi-en.htm#EditTextmode")!=NULL);
 CHECK(ResolveWinViHelpUrl(NULL,L"fr",L"",url,WV_TEXT_CAP*3));CHECK(wcsstr(url,L"WinVi-en.htm")!=NULL);
 CHECK(ResolveWinViHelpUrl(NULL,L"de",L"",url,WV_TEXT_CAP*3));CHECK(wcsstr(url,L"WinVi-de.htm")!=NULL);
 CHECK(ResolveWinViHelpUrl("https://www.winvi.de/en/",L"en",L"Example",url,WV_TEXT_CAP*3));CHECK(!wcscmp(url,L"https://www.winvi.de/en/#Example"));
 CHECK(GetCurrentDirectoryW(WV_PATH_CAP,cwd));wcscpy(longDir,cwd);wcscat(longDir,L"\\native-long-path");CreateDirectoryW(longDir,NULL);
 for(i=0;i<9;i++){LPWSTR native;wcscat(longDir,L"\\abcdefghijklmnopqrstuvwxyz0123456789abcdef");native=NativePath(longDir);CHECK(native);CHECK(CreateDirectoryW(native,NULL) || GetLastError()==ERROR_ALREADY_EXISTS);free(native);}
 wcscpy(file,longDir);wcscat(file,L"\\native-test.txt");CHECK(wcslen(file)>260);
 h=WvCreateFileW(file,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);CHECK(h!=INVALID_HANDLE_VALUE);
 CHECK(WriteFile(h,"test",4,&bytes,NULL) && bytes==4);CloseHandle(h);
 CHECK(WvGetFileAttributesExW(file,GetFileExInfoStandard,&data));CHECK(data.nFileSizeLow==4);
 CHECK(WvSetCurrentDirectoryW(longDir));h=WvCreateFileW(L"native-test.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(h!=INVALID_HANDLE_VALUE);CHECK(ReadFile(h,read,4,&bytes,NULL) && !strcmp(read,"test"));CloseHandle(h);
 CHECK(WvSetCurrentDirectoryW(cwd));CHECK(WvDeleteFileW(file));
 puts("PASS bundled help resolution, language fallback, URLs, long and relative native paths");return 0;
}
