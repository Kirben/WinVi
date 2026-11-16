/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several                      *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2009 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *      3-Dec-2000	first publication of source code
 *      1-Jan-2001	some preparations for keyboard mappings (not working yet)
 *     23-Oct-2002	better handling of Ctrl+S/Ctrl+Q (xoff/xon)
 *     11-Jan-2003	mapping and abbreviations now working
 *      1-Jan-2009	can now be compiled with VC++ 2008 Express
 */

#include <windows.h>
#include <malloc.h>
#include <string.h>
#include "myassert.h"
#include "resource.h"
#include "winvi.h"
#include "map.h"
#include "exec.h"
#include "page.h"

#define MAP_KEYDOWN 0x0100
#define MAP_CHAR	0x0200
#define MAP_CTRL	0x0400
#define MAP_SHIFT	0x0800
#define MAP_ALT		0x1000

PMAP FirstMapping = NULL, NewMappings = NULL;

BOOL InitMappings(VOID)
{
	return (TRUE);
}

VOID TermMappings(VOID)
{
}

VOID FreeMappings(PMAP Tree)
{
	if(Tree && Tree==FirstMapping)StopMap(TRUE);
	while (Tree != NULL) {
		PMAP ToDel = Tree;

		Tree = Tree->Next;
		if (ToDel->Name			!= NULL) _ffree(ToDel->Name);
		if (ToDel->InputDisplay	!= NULL) _ffree(ToDel->InputDisplay);
		if (ToDel->Replace		!= NULL) _ffree(ToDel->Replace);
		if (ToDel->InputMatch	!= NULL) _ffree(ToDel->InputMatch);
		if (ToDel->ReplaceMap	!= NULL) _ffree(ToDel->ReplaceMap);
		free(ToDel);
	}
}

extern HWND hwndSettings;

VOID DefaultOkButton(HWND hDlg)
{	HWND SetButton = GetDlgItem(hDlg, IDC_SETMACRO);

	SendMessage(hDlg, DM_SETDEFID, 0, 0);
	if (GetFocus() != SetButton) {
		SendMessage(SetButton, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 1);
		SendMessage(hwndSettings, DM_SETDEFID, IDOK, 0);
	}
}

static BYTE Keys[] = {
	VK_F10,	VK_F11,	VK_F12,
	VK_F1,	VK_F2,	VK_F3,	VK_F4,	VK_F5,	VK_F6,	VK_F7,	VK_F8,	VK_F9,

	VK_INSERT,	VK_INSERT,	VK_INSERT,	VK_DELETE,	VK_DELETE,	VK_DELETE,
	VK_HOME,	VK_HOME,	VK_HOME,	VK_END,		VK_END,
	VK_PRIOR,	VK_PRIOR,	VK_PRIOR,	VK_PRIOR,
	VK_NEXT,	VK_NEXT,	VK_NEXT,	VK_NEXT,	VK_NEXT,
	VK_RETURN,	VK_RETURN,	VK_RETURN,	VK_RETURN,	VK_RETURN,	VK_RETURN,
	VK_TAB,		VK_ESCAPE,	VK_ESCAPE,
	VK_SPACE,	VK_SPACE,	VK_SPACE,	VK_SPACE,	VK_SPACE,
	VK_BACK,	VK_BACK,	VK_BACK,	VK_BACK,	VK_BACK,
	VK_LEFT,	VK_LEFT,	VK_LEFT,	VK_UP,		VK_UP,		VK_UP,
	VK_RIGHT,	VK_RIGHT,	VK_RIGHT,	VK_DOWN,	VK_DOWN,	VK_DOWN,

	VK_INSERT, VK_HOME, VK_PRIOR,			 VK_UP,
	VK_DELETE, VK_END,  VK_NEXT,	VK_LEFT, VK_DOWN, VK_RIGHT
};
static PSTR KeyNames[] = {
	"F10",	"F11",	"F12",
	"F1",	"F2",	"F3",	"F4",	"F5",	"F6",	"F7",	"F8",	"F9",

	"Insert",	"Inser",	"Einfg",	"Supr",		"Suppr",	"Entf",
	"Inicio",	"Origine",	"Pos1",		"Fin",		"Ende",
	"RePág",	"RePag",	"PgPrec",	"BildHoch",
	"AvPág",	"AvPag",	"PgSuiv",	"PgDown",	"BildRunter",
	"Enter",	"Return",	"Entrar",	"Entrée",	"Entree",	"Entr",
	"Tab",		"Esc",		"Echap",
	"Blank",	"Space",	"Espacio",	"Espace",	"Leer",
	"Backspace","Retroceso","RetArr",	"Rück",		"Rueck",
	"Izquierda","Gauche",	"Links",	"Arriba",	"Haute",	"Hoch",
	"Derecha",	"Droite",	"Rechts",	"Abajo",	"Bas",		"Runter",

	"Ins",	"Home",	"PgUp",					"Up",
	"Del",	"End",	"PgDn",			"Left",	"Down",	"Right",

	NULL
};

static BOOL HandleAsWmChar(DWORD VKey)
{
	switch (VKey) {
		case VK_TAB:
		case VK_RETURN:
		case VK_ESCAPE:
		case VK_SPACE:
			return (TRUE);

		default:
			return (FALSE);
	}
}

