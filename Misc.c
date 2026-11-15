/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2007 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *     11-Jan-2003	extracted from paint.c
 *     12-Jan-2003	added FormatShell and Help
 */

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdarg.h>
#include <stdio.h>
#include "winvi.h"

static WCHAR ErrBuf1[200], ErrBuf2[280];
extern WCHAR BufferW[WV_TEXT_CAP];

void Error(INT StringNo, ...)
{
    va_list args;
	LOADSTRINGW(hInst, StringNo, ErrBuf1, WSIZE(ErrBuf1));
	if (!*ErrBuf1) return;
	va_start(args, StringNo);
    _vsnwprintf(BufferW, WSIZE(BufferW)-1, ErrBuf1, args);
    va_end(args);
    BufferW[WSIZE(BufferW)-1] = L'\0';
	NewStatus(0, BufferW, NS_ERROR);
}

WCHAR ErrorTitle[16] = {'\0'};

INT ErrorBox(INT Flags, INT StringNo, ...)
{
    va_list args;
	LOADSTRINGW(hInst, StringNo, ErrBuf1, WSIZE(ErrBuf1));
	if (!*ErrBuf1 /*error string not defined in resource string table*/)
		return(IDCANCEL);
	if (!*ErrorTitle) LOADSTRINGW(hInst, 314, ErrorTitle, WSIZE(ErrorTitle));
	va_start(args, StringNo);
    _vsnwprintf(ErrBuf2, WSIZE(ErrBuf2)-1, ErrBuf1, args);
    va_end(args);
    ErrBuf2[WSIZE(ErrBuf2)-1] = L'\0';
	return (MessageBoxW(hwndErrorParent, ErrBuf2,
			Flags & MB_ICONINFORMATION ? L"WinVi" : ErrorTitle, MB_OK | Flags));
}

BOOL Confirm(HWND hParent, INT nQuestion, ...)
{
    va_list args;	CHAR Title[20];
	CHAR Format[80];
	CHAR Question[256];
	INT	 Answer;

	LOADSTRING(hInst, 341,		 Title,  sizeof(Title));
	LOADSTRING(hInst, nQuestion, Format, sizeof(Format));
	va_start(args, nQuestion);
    _vsnprintf(Question, sizeof(Question)-1, Format, args);
    va_end(args);
    Question[sizeof(Question)-1] = '\0';
	Answer = MessageBox(hParent, Question, Title, MB_YESNO | MB_ICONQUESTION);
	return (Answer == IDYES);
}

INT FormatShell(PSTR Result, LPSTR FormatString, LPSTR Cmd0, PSTR Cmd1)
{	PSTR p;
	INT   Flags = 0, Length = 0;
	BOOL  Cmd1Done = FALSE;

	for (;;) {
		switch (*FormatString++) {
			case '\0':
				if (!Cmd1Done) {
					if (Result != NULL) {
						*Result++ = ' ';
						strcpy(Result, Cmd1);
					}
					Length += strlen(Cmd1) + 1;
				} else if (Result != NULL) *Result = '\0';
				return (Length);

			case '%':
				switch (*FormatString++) {
					case '0':	p = Cmd0;
								break;

					case '1':	p = Cmd1;
								Cmd1Done = TRUE;
								break;

					case '%':	++FormatString;
								/*FALLTHROUGH*/
					default:	--FormatString;
								if (Result != NULL) *Result++ = '%';
								++Length;
								continue;
				}
				while (*p != '\0') {
					if (   (*p == '\'' && Flags & 1)
						|| (*p == '"'  && Flags & 2)
						|| (*p == '\\' && Flags)) {
							if (Result != NULL) *Result++ = '\\';
							++Length;
					}
					if (Result != NULL) *Result++ = *p;
					++Length;
					++p;
				}
				break;

			case '\'':
				Flags ^= 1;
				if (Result != NULL) *Result++ = '\'';
				++Length;
				break;

			case '"':
				Flags ^= 2;
				/*FALLTHROUGH*/
			default:
				if (Result != NULL) *Result++ = FormatString[-1];
				++Length;
		}
	}
}

