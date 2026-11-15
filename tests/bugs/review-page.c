#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include "../../Page.c"
int SecurityTemp(void) {
 DWORD flags=0;SECURITY_DESCRIPTOR_CONTROL control;DWORD revision;PSECURITY_DESCRIPTOR sd=NULL;WCHAR first[WV_PATH_CAP],second[WV_PATH_CAP];
 DefaultTmpFlag=FALSE;TmpDirectory=L".";
 if(!CreateTmpFile()){printf("FAIL temp creation %lu\n",GetLastError());return 1;}
 WvCopyString(first,TmpFileName);
 if(!GetHandleInformation(TmpFile,&flags) || (flags&HANDLE_FLAG_INHERIT)){puts("FAIL temp inheritable");return 1;}
 if(GetSecurityInfo(TmpFile,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,NULL,NULL,&sd)!=ERROR_SUCCESS || !GetSecurityDescriptorControl(sd,&control,&revision) || !(control&SE_DACL_PROTECTED)){puts("FAIL private temp DACL");return 1;}
 {PACL acl;BOOL present,defaulted;if(!GetSecurityDescriptorDacl(sd,&present,&acl,&defaulted)||!present||!acl||acl->AceCount!=2){puts("FAIL temp ACL entries");return 1;}}
 {PACL acl;BOOL present,defaulted;DWORD k;BOOL owner=FALSE,system=FALSE;GetSecurityDescriptorDacl(sd,&present,&acl,&defaulted);
  for(k=0;k<acl->AceCount;k++){ACCESS_ALLOWED_ACE *ace;LPWSTR sid=NULL;
   if(!GetAce(acl,k,(LPVOID*)&ace)||ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE||ace->Mask!=FILE_ALL_ACCESS)return 1;
   if(!ConvertSidToStringSidW(&ace->SidStart,&sid))return 1;
   if(!wcscmp(sid,L"S-1-3-4"))owner=TRUE;else if(!wcscmp(sid,L"S-1-5-18"))system=TRUE;else{LocalFree(sid);return 1;}LocalFree(sid);
  }if(!owner||!system)return 1;
 }
 LocalFree(sd);RemoveTmpFile();if(GetFileAttributesW(first)!=INVALID_FILE_ATTRIBUTES){puts("FAIL temp remains after close");return 1;}
 if(!CreateTmpFile())return 1;WvCopyString(second,TmpFileName);RemoveTmpFile();
 if(!wcscmp(first,second)){puts("FAIL repeated temp name");return 1;}
 puts("PASS randomized, private, non-inheritable, delete-on-close paging files");return 0;
}

int SecurityTempCrashChild(void) {
 HANDLE marker;DWORD written;DefaultTmpFlag=FALSE;TmpDirectory=L".";
 if(!CreateTmpFile())return 2;
 marker=CreateFileW(L"security-temp-marker.tmp",GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);
 if(marker==INVALID_HANDLE_VALUE)return 3;
 if(!WriteFile(marker,TmpFileName,(DWORD)((wcslen(TmpFileName)+1)*sizeof(WCHAR)),&written,NULL)){CloseHandle(marker);return 4;}
 CloseHandle(marker);Sleep(INFINITE);return 5;
}
int SecurityTempCrash(void) {
 WCHAR self[WV_PATH_CAP],command[WV_PATH_CAP+64],path[WV_PATH_CAP]={0};STARTUPINFOW si={sizeof(si)};PROCESS_INFORMATION child={0};HANDLE marker=INVALID_HANDLE_VALUE;DWORD bytes=0,i,exitCode=0;BOOL ok=FALSE;
 if(!GetModuleFileNameW(NULL,self,WV_PATH_CAP))return 1;
 _snwprintf(command,_countof(command),L"\"%s\" --temp-crash-child",self);
 if(!CreateProcessW(self,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&child))return 1;
 for(i=0;i<500;i++){marker=CreateFileW(L"security-temp-marker.tmp",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);if(marker!=INVALID_HANDLE_VALUE)break;if(WaitForSingleObject(child.hProcess,10)==WAIT_OBJECT_0)break;}
 if(marker!=INVALID_HANDLE_VALUE){ok=ReadFile(marker,path,sizeof(path)-sizeof(WCHAR),&bytes,NULL)&&bytes>=sizeof(WCHAR);CloseHandle(marker);}
 TerminateProcess(child.hProcess,99);WaitForSingleObject(child.hProcess,5000);GetExitCodeProcess(child.hProcess,&exitCode);CloseHandle(child.hThread);CloseHandle(child.hProcess);DeleteFileW(L"security-temp-marker.tmp");
 if(!ok||exitCode!=99||GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES){puts("FAIL abrupt-exit temp cleanup");return 1;}
 puts("PASS paging file removed after forced child termination");return 0;
}