size_t WvMapLength(const DWORD *p)
{size_t n=0;if(p)while(p[n])n++;return n;}
DWORD WvMapChar(WCHAR c)
{return c>255?WV_MAP_UNICODE|c:MAP_CHAR|(c<32?MAP_CTRL|'@'|c:c);}
BOOL TranslateMapping(LPDWORD *ppMapping, PCSTR String)
{ DWORD *Result,*p;size_t length=strlen(String);
	INT  c;

 *ppMapping=NULL;
 if(length>=WV_MAP_TEXT_CAP)return FALSE;
 Result=calloc(length+2,sizeof(DWORD));if(!Result)return FALSE;p=Result;
	assert(sizeof(Keys) == sizeof(KeyNames)/sizeof(KeyNames[0])-1);
	*p = 0;
	while (*String) {
		assert(p < Result + length + 1);
		switch (*String) {
			case '\'':
			case '"':	/*WM_CHARs*/
						c = *String;
						while (*++String && *String != c) {
							assert(p < Result + length + 1);
							p[1] = p[0];
							*p |= MAP_CHAR | (BYTE)*String;
							if ((BYTE)*String < ' ') *p |= MAP_CTRL | '@';
							++p;
						}
						*p = 0;
						if (*String) ++String;
						break;

			case ' ':	/*separator with no meaning*/
						++String;
						break;

            default:
                        if(String[0]=='V' && String[1]=='+' && strlen(String)>=4){
                            UINT value=0;INT i;for(i=2;i<4;i++){INT d=String[i];d=d>='0'&&d<='9'?d-'0':d>='A'&&d<='F'?d-'A'+10:d>='a'&&d<='f'?d-'a'+10:-1;if(d<0)break;value=value*16+d;}
                            if(i==4 && value && (!String[4] || String[4]==' ')){*p++|=MAP_KEYDOWN|value;*p=0;String+=4;break;}
                        }
                        if(String[0]=='F' && String[1]>='1' && String[1]<='2' && String[2]>='0' && String[2]<='9'){
                            UINT f=(String[1]-'0')*10+String[2]-'0';
                            if(f>=13 && f<=24 && (!String[3] || String[3]==' ')){*p++|=MAP_KEYDOWN|(VK_F1+f-1);*p=0;String+=3;break;}
                        }
                        if(String[0]=='U' && String[1]=='+' && strlen(String)>=6){
                            UINT value=0;INT i;
                            for(i=2;i<6;i++){INT d=String[i];d=d>='0'&&d<='9'?d-'0':d>='A'&&d<='F'?d-'A'+10:d>='a'&&d<='f'?d-'a'+10:-1;if(d<0)break;value=value*16+d;}
                            if(i==6 && (!String[6] || String[6]==' ')){if(*p && value>255){free(Result);return FALSE;}*p++|=WvMapChar((WCHAR)value);*p=0;String+=6;break;}
                        }
                        /*check several strings...*/
						if (strncmp(String, "Ctrl+", 5) == 0 ||
							strncmp(String, "Strg+", 5) == 0) {
								*p |= MAP_CTRL;
								String += 5;
								break;
						}
						if (strncmp(String, "Shift+",    6) == 0 ||
							strncmp(String, "Umschalt+", 9) == 0 ||
							strncmp(String, "Mayus+",    6) == 0 ||
							strncmp(String, "Mayús+",    6) == 0 ||
							strncmp(String, "Maj+",      4) == 0) {
								*p |= MAP_SHIFT;
								String = strchr(String, '+') + 1;
								break;
						}
						if (strncmp(String, "Alt+", 4) == 0) {
							*p |= MAP_ALT;
							String += 4;
							break;
						}
						{	PSTR *pn;

							for (pn = KeyNames; *pn != NULL; ++pn) {
								if (strncmp(String, *pn, lstrlen(*pn)) == 0) {
									String += lstrlen(*pn);
									*p |= Keys[pn-KeyNames];
									if (HandleAsWmChar(*p))	*p |= MAP_CHAR;
									else					*p |= MAP_KEYDOWN;
									break;
								}
							}
							if (*pn == NULL) *p |= MAP_CHAR | (BYTE)*String++;
							if (*p & MAP_CHAR) {
								if ((BYTE)*p < ' ') *p |= MAP_CTRL | '@';
								if (*p & MAP_CTRL
										&& (BYTE)*p >= 'a' && (BYTE)*p <= 'z')
									*p &= ~('a'-'A');
								if (*p == (MAP_CTRL | MAP_ALT | MAP_CHAR | 'V'))
									*p ^= MAP_KEYDOWN | MAP_CHAR;
							}
							*++p = 0;
						}
						break;
		}
	}
 *p=0;
 for(DWORD *q=Result;*q;q++)if(*q&WV_MAP_UNICODE){
  UINT c=*q&0xffff;
  if(c>=0xd800 && c<=0xdbff){if(!(q[1]&WV_MAP_UNICODE) || (q[1]&0xffff)<0xdc00 || (q[1]&0xffff)>0xdfff){free(Result);return FALSE;}q++;}
  else if(c>=0xdc00 && c<=0xdfff){free(Result);return FALSE;}
 }
 *ppMapping=Result;return TRUE;
}

PMAP DupMapping(PMAP m)
{	PMAP n;

	if ((n = calloc(1, sizeof(*n))) == NULL) return (NULL);
	AllocStringA(&n->Name, m->Name);
	AllocStringA(&n->InputDisplay, m->InputDisplay);
	AllocStringA(&n->Replace, m->Replace);
 n->ReplaceMap=calloc(WvMapLength(m->ReplaceMap)+1,sizeof(DWORD));
 if(n->ReplaceMap)memcpy(n->ReplaceMap,m->ReplaceMap,(WvMapLength(m->ReplaceMap)+1)*sizeof(DWORD));
 n->InputMatch=calloc(WvMapLength(m->InputMatch)+1,sizeof(DWORD));
 if(n->InputMatch)memcpy(n->InputMatch,m->InputMatch,(WvMapLength(m->InputMatch)+1)*sizeof(DWORD));

	n->Flags = m->Flags;
	if (n->Name == NULL || n->InputDisplay == NULL || n->Replace	== NULL
						|| n->InputMatch   == NULL || n->ReplaceMap	== NULL) {
		FreeMappings(n);
		return (NULL);
	}
	return (n);
}