INT FormatShellW(PWSTR Result, LPSTR FormatString, LPSTR Cmd0, PWSTR Cmd1)
{	LPSTR lp;
	PWSTR pw;
	INT   Flags = 0, Length = 0;
	BOOL  Cmd1Done = FALSE;

	for (;;) {
		switch (*FormatString++) {
			case '\0':
				if (!Cmd1Done) {
					if (Result != NULL) {
						*Result++ = ' ';
						WvCopyString(Result, Cmd1);
					}
					Length += wcslen(Cmd1) + 1;
				} else if (Result != NULL) *Result = '\0';
				return (Length);

			case '%':
				switch (*FormatString++) {
					case '0':	lp = Cmd0;
								while (*lp != '\0') {
									if ((*lp == '\'' && Flags & 1) ||
										(*lp == '"'  && Flags & 2) ||
										(*lp == '\\' && Flags)) {
											if (Result != NULL)
												*Result++ = '\\';
											++Length;
									}
									if (Result != NULL) *Result++ = *lp;
									++Length;
									++lp;
								}
								break;

					case '1':	pw = Cmd1;
								Cmd1Done = TRUE;
								while (*pw != '\0') {
									if ((*pw == '\'' && Flags & 1) ||
										(*pw == '"'  && Flags & 2) ||
										(*pw == '\\' && Flags)) {
											if (Result != NULL)
												*Result++ = '\\';
											++Length;
									}
									if (Result != NULL) *Result++ = *pw;
									++Length;
									++pw;
								}
								break;

					case '%':	++FormatString;
								/*FALLTHROUGH*/
					default:	--FormatString;
								if (Result != NULL) *Result++ = '%';
								++Length;
								continue;
				}
				break;

			case '\'':
				Flags ^= 1;
				if (Result != NULL) *Result++ = '\'';
				++Length;
				break;

			case '"':
				Flags ^= 2;
				/*FALLTHROUGH*/
			default:
				if (Result != NULL) *Result++ = FormatString[-1];
				++Length;
		}
	}
}

extern CHAR  QueryString[150];
extern DWORD QueryTime;

BOOL ShowUrl(PSTR Url)
{
 WCHAR *wide=(WCHAR*)calloc(WV_TEXT_CAP,sizeof(WCHAR));BOOL result=FALSE;
 if(wide && MultiByteToWideChar(CP_ACP,0,Url,-1,wide,WV_TEXT_CAP))result=OpenWinViUrlW(wide);
 free(wide);return result;
}
PCSTR HelpAnchors[] = {
	/*HelpAll*/				NULL,
	/*HelpEditTextmode*/	"EditTextmode",
	/*HelpEditHexmode*/		"EditHexmode",
	/*HelpInsertmode*/		"Insertmode",
	/*HelpReplacemode*/		"Replacemode",
	/*HelpEditC*/			"EditC",
	/*HelpEditD*/			"EditD",
	/*HelpEditY*/			"EditY",
	/*HelpEditZ*/			"EditZ",
	/*HelpEditMark*/		"EditMark",
	/*HelpEditFT*/			"EditFT",
	/*HelpEditZZ*/			"EditZZ",
	/*HelpEditShift*/		"EditShift",
	/*HelpEditShell*/		"EditShell",
	/*HelpEditDoublequote*/	"EditDoublequote",
	/*HelpPrint*/			"Print",
	/*HelpSearch*/			"Search",
	/*HelpSearchDlg*/		"SearchDlg",
	/*HelpSettingsProfile*/	"SettingsProfile",
	/*HelpSettingsFiletype*/"SettingsFiletype",
	/*HelpSettingsDisplay*/	"SettingsDisplay",
	/*HelpSettingsEdit*/	"SettingsEdit",
	/*HelpSettingsFiles*/	"SettingsFiles",
	/*HelpSettingsMapping*/	"SettingsMapping",
	/*HelpSettingsShell*/	"SettingsShell",
	/*HelpSettingsColors*/	"SettingsColors",
	/*HelpSettingsFonts*/	"SettingsFonts",
	/*HelpSettingsPrint*/	"SettingsPrint",
	/*HelpSettingsLanguage*/"SettingsLang",
	/*HelpExCommand*/		"ExCommand",
	/*HelpExRead*/			"ExRead",
	/*HelpExEdit*/			"ExEdit",
	/*HelpExNext*/			"ExNext",
	/*HelpExRewind*/		"ExRewind",
	/*HelpExWrite*/			"ExWrite",
	/*HelpExFilename*/		"ExFilename",
	/*HelpExArgs*/			"ExArgs",
	/*HelpExVi*/			"ExVi",
	/*HelpExCd*/			"ExCd",
	/*HelpExTag*/			"ExTag",
	/*HelpExShell*/			"ExShell",
	/*HelpExSubstitute*/	"ExSubstitute"
};
/* Help manuals are resolved relative to the executable. */
HELPCONTEXT	HelpContext;

VOID Help(VOID)
{
 extern LPSTR HelpURL;
 WCHAR *url=(WCHAR*)calloc(WV_TEXT_CAP*3,sizeof(WCHAR));WCHAR anchor[80];
 const WCHAR *language=Language==7000 ? L"de" : Language==10000 ? L"es" : Language==12000 ? L"fr" : L"en";
 anchor[0]=0;
 if(HelpContext>HelpAll && HelpContext<WSIZE(HelpAnchors))MultiByteToWideChar(CP_ACP,0,HelpAnchors[HelpContext],-1,anchor,WSIZE(anchor));
 if(url && ResolveWinViHelpUrl(HelpURL,language,anchor,url,WV_TEXT_CAP*3)) {
  if(!OpenWinViUrlW(url))NewStatus(0,L"Unable to open the help file in your default browser.",NS_ERROR);
 } else NewStatus(0,L"Help file missing. Place WinVi-en.htm beside WinVi64.exe.",NS_ERROR);
 free(url);
}