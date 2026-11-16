/* GUI input and document encoding for line-oriented ex input. GPL-2.0-or-later. */
#include <windows.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "winvi.h"
#include "page.h"

static BOOL InputError(LPCWSTR text)
{NewStatus(0,(PWSTR)text,NS_ERROR);return FALSE;}
typedef struct {INT kind;PWSTR text;BOOL busy,cancelled;} EX_INPUT;
static UINT InputScalar(LPCWSTR,UINT,BYTE[8]);
static UINT InputUnits(LPCWSTR text)
{
 if(*text>=0xd800 && *text<=0xdbff)return text[1]>=0xdc00 && text[1]<=0xdfff?2:0;
 return *text>=0xdc00 && *text<=0xdfff?0:1;
}
static BOOL InputRepresentable(HWND window,EX_INPUT *input,LPCWSTR text)
{
 BYTE encoded[8];UINT units,work=0;
 while(*text){
  if(*text==L'\r' || *text==L'\n')text++;
  else{units=InputUnits(text);if(!units || !InputScalar(text,units,encoded))return FALSE;text+=units;}
  if((++work&255)==0){
   MSG message;UINT pending=0;
   while(pending++<32 && PeekMessage(&message,NULL,0,0,PM_REMOVE)){
    if(message.message==WM_QUIT){PostQuitMessage((INT)message.wParam);input->cancelled=TRUE;break;}
    if(!IsDialogMessage(window,&message)){TranslateMessage(&message);DispatchMessage(&message);}
   }
   if(input->cancelled)return FALSE;
  }
 }
 return TRUE;
}
static void InputLayout(HWND window)
{
 RECT r,pad={0,0,8,8},button={0,0,56,16},note;WCHAR text[256];HWND label=GetDlgItem(window,32002);
 HDC dc;HFONT old,font;INT height,top;
 GetClientRect(window,&r);MapDialogRect(window,&pad);MapDialogRect(window,&button);
 GetWindowTextW(label,text,256);note.left=note.top=note.bottom=0;note.right=r.right-2*pad.right;
 dc=GetDC(label);font=(HFONT)SendMessage(label,WM_GETFONT,0,0);old=(HFONT)SelectObject(dc,font);
 DrawTextW(dc,text,-1,&note,DT_CALCRECT|DT_WORDBREAK);SelectObject(dc,old);ReleaseDC(label,dc);
 height=note.bottom>2*pad.bottom?note.bottom:2*pad.bottom;top=pad.bottom+height+pad.bottom/2;
 MoveWindow(label,pad.right,pad.bottom,r.right-2*pad.right,height,TRUE);
 MoveWindow(GetDlgItem(window,32001),pad.right,top,r.right-2*pad.right,r.bottom-top-button.bottom-3*pad.bottom,TRUE);
 MoveWindow(GetDlgItem(window,IDOK),r.right-2*button.right-2*pad.right,r.bottom-button.bottom-pad.bottom,button.right,button.bottom,TRUE);
 MoveWindow(GetDlgItem(window,IDCANCEL),r.right-button.right-pad.right,r.bottom-button.bottom-pad.bottom,button.right,button.bottom,TRUE);
}
static INT_PTR CALLBACK InputProc(HWND window,UINT message,WPARAM w,LPARAM l)
{
 EX_INPUT *input=(EX_INPUT*)GetWindowLongPtrW(window,DWLP_USER);
 switch(message){
 case WM_INITDIALOG:
  input=(EX_INPUT*)l;SetWindowLongPtrW(window,DWLP_USER,(LONG_PTR)input);
  SetWindowTextW(window,input->kind==0?L"WinVi - Append lines":input->kind==1?L"WinVi - Insert lines":L"WinVi - Change lines");
  if(input->kind==2)SetDlgItemTextW(window,32002,L"Enter replacement lines, then choose Apply. Empty input deletes the addressed lines. Cancel makes no changes.");
  SendDlgItemMessageW(window,32001,EM_LIMITTEXT,INT_MAX-1,0);InputLayout(window);
  SetFocus(GetDlgItem(window,32001));return FALSE;
 case WM_SIZE:if(input)InputLayout(window);return TRUE;
 case WM_GETMINMAXINFO:{RECT minimum={0,0,240,160};MapDialogRect(window,&minimum);
  ((MINMAXINFO*)l)->ptMinTrackSize.x=minimum.right+2*GetSystemMetrics(SM_CXFRAME);
  ((MINMAXINFO*)l)->ptMinTrackSize.y=minimum.bottom+GetSystemMetrics(SM_CYCAPTION)+2*GetSystemMetrics(SM_CYFRAME);return TRUE;}
 case WM_COMMAND:
  if(LOWORD(w)==IDCANCEL){if(input->busy)input->cancelled=TRUE;else EndDialog(window,0);return TRUE;}
  if(LOWORD(w)==IDOK){
   HWND edit=GetDlgItem(window,32001);INT n,read;PWSTR text;
   BOOL valid;if(input->busy)return TRUE;
   SetLastError(ERROR_SUCCESS);n=GetWindowTextLengthW(edit);
   if((!n && GetLastError()!=ERROR_SUCCESS) || n<0 || n>=INT_MAX || (size_t)n>((size_t)-1)/sizeof(WCHAR)-1){InputError(L"Cannot determine the input length.");return TRUE;}
   text=malloc(((size_t)n+1)*sizeof(WCHAR));
   if(!text){InputError(L"Not enough memory to collect the entered lines.");return TRUE;}
   SetLastError(ERROR_SUCCESS);read=GetWindowTextW(edit,text,n+1);
   if(read!=n || (!read && GetLastError()!=ERROR_SUCCESS)){free(text);InputError(L"Cannot read the entered lines.");return TRUE;}
   input->busy=TRUE;EnableWindow(edit,FALSE);EnableWindow(GetDlgItem(window,IDOK),FALSE);
   valid=InputRepresentable(window,input,text);
   input->busy=FALSE;EnableWindow(edit,TRUE);EnableWindow(GetDlgItem(window,IDOK),TRUE);
   if(input->cancelled){free(text);EndDialog(window,0);return TRUE;}
   if(!valid){
    free(text);SetDlgItemTextW(window,32002,L"Some characters cannot be represented exactly in this document's encoding. Correct the text or choose Cancel; the document is unchanged.");InputLayout(window);SetFocus(edit);return TRUE;
   }
   input->text=text;EndDialog(window,1);return TRUE;
  }
  break;
 case WM_CLOSE:if(input->busy)input->cancelled=TRUE;else EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
BOOL WvExAskText(INT kind,PWSTR *text)
{
 EX_INPUT input={kind,NULL};INT_PTR result;
 *text=NULL;result=DialogBoxParamW(hInst,MAKEINTRESOURCEW(32000),hwndMain,InputProc,(LPARAM)&input);
 if(result!=1){free(input.text);if(result==-1)InputError(L"Cannot open the line input dialog.");return FALSE;}
 *text=input.text;return TRUE;
}
/* Convert one validated scalar without silently substituting a default/best-fit
   byte in legacy encodings. */
static UINT InputScalar(LPCWSTR text,UINT units,BYTE bytes[8])
{
 extern INT AnsiCodePage,OemCodePage;
 BOOL used=FALSE;INT n;UINT i;
 if(UtfEncoding==16){for(i=0;i<units;i++){bytes[2*i]=(BYTE)(UtfLsbFirst?text[i]:text[i]>>8);bytes[2*i+1]=(BYTE)(UtfLsbFirst?text[i]>>8:text[i]);}return units*2;}
 if(CharSet==CS_EBCDIC){if(units!=1 || *text>255)return 0;bytes[0]=MapAnsiToEbcdic[*text];return MapEbcdicToAnsi[bytes[0]]==*text?1:0;}
 {UINT cp=UtfEncoding==8?CP_UTF8:CharSet==CS_OEM?OemCodePage:AnsiCodePage;
  n=WideCharToMultiByte(cp,cp==CP_UTF8?WC_ERR_INVALID_CHARS:WC_NO_BEST_FIT_CHARS,text,units,(LPSTR)bytes,8,NULL,cp==CP_UTF8?NULL:&used);
 }
 return n>0 && !used?(UINT)n:0;
}
BOOL WvExEncodeLines(LPCWSTR text,BYTE **out,WV_ULONG *length,WV_ULONG *lastStart)
{
 extern INT AnsiCodePage,OemCodePage;
 INT encoding=UtfEncoding,ansi=AnsiCodePage,oem=OemCodePage,newline=DefaultNewLine;UINT charset=CharSet;BOOL little=UtfLsbFirst;
 WV_ULONG size=0,last=0,work=0,capacity=0;BYTE *buffer=NULL,eol[4],encoded[8];UINT eolSize=WvEncodedEol(eol),pass;
 *out=NULL;*length=*lastStart=0;
 for(pass=0;pass<2;pass++){
  LPCWSTR p=text;BOOL lineStart=TRUE,ended=TRUE;size=last=0;
  while(*p){
   UINT n,units=1;BOOL newline=*p==L'\r' || *p==L'\n';WV_ULONG next;
   if(lineStart){last=size;lineStart=FALSE;}
   if(newline){if(*p==L'\r' && p[1]==L'\n')p++;p++;n=eolSize;memcpy(encoded,eol,n);lineStart=ended=TRUE;}
   else{
    units=InputUnits(p);if(!units)goto invalid;
    n=InputScalar(p,units,encoded);if(!n)goto invalid;p+=units;ended=FALSE;
   }
   if(!WvSizeAdd(size,n,&next) || (pass && next>capacity))goto large;
   if(pass)memcpy(buffer+size,encoded,n);size=next;
   if((++work&255)==0 && CheckInterrupt())goto cancelled;
  }
  if(encoding!=UtfEncoding || ansi!=AnsiCodePage || oem!=OemCodePage || newline!=DefaultNewLine || charset!=CharSet || little!=UtfLsbFirst)goto invalid;
  if(!ended){WV_ULONG next;if(!WvSizeAdd(size,eolSize,&next) || (pass && next>capacity))goto large;if(pass)memcpy(buffer+size,eol,eolSize);size=next;}
  if(!pass){capacity=size;if(size){buffer=malloc((size_t)size);if(!buffer)return InputError(L"Not enough memory for the entered lines.");}}
 }
 *out=buffer;*length=size;*lastStart=last;return TRUE;
invalid:free(buffer);return InputError(L"The entered text cannot be represented exactly in this document's encoding.");
large:free(buffer);return InputError(L"The entered lines exceed the document size limit.");
cancelled:free(buffer);return InputError(L"Line input cancelled; document unchanged.");
}