INT FindMacroName(HWND ListBox, LPSTR Name)
	/*case sensitive searching in listbox*/
{	INT  i;
	CHAR Buf[128];

	i = -1;
	do {
		INT j = i;

		i = SendMessage(ListBox, LB_FINDSTRINGEXACT, i, (LPARAM)Name);
		if (i <= j) return (-1);
		SendMessage(ListBox, LB_GETTEXT, i, (LPARAM)(LPSTR)Buf);
	} while (lstrcmp(Buf, Name) != 0);
	return (i);
}

BOOL IsMappingEqual(LPDWORD a,LPDWORD b)
{size_t n=WvMapLength(a);return n==WvMapLength(b) && (!n || !memcmp(a,b,n*sizeof(DWORD)));}

INT_PTR MacrosCallback(HWND hDlg, UINT uMsg, WPARAM wPar, LPARAM lPar)
{	HWND  ListBox;
	CHAR *Buf;
	PMAP  ThisMapping, m, *MapPtr;
	INT	  i, k;

	PARAM_NOT_USED(lPar);
 if(uMsg!=WM_INITDIALOG && uMsg!=WM_COMMAND)return FALSE;
 Buf=malloc(WV_MAP_TEXT_CAP);if(!Buf)return FALSE;
	switch (uMsg) {
		case WM_INITDIALOG:
			FreeMappings(NewMappings);
			NewMappings = NULL;
			ListBox = GetDlgItem(hDlg, IDC_MACROLIST);
			if (ListBox) {
				MapPtr = &NewMappings;
				SendMessage(ListBox, LB_RESETCONTENT, 0, 0);
				for (m = FirstMapping; m != NULL; m = m->Next) {
					if ((ThisMapping = DupMapping(m)) != NULL) {
						SendMessage(ListBox, LB_ADDSTRING, 0, (LPARAM)m->Name);
						*MapPtr = ThisMapping;
						MapPtr = &ThisMapping->Next;
					} else ErrorBox(MB_ICONSTOP, 300, 6);
				}
				SetDlgItemText(hDlg, IDC_MACRONAME,	   "");
				SetDlgItemText(hDlg, IDC_MACROINPUT,   "");
				SetDlgItemText(hDlg, IDC_MACROREPLACE, "");
				SendDlgItemMessage(hDlg, IDC_MACRONAME,    EM_LIMITTEXT,
								   127, 0);
				SendDlgItemMessage(hDlg, IDC_MACROINPUT,   EM_LIMITTEXT,
								   WV_MAP_TEXT_CAP   - 1, 0);
				SendDlgItemMessage(hDlg, IDC_MACROREPLACE, EM_LIMITTEXT,
								   WV_MAP_TEXT_CAP   - 1, 0);
				EnableWindow(GetDlgItem(hDlg, IDC_DELMACRO), FALSE);
			}
			CheckDlgButton(hDlg, IDC_INSERTMODE,  TRUE);
			CheckDlgButton(hDlg, IDC_COMMANDMODE, TRUE);
			CheckDlgButton(hDlg, IDC_COMMANDLINE, TRUE);
			CheckRadioButton(hDlg, IDC_MAP, IDC_ABBREV, IDC_MAP);
			break;

		case WM_COMMAND:
			ListBox = GetDlgItem(hDlg, IDC_MACROLIST);
			if (!ListBox) break;
			switch (COMMAND) {
				static BOOL InitialString = FALSE;
				
				case IDC_MACROLIST:
					if (NOTIFICATION != LBN_SELCHANGE) break;
					i = SendMessage(ListBox, LB_GETCURSEL, 0, 0);
					if (i == LB_ERR) m = NULL;
					else {
						SendMessage(ListBox, LB_GETTEXT, i, (LPARAM)(LPSTR)Buf);
						for (m = NewMappings;
							 m != NULL && lstrcmp(m->Name, Buf);
							 m = m->Next);
					}
					if (m != NULL) {
						InitialString = TRUE;
						SetDlgItemText(hDlg, IDC_MACRONAME, Buf);
						SetDlgItemText(hDlg, IDC_MACROINPUT, m->InputDisplay);
						SetDlgItemText(hDlg, IDC_MACROREPLACE, m->Replace);
						CheckRadioButton(hDlg, IDC_MAP, IDC_ABBREV,
										 m->Flags & M_ABBREVIATION
										 ? IDC_ABBREV : IDC_MAP);
						CheckDlgButton(hDlg, IDC_INSERTMODE,
									   (m->Flags & M_INSERTMODE) !=0);
						CheckDlgButton(hDlg, IDC_COMMANDMODE,
									   (m->Flags & M_COMMANDMODE)!=0);
						CheckDlgButton(hDlg, IDC_COMMANDLINE,
									   (m->Flags & M_COMMANDLINE)!=0);
						EnableWindow(GetDlgItem(hDlg, IDC_COMMANDMODE),
									 (m->Flags & M_ABBREVIATION) ==0);
						InitialString = FALSE;
					} else {
						/*should not occur, set empty name to indicate error*/
						SetDlgItemText(hDlg, IDC_MACRONAME, "");
					}
					EnableWindow(GetDlgItem(hDlg, IDC_DELMACRO),
								 m != NULL);
					DefaultOkButton(hDlg);
					break;

				case IDC_MACRONAME:
				case IDC_MACROINPUT:
					if (NOTIFICATION != EN_CHANGE) break;
					GetDlgItemText(hDlg, IDC_MACRONAME, Buf, 128);
					for (k = 0; k < 2; ++k) {
						LPDWORD CmpMapping;
						DWORD  f;

						i = FindMacroName(ListBox, Buf);
						if (i != -1 || k) break;
						/*same name not found, try to find same input...*/
						GetDlgItemText(hDlg, IDC_MACROINPUT, Buf, WV_MAP_TEXT_CAP);
						if (!TranslateMapping(&CmpMapping, Buf)) break;
						f = 0;
						if (IsDlgButtonChecked(hDlg, IDC_ABBREV))
							f |= M_ABBREVIATION;
						if (IsDlgButtonChecked(hDlg, IDC_INSERTMODE))
							f |= M_INSERTMODE;
						if (IsDlgButtonChecked(hDlg, IDC_COMMANDMODE))
							f |= M_COMMANDMODE;
						if (IsDlgButtonChecked(hDlg, IDC_COMMANDLINE))
							f |= M_COMMANDLINE;
						for (m = NewMappings; m != NULL; m = m->Next)
							if (IsMappingEqual(m->InputMatch, CmpMapping)
									&& (m->Flags & f) == m->Flags)
								break;
						_ffree(CmpMapping);
						if (m == NULL) break;
						lstrcpy(Buf, m->Name);
					}
					SendMessage(ListBox, LB_SETCURSEL, i, 0);
					EnableWindow(GetDlgItem(hDlg, IDC_DELMACRO), i >= 0);
					/*FALLTHROUGH*/
				case IDC_MACROREPLACE:
					if (NOTIFICATION != EN_CHANGE || InitialString) break;
					/*FALLTHROUGH*/
				case IDC_INSERTMODE:
				case IDC_COMMANDMODE:
				case IDC_COMMANDLINE:
				case IDC_MAP:
				case IDC_ABBREV:
					if (COMMAND == IDC_ABBREV || COMMAND == IDC_MAP) {
						if (COMMAND == IDC_ABBREV)
							CheckDlgButton(hDlg, IDC_COMMANDMODE, FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_COMMANDMODE),
									 COMMAND == IDC_MAP);
					}
					if (GetDlgItemText(hDlg, IDC_MACRONAME, Buf, 128) &&
						GetDlgItemText(hDlg, IDC_MACROINPUT,Buf, WV_MAP_TEXT_CAP)) {
							SendMessage(hwndSettings, DM_SETDEFID, 102, 0);
							SendMessage(hDlg, DM_SETDEFID, IDC_SETMACRO, 0);
					}
					break;

				case IDC_SETMACRO:
					ThisMapping = calloc(1, sizeof(*ThisMapping));
					if (ThisMapping != NULL) {
						PMAP *pmNew, *pmFree = NULL;

						/*create a new entry...*/
						GetDlgItemText(hDlg, IDC_MACROINPUT, Buf, WV_MAP_TEXT_CAP);
						if (!AllocStringA	 (&ThisMapping->InputDisplay,Buf) ||
							!TranslateMapping(&ThisMapping->InputMatch,  Buf) ||
                            !WvMapLength(ThisMapping->InputMatch) || WvMapLength(ThisMapping->InputMatch)>63) {
								FreeMappings(ThisMapping);
								break;
						}
						GetDlgItemText(hDlg, IDC_MACROREPLACE,Buf,WV_MAP_TEXT_CAP);
						if (!AllocStringA	 (&ThisMapping->Replace,	 Buf) ||
							!TranslateMapping(&ThisMapping->ReplaceMap,  Buf)) {
								FreeMappings(ThisMapping);
								break;
						}
						GetDlgItemText(hDlg, IDC_MACRONAME, Buf, 128);
						if (!AllocStringA(&ThisMapping->Name,	Buf)) {
							FreeMappings(ThisMapping);
							break;
						}
						ThisMapping->Flags	 = 0;
						if (IsDlgButtonChecked(hDlg, IDC_ABBREV))
							ThisMapping->Flags |= M_ABBREVIATION;
						if (IsDlgButtonChecked(hDlg, IDC_INSERTMODE))
							ThisMapping->Flags |= M_INSERTMODE;
						if (IsDlgButtonChecked(hDlg, IDC_COMMANDMODE))
							ThisMapping->Flags |= M_COMMANDMODE;
						if (IsDlgButtonChecked(hDlg, IDC_COMMANDLINE))
							ThisMapping->Flags |= M_COMMANDLINE;
						/*find old entry to delete and new position to enter..*/
						for (MapPtr = pmNew = &NewMappings;
								(m = *MapPtr) != NULL;
								MapPtr = &m->Next) {
							if ((i = lstrcmp(m->Name, Buf)) >= 0) {
								if (i == 0) {
									pmFree = MapPtr;
									break;
								}
							} else pmNew = &m->Next;
							if (IsMappingEqual(m->InputMatch,
											   ThisMapping->InputMatch) &&
									(m->Flags & ThisMapping->Flags) == m->Flags)
								pmFree = MapPtr;
						}
						/*delete old entry*/
						if (pmFree != NULL) {
							PMAP ToFree = *pmFree;

							if ((i = FindMacroName(ListBox, ToFree->Name)) >= 0)
								SendMessage(ListBox, LB_DELETESTRING, i, 0);
							*pmFree = ToFree->Next;
							if (pmNew == &ToFree->Next) pmNew = pmFree;
							ToFree->Next = NULL;
							FreeMappings(ToFree);
						}
						/*enter new entry...*/
						i = SendMessage(ListBox, LB_ADDSTRING, 0,
										(LPARAM)ThisMapping->Name);
						SendMessage(ListBox, LB_SETCURSEL, i, 0);
						ThisMapping->Next = *pmNew;
						*pmNew = ThisMapping;
					}
					EnableWindow(GetDlgItem(hDlg, IDC_DELMACRO), TRUE);
					DefaultOkButton(hDlg);
					break;

				case IDC_DELMACRO:
					i = SendMessage(ListBox, LB_GETCURSEL, 0, 0);
					if (i != LB_ERR) {
						SendMessage(ListBox, LB_GETTEXT, i, (LPARAM)(LPSTR)Buf);
						for (MapPtr = &NewMappings;
							 *MapPtr != NULL
								&& lstrcmp((*MapPtr)->Name, Buf);
							 MapPtr = &(*MapPtr)->Next);
						if ((ThisMapping = *MapPtr) != NULL) {
							HWND DelButton = GetDlgItem(hDlg, IDC_DELMACRO);

							*MapPtr = ThisMapping->Next;
							ThisMapping->Next = NULL;
							FreeMappings(ThisMapping);
							SendMessage(ListBox, LB_DELETESTRING, i, 0);
							DefaultOkButton(hDlg);
							EnableWindow(DelButton, FALSE);
							SendMessage(DelButton, BM_SETSTYLE, (WPARAM)BS_PUSHBUTTON, 1);
							SetFocus(ListBox);
						}
					}
					break;

				case IDOK:
				case ID_APPLY:
				case IDC_STORE:
					FreeMappings(FirstMapping);
					FirstMapping = NewMappings;
					NewMappings = NULL;
					DlgResult = TRUE;
					if (COMMAND == IDC_STORE) SaveMappingSettings();
					if (COMMAND == IDOK)	  EndDialog(hDlg, 0);
					else MacrosCallback(hDlg, WM_INITDIALOG, 0, 0);
					break;

				case IDCANCEL:
					FreeMappings(NewMappings);
					NewMappings = NULL;
					EndDialog(hDlg, 0);
					break;
			}
			break;

		default:
			free(Buf);return (FALSE);
	}
	free(Buf);return (TRUE);
}

