#include <windows.h>
#include <stdio.h>
#include <wchar.h>
extern PCWSTR ClassName;
static PCWSTR testTitle;
static BOOL WINAPI SecurityEnum(WNDENUMPROC fn,LPARAM p){return fn((HWND)1,p);}
static int WINAPI SecurityClass(HWND h,LPWSTR p,int n){(void)h;lstrcpynW(p,ClassName,n);return lstrlenW(p);}
static int WINAPI SecurityTitle(HWND h,LPWSTR p,int n){(void)h;lstrcpynW(p,testTitle,n);return lstrlenW(p);}
static LRESULT WINAPI SecurityReply(HWND h,UINT m,WPARAM w,LPARAM l,UINT f,UINT t,PDWORD_PTR r){(void)h;(void)m;(void)w;(void)l;(void)f;(void)t;*r=0;return 1;}
#define EnumWindows SecurityEnum
#define GetClassNameW SecurityClass
#define GetWindowTextW SecurityTitle
#undef SendMessageTimeout
#define SendMessageTimeout SecurityReply
#include "../../InterCom.c"
int SecurityPeer(void) {
 WCHAR many[601],out[600];PCWSTR titles[4];HMENU menu;int i,j;
 for(i=0;i<600;i++)many[i]='&';many[600]=0;
 titles[0]=L"";titles[1]=L"x";titles[2]=L"unexpected peer title";titles[3]=many;
 IcMessage=WM_APP+3;
 for(i=0;i<4;i++){testTitle=titles[i];menu=CreatePopupMenu();if(!menu)return 1;
  InterCommMenu(NULL,menu);if(GetMenuItemCount(menu)!=1){DestroyMenu(menu);return 1;}
  j=GetMenuStringW(menu,0,out,600,MF_BYPOSITION);DestroyMenu(menu);
  if(j<4 || j>=520)return 1;
 }
 hwndMain=(HWND)2;if(InterCommActivate(FALSE))return 1;hwndMain=NULL;
 puts("PASS empty, unexpected and ampersand-heavy peer titles and missing window index");return 0;
}

