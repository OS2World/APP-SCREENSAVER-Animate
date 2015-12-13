/***************************************************************************\
* Module Name: andia.c
\***************************************************************************/

#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WINSHELLDATA
#define INCL_WINSYS

#include <os2.h>

typedef struct {
	HAB animatehab;
	HPS shadowhps;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG hpssemaphore;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
	BOOL (far pascal _loadds *screenvisible)(void);
} INITBLOCK;

typedef struct {
	int firespeed;
	int starspeed;
	int zoomlength,background;
	BOOL enabled;
	unsigned int timeout;
} PROFILEREC;

MRESULT EXPENTRY _export animatedialog(HWND, USHORT, MPARAM, MPARAM);

extern char near animate[];
extern char near opt[];
extern BOOL near nodlg;
extern INITBLOCK near init;
extern PROFILEREC volatile near profile;

MRESULT EXPENTRY _export animatedialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	RECTL rect;

	switch(message) {
	case WM_INITDLG:
		WinQueryWindowRect(hwnd,&rect);
		WinSetWindowPos(hwnd,NULL,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN)-rect.xRight+rect.xLeft)/2,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN)-rect.yTop+rect.yBottom)/2,
			0,0,SWP_MOVE);
		if (profile.enabled) WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,
			MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.timeout,FALSE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			profile.enabled = 
				SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinQueryDlgItemShort(hwnd,4,(PSHORT)&profile.timeout,FALSE);
			WinWriteProfileData(init.animatehab,animate,opt,(PBYTE)&profile,
				sizeof(profile));
		}
		WinDestroyWindow(hwnd);
		nodlg = TRUE;
		return FALSE;
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