static BOOL MapIsMatching(LPDWORD Input, LPDWORD CompareWith, INT Len)
{
	#if 0
	//	return (_fstrncmp((LPCSTR)Input, (LPCSTR)CompareWith,
	//			Len * sizeof(WORD)) == 0);
	#else
		while (Len--) {
			if (*Input != *CompareWith
                && ((*Input&WV_MAP_UNICODE) || (*CompareWith&WV_MAP_UNICODE) || ((*Input & (MAP_CHAR | MAP_SHIFT)) != (MAP_CHAR | MAP_SHIFT)
					|| (*Input & ~MAP_SHIFT) != *CompareWith)))
				return (FALSE);
			++Input;
			++CompareWith;
		}
		return (TRUE);
	#endif
}

BOOL   MatchingAbbreviation;
DWORD  InputQueue[64];
INT    QueuedChars   = 0;		/*already matched input chars*/
LPDWORD DoSequence	 = NULL;
static LPDWORD ReplayOwned=NULL;
INT    SequenceLen   = 0;		/*number of valid entries in DoSequence*/
INT    AbbrevInputLen = 0;
INT    AbbrevDeletes = 0;
MSG    PendingMsg    = {0};
typedef struct {MSG message;DWORD token;} DEFERREDKEY;
static DEFERREDKEY *DeferredKeys;
static BOOL MatchKeyboardToken(PMSG,DWORD);
static size_t DeferredCount,DeferredCapacity;
static BOOL PlaybackActive(void);
DWORD  PendingDown   = 0;
DWORD  PendingUp	 = 0;
DWORD  FinalKeyState = 0;
BOOL   DenyAbbrev	 = FALSE, PrevDenyAbbrev;

