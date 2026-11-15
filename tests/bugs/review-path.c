#include <windows.h>
#include <winnetwk.h>
static BOOL WINAPI BugTrackPopup(HMENU a,UINT b,int c,int d,int e,HWND f,const RECT *g){return TRUE;}
#define TrackPopupMenu BugTrackPopup
static DWORD WINAPI SecurityNoNetworkOpen(DWORD a,DWORD b,DWORD c,LPNETRESOURCEW d,LPHANDLE e) {(void)a;(void)b;(void)c;(void)d;(void)e;return ERROR_ACCESS_DENIED;}
#define WNetOpenEnumW SecurityNoNetworkOpen
#include "../../PathExp.c"
#undef WNetOpenEnumW
int SecurityUNC(void) {
 WCHAR name[]=L"\\\\server\\share-pattern";FIND_DATA data;
 /* Two leading slashes are needed for a UNC path; EnumShares accepts the split directly. */
 return EnumShares(name,name+8,&data)==INVALID_HANDLE_VALUE ? 0 : 1;
}
int BugWildcard(void) {
 WCHAR pattern[100],name[40];int i;
 for(i=0;i<30;i++){pattern[2*i]=L'*';pattern[2*i+1]=L'a';name[i]=L'a';}
 pattern[60]=L'b';pattern[61]=0;name[30]=L'c';name[31]=0;
 return PatternMatch(name,pattern)?1:0;
}
int BugPopup(void) {
 int i;WCHAR name[20];StartPopupMenu();nMenuHeight=10;nMenuItemHght=20;
 for(i=0;i<8;i++){_snwprintf(name,20,L"item%d",i);AppendMenuItemW(0,name);}
 if(!ShowPopupMenu(0,0))return 1;DestroyMenu(PathExpMenu);PathExpMenu=NULL;return 0;
}
static BOOL GlobOracle(PCWSTR text,PCWSTR pattern) {
 PCWSTR end=pattern;BOOL dp[25][25];size_t n=wcslen(text),m,i,j;
 for(;;){while(*end&&*end!=','&&*end!=';')end++;m=end-pattern;memset(dp,0,sizeof(dp));dp[0][0]=TRUE;
  for(j=1;j<=m;j++)dp[0][j]=pattern[j-1]=='*'&&dp[0][j-1];
  for(i=1;i<=n;i++)for(j=1;j<=m;j++)dp[i][j]=pattern[j-1]=='*'?(dp[i][j-1]||dp[i-1][j]):(dp[i-1][j-1]&&(pattern[j-1]=='?'||CompareStringOrdinal(text+i-1,1,pattern+j-1,1,TRUE)==CSTR_EQUAL));
  if(dp[n][m])return TRUE;if(!*end)return FALSE;pattern=++end;
 }
}
int BugWildcardOracle(void) {
 unsigned seed=0x2a19;int trial,i,n,m;WCHAR text[20],pattern[24];const WCHAR chars[]=L"abA",glob[]=L"abA*?;,";
 for(trial=0;trial<30000;trial++){
  n=trial%16;m=(trial/7)%22;
  for(i=0;i<n;i++){seed=seed*1664525u+1013904223u;text[i]=chars[seed%3];}text[n]=0;
  for(i=0;i<m;i++){seed=seed*1664525u+1013904223u;pattern[i]=glob[seed%7];}pattern[m]=0;
  if(PatternMatch(text,pattern)!=GlobOracle(text,pattern)){printf("FAIL glob trial %d\n",trial);return 1;}
 }
 {WCHAR *longPattern=malloc(32768*sizeof(WCHAR));BOOL ok;if(!longPattern)return 1;for(i=0;i<32767;i++)longPattern[i]='*';longPattern[32767]=0;ok=PatternMatch(L"",longPattern);free(longPattern);if(!ok)return 1;}
 puts("PASS 30000 wildcard/oracle comparisons and 32767 stars");return 0;
}


