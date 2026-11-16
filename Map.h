#ifndef WINVI_MAP_H
#define WINVI_MAP_H
/********************************************************\
|*                                                      *|
|*   Program:              WinVi                        *|
|*   Author:               Raphael Molle                *|
|*   Compiler:             several (w/ WIN32 support)   *|
|*   --------------------- --------------------------   *|
|*   © Copyright 1994-2002 by Raphael Molle, Berlin     *|
|*   © Copyleft by GPL     See file COPYING for more    *|
|*                         info about terms of use      *|
|*                                                      *|
\********************************************************/

/* revision log:
 *		3-Dec-2000	first publication of source code
 *     28-Dec-2002	implementation of mappings
 *     11-Jan-2003	prototypes for new mappings/abbreviations
 */

typedef struct tagMAP {
	struct tagMAP *Next;
	LPSTR		  Name, InputDisplay, Replace;
	LPDWORD		  InputMatch, ReplaceMap;
	DWORD		  Flags;
} *PMAP;

#define M_ABBREVIATION 1		/*entry is an abbreviation, otherwise...*/
#define M_INSERTMODE   2		/*map in insert or replace mode*/
#define M_COMMANDMODE  4		/*map in command mode (edit mode)*/
#define M_COMMANDLINE  8		/*map in ex command line mode*/

extern PMAP FirstMapping;

VOID FreeMappings(PMAP);
#define WV_MAP_TEXT_CAP (WV_TEXT_CAP*8+64)
#define WV_MAP_KEYDOWN 0x100UL
#define WV_MAP_CHAR 0x200UL
#define WV_MAP_CTRL 0x400UL
#define WV_MAP_SHIFT 0x800UL
#define WV_MAP_ALT 0x1000UL
#define WV_MAP_UNICODE 0x20000UL
size_t WvMapLength(const DWORD *);
DWORD WvMapChar(WCHAR);
BOOL WvMapPrepareChange(void);
BOOL TranslateMapping(PDWORD*, PCSTR);
BOOL IsMappingEqual(PDWORD,PDWORD);
BOOL WvExMapping(PWSTR,BOOL,BOOL,BOOL);
INT_PTR MacrosCallback(HWND, UINT, WPARAM, LPARAM);
VOID StopMap(BOOL);
BOOL IsKeyboardMapMsg(PMSG);
BOOL KeyboardMapDequeue(PMSG);
VOID WmChar(WPARAM);
VOID WmKeydown(WPARAM);

#endif