static BOOL MapMatch(BOOL DequeueSingle)
{	HWND		 Focus = GetFocus();
	PMAP		 m;
	static DWORD Match;

 if(DequeueSingle && (SequenceLen>0 || PendingDown || PendingUp || AbbrevDeletes))return TRUE;
 if(SequenceLen<=0 && ReplayOwned){free(ReplayOwned);ReplayOwned=NULL;DoSequence=NULL;}
 if(hwndCmd && Focus==hwndCmd)Match=M_COMMANDLINE;
 else if(hwndMain && Focus==hwndMain)Match=Mode>=InsertMode?M_INSERTMODE:M_COMMANDMODE;
 else Match=0;

	for (m = FirstMapping; m != NULL; m = m->Next) {
		INT Len;

		if (!(m->Flags & Match) || (m->Flags&M_ABBREVIATION && PrevDenyAbbrev))
			continue;
		if ((Len = (INT)WvMapLength(m->InputMatch)) == 0) continue;
		if ((m->Flags&M_ABBREVIATION) && Len<QueuedChars)continue;
		if (Len > QueuedChars) Len = QueuedChars;
		if (MapIsMatching(InputQueue, m->InputMatch, Len)) {
			if (Len <= QueuedChars && m->InputMatch[Len] == 0) {
				/*full match...*/
				DoSequence	= m->ReplaceMap;
				SequenceLen	= (INT)WvMapLength(DoSequence);
				if (m->Flags & M_ABBREVIATION) {
					AbbrevDeletes=0;AbbrevInputLen=Len;
                    for(INT k=0;k<Len;k++){
                     DWORD c=m->InputMatch[k];AbbrevDeletes++;
                     if((c&WV_MAP_UNICODE) && (c&0xffff)>=0xd800 && (c&0xffff)<=0xdbff && k+1<Len && (m->InputMatch[k+1]&WV_MAP_UNICODE) && (m->InputMatch[k+1]&0xffff)>=0xdc00 && (m->InputMatch[k+1]&0xffff)<=0xdfff)k++;
                    }
				}
				if (GetKeyState(VK_CONTROL)	< 0) FinalKeyState |= MAP_CTRL;
				if (GetKeyState(VK_SHIFT)	< 0) FinalKeyState |= MAP_SHIFT;
				if (GetKeyState(VK_MENU)	< 0) FinalKeyState |= MAP_ALT;
				PendingUp = FinalKeyState;
				if (!(m->Flags&M_ABBREVIATION) && (QueuedChars -= Len) > 0)
				  memmove(InputQueue, InputQueue+Len, QueuedChars*sizeof(DWORD));
			} else /*partial match*/ SequenceLen = 0;
			return (!(MatchingAbbreviation = (m->Flags & M_ABBREVIATION) != 0));
		}
	}
	/*no match...*/
	if (MatchingAbbreviation) {
		QueuedChars = 0;
		MatchingAbbreviation = FALSE;
		return (FALSE);
	}
	if (DequeueSingle && QueuedChars == 1 && !SequenceLen && !AbbrevDeletes) {
		QueuedChars = 0;
		return (FALSE);
	}
	DoSequence  = InputQueue;
	SequenceLen = QueuedChars;
	return (TRUE);
}

BOOL MapInterrupted = FALSE;

static VOID KeyState(BYTE VKey, BOOL Down)
{	BYTE AllStates[256];
	BYTE Value = Down ? 0x80 : 0;

	if (GetKeyboardState(AllStates)) {
		AllStates[VKey] = (AllStates[VKey] & ~0x80) | Value;
		SetKeyboardState(AllStates);
	}
}

BOOL WvMapPrepareChange(void)
{
 LPDWORD copy;UINT_PTR p=(UINT_PTR)DoSequence;
 if(SequenceLen<=0 || ReplayOwned || (p>=(UINT_PTR)InputQueue && p<(UINT_PTR)(InputQueue+64)))return TRUE;
 if((size_t)SequenceLen>SIZE_MAX/sizeof(DWORD)-1)return FALSE;
 copy=malloc(((size_t)SequenceLen+1)*sizeof(DWORD));if(!copy)return FALSE;
 memcpy(copy,DoSequence,(size_t)SequenceLen*sizeof(DWORD));copy[SequenceLen]=0;
 ReplayOwned=DoSequence=copy;return TRUE;
}

VOID StopMap(BOOL ResetShiftKeys)
{
	AbbrevInputLen = AbbrevDeletes = QueuedChars = SequenceLen = PendingDown = PendingUp = 0;
 free(ReplayOwned);ReplayOwned=NULL;DoSequence=NULL;PendingMsg.message=0;free(DeferredKeys);DeferredKeys=NULL;DeferredCount=DeferredCapacity=0;MatchingAbbreviation=DenyAbbrev=PrevDenyAbbrev=FALSE;
	if (ResetShiftKeys) {
		if (GetKeyState(VK_CONTROL)	< 0) KeyState(VK_CONTROL, FALSE);
		if (GetKeyState(VK_SHIFT)	< 0) KeyState(VK_SHIFT,	  FALSE);
		if (GetKeyState(VK_MENU)	< 0) KeyState(VK_MENU,	  FALSE);
		FinalKeyState = 0;
	}
}

static BOOL MappingWord(DWORD token)
{
 if(token&WV_MAP_UNICODE){WCHAR c=(WCHAR)token;WORD one=0,three=0;
  GetStringTypeW(CT_CTYPE1,&c,1,&one);GetStringTypeW(CT_CTYPE3,&c,1,&three);
  return (one&(C1_ALPHA|C1_DIGIT)) || (three&(C3_NONSPACING|C3_DIACRITIC));
 }
 return (token&(MAP_CHAR|MAP_CTRL))==MAP_CHAR && (CharFlags((BYTE)token)&8)!=0;
}

BOOL IsKeyboardMapMsg(PMSG Msg)
	/*returns: TRUE  if a keyboard message matches part of or the total input
	 *				 sequence of a mapping or a previously matching sequence
	 *				 does not match anymore and must be handled first,
	 *		   FALSE if Msg is not a keyboard message or if it does not match
	 *				 a mapping macro.
	 */
{ DWORD c = 0;
	if (Msg->message == WM_KEYDOWN || Msg->message == WM_SYSKEYDOWN) {
		if ((Msg->wParam>=VK_F1 && Msg->wParam<=VK_F24) || (Msg->wParam<=255 && memchr(Keys,(int)Msg->wParam,sizeof(Keys))!=NULL)) {
			c = (BYTE)Msg->wParam;
			if (GetKeyState(VK_CONTROL)	< 0) c |= MAP_CTRL;
			if (GetKeyState(VK_SHIFT)	< 0) c |= MAP_SHIFT;
			if (GetKeyState(VK_MENU)	< 0) c |= MAP_ALT;
			if (HandleAsWmChar(c)) c = 0;
			else c |= MAP_KEYDOWN;
		} else if (Msg->wParam == 'V'
				   && GetKeyState(VK_MENU)	  < 0
				   && GetKeyState(VK_CONTROL) < 0)
			c = MAP_CTRL | MAP_ALT | MAP_KEYDOWN | 'V';
	} else if (Msg->message == WM_CHAR || Msg->message == WM_SYSCHAR) {
        if(Msg->wParam>255)c=WV_MAP_UNICODE|(WCHAR)Msg->wParam;
        else c=MAP_CHAR|(BYTE)Msg->wParam;
		if (c < (MAP_CHAR | ' '))	   c |= MAP_CTRL | '@';
        if(!(c&WV_MAP_UNICODE)){
		 if (GetKeyState(VK_SHIFT) < 0) c |= MAP_SHIFT;
		 if (GetKeyState(VK_MENU)  < 0) c |= MAP_ALT;
        }
	}
 return MatchKeyboardToken(Msg,c);
}

static BOOL MatchKeyboardToken(PMSG Msg,DWORD c)
{
	if (c == 0) return (FALSE);

	if (c == (MAP_CHAR|MAP_CTRL|'C') || c == (MAP_KEYDOWN|MAP_CTRL|VK_CANCEL)) {
		StopMap(FALSE);
		MapInterrupted = TRUE;
	}
 /* Do not match physical type-ahead until the preceding replacement and its
    delimiter have reached the editor. Word boundaries depend on that order. */
 if(PlaybackActive()){
  if(DeferredCount==DeferredCapacity){
   size_t capacity=DeferredCapacity?DeferredCapacity*2:64;
   DEFERREDKEY *keys=NULL;
   if(capacity>DeferredCapacity && capacity<=SIZE_MAX/sizeof(DEFERREDKEY))keys=malloc(capacity*sizeof(DEFERREDKEY));
   if(!keys){StopMap(TRUE);NewStatus(0,L"Not enough memory for keyboard type-ahead; macro stopped.",NS_ERROR);return FALSE;}
   if(DeferredCount)memcpy(keys,DeferredKeys,DeferredCount*sizeof(DEFERREDKEY));
   free(DeferredKeys);DeferredKeys=keys;DeferredCapacity=capacity;
  }
  DeferredKeys[DeferredCount].message=*Msg;DeferredKeys[DeferredCount++].token=c;
  return TRUE;
 }
 static HWND previousFocus;
 static INT previousMode=-1;
 HWND focus=GetFocus();
 /* A normal-mode command such as i is not part of the inserted word. */
 if(focus!=previousFocus || Mode!=previousMode){
  /* Echoed abbreviation text belongs to its original mode/control. */
  if(MatchingAbbreviation){
   QueuedChars=AbbrevInputLen=AbbrevDeletes=SequenceLen=0;
   DoSequence=NULL;MatchingAbbreviation=FALSE;
  }
  DenyAbbrev=PrevDenyAbbrev=FALSE;
  previousFocus=focus;previousMode=Mode;
 }

	if (AbbrevDeletes && !PendingMsg.message) {
		if (!MappingWord(c)) {
			/*final match of non-letter for abbreviation...*/
			memcpy(&PendingMsg, Msg, sizeof(PendingMsg));
            if(AbbrevInputLen>0 && AbbrevInputLen<=QueuedChars){QueuedChars-=AbbrevInputLen;memmove(InputQueue,InputQueue+AbbrevInputLen,(size_t)QueuedChars*sizeof(DWORD));}AbbrevInputLen=0;
			return (TRUE);
		}
		AbbrevDeletes=AbbrevInputLen=SequenceLen=0;DoSequence=NULL;
	}
	if (QueuedChars >= sizeof(InputQueue)/sizeof(InputQueue[0])-1) {
        /* Abbreviation prefixes have already been echoed. A longer word must
           pass through unchanged, even when the matching buffer is full. */
        if(MatchingAbbreviation){
         QueuedChars=AbbrevInputLen=AbbrevDeletes=SequenceLen=0;DoSequence=NULL;
         MatchingAbbreviation=FALSE;DenyAbbrev=MappingWord(c);return FALSE;
        }
		/*buffer size exceeded, ignore keyboard message...*/
		return (TRUE);
	}
	PrevDenyAbbrev = DenyAbbrev && QueuedChars == 0;
	DenyAbbrev=MappingWord(c);
	InputQueue[QueuedChars++] = c;
	return (MapMatch(TRUE));
}

static BOOL PlaybackActive(void)
{return PendingDown || PendingUp || PendingMsg.message || (SequenceLen>0 && !AbbrevDeletes);}
static HWND KeyDownFocus;

BOOL KeyboardMapDequeue(PMSG Msg)
	/*called only after CheckMapping returned TRUE,
	 *returns: TRUE  if there are pending messages from a mapping
	 *				 (in Msg, the next msg resulting from macro is returned),
	 *		   FALSE if there is no more pending message to handle.
	 */
{ HWND Focus = GetFocus();
 while(!PlaybackActive() && DeferredCount){
  DWORD token=DeferredKeys[0].token;*Msg=DeferredKeys[0].message;--DeferredCount;
  memmove(DeferredKeys,DeferredKeys+1,(size_t)DeferredCount*sizeof(DEFERREDKEY));
  Msg->hwnd=Focus;
  if(!MatchKeyboardToken(Msg,token))return TRUE;
 }

    if(AbbrevDeletes && !PendingMsg.message)return FALSE;
	if (AbbrevDeletes) {
		--AbbrevDeletes;
		Msg->message = WM_CHAR;
		Msg->wParam	 = '\b';
		Msg->lParam	 = 1;
		Msg->hwnd	 = Focus;
		return (TRUE);
	}
 if(!PendingDown && !PendingUp && SequenceLen<=0 && PendingMsg.message){
  memcpy(Msg,&PendingMsg,sizeof(*Msg));PendingMsg.message=0;DenyAbbrev=PrevDenyAbbrev=FALSE;return TRUE;
 }
	if (!PendingDown && !PendingUp) {
		if (SequenceLen <= 0 && QueuedChars) MapMatch(FALSE);
		if (SequenceLen > 0) {
			--SequenceLen;
			PendingDown = PendingUp = *DoSequence;
            if(DoSequence++==InputQueue){
             if(QueuedChars>0)memmove(InputQueue,InputQueue+1,(size_t)--QueuedChars*sizeof(DWORD));
             SequenceLen=0;DoSequence=NULL;
            }
		} else {
			PendingDown = FinalKeyState;	/*restore last received kbd state*/
			FinalKeyState = 0;
		}
	}
 if(PendingDown & WV_MAP_UNICODE){
  Msg->message=WM_CHAR;Msg->wParam=PendingDown&0xffff;PendingDown=PendingUp=0;
 }else if (PendingDown) {
		if (PendingDown & MAP_CHAR) {
			Msg->message = PendingDown & MAP_ALT ? WM_SYSCHAR : WM_CHAR;
			Msg->wParam = (BYTE)PendingDown;
			if (PendingDown & MAP_CTRL) Msg->wParam &= ~0x60;
			PendingDown = PendingUp = 0;
		} else {
			Msg->message = WM_KEYDOWN;
			if (PendingDown & MAP_CTRL) {
				PendingDown &= ~MAP_CTRL;
				Msg->wParam = VK_CONTROL;
			} else if (PendingDown & MAP_SHIFT) {
				PendingDown &= ~MAP_SHIFT;
				Msg->wParam = VK_SHIFT;
			} else if (PendingDown & MAP_ALT) {
				PendingDown &= ~MAP_ALT;
				Msg->wParam = VK_MENU;
			} else {
				Msg->wParam = (BYTE)PendingDown;
				PendingDown = 0;
				if ((PendingUp & MAP_ALT /*&& Msg->wParam == VK_F4*/)
										   || Msg->wParam == VK_F10)
					Msg->message = WM_SYSKEYDOWN;
				KeyDownFocus = Focus;
			}
			KeyState(Msg->wParam, TRUE);
		}
	} else if (PendingUp) {
		Msg->message = WM_KEYUP;
		if (PendingUp & 0xff) {
			Msg->wParam = (BYTE)PendingUp;
			PendingUp &= ~(MAP_KEYDOWN | 0xff);
			if (KeyDownFocus != Focus) {
				/*some keys are handled when released (e.g. Enter),
				 *skip this one because it is for a different window...
				 */
				KeyState(Msg->wParam, FALSE);
				return (KeyboardMapDequeue(Msg));
			}
			if ((PendingUp & MAP_ALT /*&& Msg->wParam == VK_F4*/)
									   || Msg->wParam == VK_F10)
				Msg->message = WM_SYSKEYUP;
		} else if (PendingUp & MAP_ALT) {
			PendingUp &= ~MAP_ALT;
			Msg->wParam = VK_MENU;
		} else if (PendingUp & MAP_SHIFT) {
			PendingUp &= ~MAP_SHIFT;
			Msg->wParam = VK_SHIFT;
		} else if (PendingUp & MAP_CTRL) {
			PendingUp &= ~MAP_CTRL;
			Msg->wParam = VK_CONTROL;
		}
		KeyState(Msg->wParam, FALSE);
	} else {
		if (PendingMsg.message == 0) return (FALSE);
		memcpy(Msg, &PendingMsg, sizeof(*Msg));
		PendingMsg.message = 0;
	}
	Msg->hwnd	= Focus;
	Msg->lParam	= 1;
	#if 0
//	/*check for Enter/Escape in search dialogbox...*/
//	/*does not work*/
//	{	extern HWND hwndSearch;
//
//		if (GetParent(Focus) == hwndSearch && hwndSearch) {
//			if (Msg->wParam == '\r' || Msg->wParam == '\033')
//				if (Msg->wParam == WM_CHAR) {
//					Msg->wParam	= WM_KEYDOWN;
//					PendingUp	= Msg->wParam;
//					PostMessage(Focus, Msg->message, Msg->wParam, 1);
//				}
//		}
//	}
	#endif
	if (Msg->message == WM_SYSCHAR || Msg->message == WM_SYSKEYDOWN)
		Msg->lParam |= 1L << 29;
	/*check if next character is a menu choice...*/
	if (((Msg->message == WM_SYSCHAR && Msg->wParam >= ' ')
		 || (Msg->wParam == VK_F10 
			 && PendingUp == (MAP_KEYDOWN | MAP_SHIFT | VK_F10)))
		&& Focus == hwndMain && SequenceLen && !(*DoSequence & WV_MAP_UNICODE) && *DoSequence & MAP_CHAR) {
			/*menu select messages are *not* passing main loop,
			 *they seem to be handled within GetMessage/PeekMessage,
			 *so the mapped menu character must be posted.
			 */
			PostMessage(Focus, WM_CHAR, (BYTE)*DoSequence++, 1);
			--SequenceLen;
	}
	return (TRUE);
}

/* UTF-16 input is combined before encoding, so UTF-8 never receives lone surrogates. */
VOID WmUnicodeScalar(WPARAM scalar) { WmChar(scalar); }

VOID WmChar(WPARAM Char)
{	static WCHAR Hold[10];
    static WCHAR highSurrogate;
    if (Char >= 0xd800 && Char <= 0xdbff) { highSurrogate=(WCHAR)Char; return; }
    if (highSurrogate) {
        WCHAR high=highSurrogate; highSurrogate=0;
        if(Char>=0xdc00 && Char<=0xdfff) Char=0x10000+(((UINT)high-0xd800)<<10)+(UINT)Char-0xdc00;
        else if(Char>=0x20) GotChar(0xfffd,0);
    }
    if(Char>=0xdc00 && Char<=0xdfff) Char=0xfffd;

	if (!Disabled) {
		if (Interruptable) {
			#if defined(WIN32)
				switch (Char) {
					case CTRL('s'):	XOffState = TRUE;
									LOADSTRINGW(hInst, 914, Hold, WSIZE(Hold));
									NewStatus(1, Hold, NS_NORMAL);
									break;
					case CTRL('c'):	Interrupted	= TRUE;
									ExecInterrupt(CTRL_C_EVENT);
									/*FALLTHROUGH*/
					case CTRL('q'): XOffState = FALSE;
									NewStatus(1, L"", NS_NORMAL);
									ReleaseXOff();
									break;
				}
			#endif
		} else GotChar(Char, 0);
	} else if (Char == CTRL('c'))
		#if defined(WIN32)
			Interrupted = CTRL_C_EVENT+1;
		#else
			Interrupted	= TRUE;
		#endif
}

VOID WmKeydown(WPARAM VKey)
{
	TooltipHide(-1);
	if (Interruptable) {
		extern BOOL EscapeInput;

		#if defined(WIN32)
			if (VKey == VK_PAUSE) {
				if (VKey == VK_SCROLL) XOffState = GetKeyState(VK_SCROLL) & 1;
				else XOffState ^= TRUE;
				if (XOffState) NewStatus(1, L"Hold", NS_NORMAL);
				else {
					NewStatus(1, L"", NS_NORMAL);
					ReleaseXOff();
				}
			}
		#endif
		if (VKey == VK_CANCEL && GetKeyState(VK_CONTROL) < 0) {
			/*cancel button in search and replace dialogbox (?)*/
			#if defined(WIN32)
				ExecInterrupt(CTRL_BREAK_EVENT);
			#endif
			Interrupted	= TRUE;
			return;
		}
		if (Disabled && VKey != VK_NUMLOCK && VKey != VK_CAPITAL &&
						VKey != VK_SHIFT   && VKey != VK_CONTROL) {
			if (!EscapeInput && (VKey != 'C' || GetKeyState(VK_CONTROL) >= 0))
				MessageBeep(MB_OK);
				/*...a ctrl+c message beep will be generated later*/
			return;
		} else if (VKey=='C' || VKey=='Q' || VKey=='S') return;
	}
	GotKeydown(VKey);
}
