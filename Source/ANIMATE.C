/***************************************************************************\
* Module Name: animate.c
\***************************************************************************/

#define NUMDESK 4

#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINMENUS
#define INCL_WINPOINTERS
#define INCL_WINTIMER
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <stdlib.h>
#include <string.h>

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
	char (far pascal *name)(void);
	BOOL (far pascal *init)(INITBLOCK far *);
	void (far pascal *keychar)(char);
	void (far pascal *dblclk)(MPARAM);
	void (far pascal *paint)(HPS, RECTL far *);
	void (far pascal *close)(void);
	void (far pascal *thread)(void);
} DESKTOPS;

typedef struct {
	int firespeed;
	int starspeed;
	int zoomlength,background;
	BOOL enabled;
	unsigned int timeout;
} PROFILEREC;

char far pascal zoomname(void);
BOOL far pascal zoominit(INITBLOCK far *);
void far pascal zoomchar(char);
void far pascal zoomclick(MPARAM);
void far pascal zoompaint(HPS, RECTL far *);
void far pascal zoomclose(void);
void far pascal zoomthread(void);
char far pascal firename(void);
BOOL far pascal fireinit(INITBLOCK far *);
void far pascal firechar(char);
void far pascal fireclick(MPARAM);
void far pascal firepaint(HPS, RECTL far *);
void far pascal fireclose(void);
void far pascal firethread(void);
char far pascal spacename(void);
BOOL far pascal spaceinit(INITBLOCK far *);
void far pascal spacechar(char);
void far pascal spaceclick(MPARAM);
void far pascal spacepaint(HPS, RECTL far *);
void far pascal spaceclose(void);
void far pascal spacethread(void);
char far pascal wallname(void);
BOOL far pascal wallinit(INITBLOCK far *);
void far pascal wallchar(char);
void far pascal wallclick(MPARAM);
void far pascal wallpaint(HPS, RECTL far *);
void far pascal wallclose(void);
void far pascal wallthread(void);
MRESULT EXPENTRY _export animatedialog(HWND, USHORT, MPARAM, MPARAM);

void cdecl main(int, char **);
MRESULT EXPENTRY _export animateproc(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export animateframeproc(HWND, USHORT, MPARAM, MPARAM);
static BOOL far pascal _loadds screenvisible(void);
void cdecl _setenvp(void);

static DESKTOPS desktop[NUMDESK+1] = {
	{ zoomname, zoominit, zoomchar, zoomclick, zoompaint, zoomclose, zoomthread },
	{ firename, fireinit, firechar, fireclick, firepaint, fireclose, firethread },
	{ spacename, spaceinit, spacechar, spaceclick, spacepaint, spaceclose, spacethread },
	{ wallname, wallinit, wallchar, wallclick, wallpaint, wallclose, wallthread },
};

static ULONG near timedown;
static int volatile far * near dllstatus;
static PFNWP near pfnwp;
static HWND near framehndl;
static TID near threadid;
static BOOL near savebits;
static HMODULE near module;
static PROFILEREC near temppro;
static int desktype = 0;
static long pause = 0;
static BOOL pausing = FALSE;
static BOOL bideing = TRUE;
static BOOL visible = FALSE;
static HWND behind = HWND_BOTTOM;
static BOOL saveractive = FALSE;

char animate[] = "Animate";
char opt[] = "Options";
BOOL nodlg = TRUE;
INITBLOCK init = { NULL, NULL, NULL, { 0, 0, 0, 0 }, 0, 0, 0, screenvisible };
char progname[] = "ANIMATE";
PROFILEREC volatile profile = { 25, 900, 30, -1, FALSE, 300 };

void cdecl main(argc,argv)
int argc;
char **argv;

{
	char name;
	int volatile far * (far pascal *installhook)(HAB, HWND);
	void (far pascal *releasehook)(void);
	HMQ hmsgq;
	QMSG qmsg;
	USHORT i,numfind;
	BOOL match;
	HSWITCH hsw;
	DATETIME date;
	HDC hdcmem;
	HBITMAP hbmmem;
	ULONG aldata[2];
	SIZEL sizl;
	DEVOPENSTRUC dop;
	BITMAPINFOHEADER bmi;
	DESKTOPS tempdesk;
	USHORT err = 0;
	ULONG fcfval = 0;
	HDIR hdir = HDIR_SYSTEM;
	static SWCNTRL swctl = {0, 0, 0, 0, 0, SWL_VISIBLE,
		SWL_NOTJUMPABLE, "Animated Desktop", 0};
	static union {
		char stack[4096];
		struct {
			char str1[128];
			char str2[128];
			char str3[128];
			char str4[128];
			FILEFINDBUF findbuf;
		} a;
	} near temp;

	init.animatehab = WinInitialize(0);
	hmsgq = WinCreateMsgQueue(init.animatehab,DEFAULT_QUEUE_SIZE);

	DosGetDateTime(&date);
	srand((unsigned int)date.seconds*100+date.hundredths);
	for (i = 0; i < 17; i++) rand();
	i = sizeof(profile);
	WinQueryProfileData(init.animatehab,animate,opt,(PBYTE)&profile,&i);
	temppro = profile;
	timedown = profile.timeout;

	strcpy(temp.a.str1,argv[0]);
	strrchr(temp.a.str1,'\\')[1] = 0;
	strcpy(temp.a.str2,temp.a.str1);
	strcat(temp.a.str2,"*.ANI");
	if (argc > 1) {
		name = argv[1][(long)rand()*strlen(argv[1])>>15];
		if (name >= 'a' && name <= 'z') name -= 32;
	}
	else name = 0;
	for (i = 1; !err; i++) {
		numfind = 1;
		if (i > NUMDESK) {
			strcpy(temp.a.str3,temp.a.str1);
			strcat(temp.a.str3,temp.a.findbuf.achName);
			if (!DosLoadModule(temp.a.str4,128,temp.a.str3,&module)) {
				DosGetProcAddr(module,(PSZ)1,(PPFN)&tempdesk.name);
				DosGetProcAddr(module,(PSZ)2,(PPFN)&tempdesk.init);
				DosGetProcAddr(module,(PSZ)3,&tempdesk.keychar);
				DosGetProcAddr(module,(PSZ)4,&tempdesk.dblclk);
				DosGetProcAddr(module,(PSZ)5,&tempdesk.paint);
				DosGetProcAddr(module,(PSZ)6,&tempdesk.close);
				DosGetProcAddr(module,(PSZ)7,&tempdesk.thread);
			}
			else goto abort;
			match = name == (tempdesk.name)();
			if (match || !((long)rand()*i>>15)) {
				desktype = NUMDESK;
				if (init.thismodule) DosFreeModule(init.thismodule);
				init.thismodule = module;
				desktop[NUMDESK] = tempdesk;
				if (match) break;
			}
		}
		else {
			match = name == (desktop[i-1].name)();
			if (match || !((long)rand()*i>>15)) desktype = i-1;
			if (match) break;
		}
		if (i >= NUMDESK) {
			if (i == NUMDESK) err = DosFindFirst(temp.a.str2,&hdir,FILE_NORMAL,
				&temp.a.findbuf,sizeof(FILEFINDBUF),&numfind,0L);
			else {
			abort:
				err = DosFindNext(hdir,&temp.a.findbuf,sizeof(FILEFINDBUF),
					&numfind);
			}
		}
	}
	if (!DosLoadModule(temp.a.str4,128,"deskpic",&module) ||
		!DosLoadModule(temp.a.str4,128,"animate",&module)) {
		DosGetProcAddr(module,(PSZ)1,(PPFN)&installhook);
		DosGetProcAddr(module,(PSZ)2,&releasehook);
	}
	else module = 0;
	WinRegisterClass(init.animatehab,progname,animateproc,0L,0);
	framehndl = WinCreateStdWindow(HWND_DESKTOP,0L,&fcfval,progname,
		"",0L,0,255,&init.screenhwnd);
	if (!module || (dllstatus = (installhook)(init.animatehab,init.screenhwnd))) {
		pfnwp = WinSubclassWindow(framehndl,animateframeproc);
		WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,
			(int)WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN),
			(int)WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN),
			SWP_SIZE|SWP_MOVE|SWP_SHOW|SWP_ZORDER);
		WinQueryWindowRect(init.screenhwnd,&init.screenrectl);

		swctl.hwnd = framehndl;
		WinQueryWindowProcess(framehndl,&swctl.idProcess,NULL);
		hsw = WinAddSwitchEntry(&swctl);

		savebits = (desktop[desktype].init)(&init);
		if (savebits) {
			dop.pszLogAddress = NULL;
			dop.pszDriverName = "DISPLAY";
			dop.pdriv = NULL;
			dop.pszDataType = NULL;
			hdcmem = DevOpenDC(init.animatehab,OD_MEMORY,"*",4L,
				(PDEVOPENDATA)&dop,NULL);
			sizl.cx = sizl.cy = 0;
			init.shadowhps = GpiCreatePS(init.animatehab,hdcmem,&sizl,
				PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
			GpiQueryDeviceBitmapFormats(init.shadowhps,2L,aldata);
			GpiQueryPS(init.shadowhps,&sizl);
			bmi.cbFix = sizeof(BITMAPINFOHEADER);
			bmi.cx = (SHORT)sizl.cx;
			bmi.cy = (SHORT)sizl.cy;
			bmi.cPlanes = (USHORT)aldata[0];
			bmi.cBitCount = (USHORT)aldata[1];
			hbmmem = GpiCreateBitmap(init.shadowhps,&bmi,0L,NULL,NULL);
			GpiSetBitmap(init.shadowhps,hbmmem);
		}

		DosCreateThread((PFNTHREAD)desktop[desktype].thread,&threadid,temp.stack+4094);
		DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,threadid);
		WinStartTimer(init.animatehab,init.screenhwnd,0,1000);

		while(WinGetMsg(init.animatehab,&qmsg,NULL,0,0))
			WinDispatchMsg(init.animatehab,&qmsg);

		WinStopTimer(init.animatehab,init.screenhwnd,0);
		if (saveractive) WinShowPointer(HWND_DESKTOP,TRUE);
		DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,threadid);
		DosSemSet((HSEM)&init.closesemaphore);
		DosSemClear(&pause);
		DosSemWait((HSEM)&init.closesemaphore,SEM_INDEFINITE_WAIT);
		if (savebits) {
			GpiSetBitmap(init.shadowhps,NULL);
			GpiDeleteBitmap(hbmmem);
			GpiDestroyPS(init.shadowhps);
			DevCloseDC(hdcmem);
		}
		(desktop[desktype].close)();

		WinRemoveSwitchEntry(hsw);
		if (memcmp((char *)&profile,&temppro,sizeof(profile)))
			WinWriteProfileData(init.animatehab,animate,opt,(PBYTE)&profile,sizeof(profile));
		if (module) {
			(releasehook)();
			DosFreeModule(module);
		}
	}
	else WinMessageBox(HWND_DESKTOP,framehndl,
		"ANIMATE is already running.","Whoops!",0,MB_CANCEL|MB_ICONHAND);
	WinDestroyWindow(framehndl);
	if (init.thismodule) DosFreeModule(init.thismodule);
	WinDestroyMsgQueue(hmsgq);
	WinTerminate(init.animatehab);
}

MRESULT EXPENTRY _export animateproc(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	HPS hps;
	RECTL rectup;
	POINTL aptl[3];
	static int mouse = 0;
	static BOOL paintready = FALSE;

	switch(message) {
	case WM_PAINT:
		if (!saveractive) WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
		DosSemClear(&pause);
		DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,threadid);
		DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinBeginPaint(hwnd,NULL,&rectup);
		if (paintready) {
			if (savebits) {
				aptl[0].x = aptl[2].x = rectup.xLeft;
				aptl[0].y = aptl[2].y = rectup.yBottom;
				aptl[1].x = rectup.xRight;
				aptl[1].y = rectup.yTop;
				GpiBitBlt(hps,init.shadowhps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
			}
			else (desktop[desktype].paint)(hps,&rectup);
			if (bideing) {
				bideing = FALSE;
				DosSemClear(&pause);
			}
		}
		WinEndPaint(hps);
		DosSemClear(&init.hpssemaphore);
		DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,threadid);
		break;
	case WM_CHAR:
		if (!(SHORT1FROMMP(mp1)&KC_KEYUP)) {
			if ((SHORT2FROMMP(mp2) == VK_F4 &&
				(SHORT1FROMMP(mp1)&(KC_SHIFT|KC_ALT|KC_CTRL)) == KC_ALT) ||
				(SHORT2FROMMP(mp2) == VK_F3 &&
				!(SHORT1FROMMP(mp1)&(KC_SHIFT|KC_ALT|KC_CTRL))))
				WinSendMsg(hwnd,WM_CLOSE,0,0);
			else if (SHORT1FROMMP(mp1)&KC_CHAR) switch((char)SHORT1FROMMP(mp2)) {
			case 'F':
			case 'f':
				mouse = 3;
				if (behind == HWND_TOP) behind = HWND_BOTTOM;
				else behind = HWND_TOP;
				WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
				break;
			case 'P':
			case 'p':
				pausing = !pausing;
				DosSemClear(&pause);
				break;
			case 'S':
			case 's':
				if (nodlg && module) {
					nodlg = FALSE;
					WinLoadDlg(HWND_DESKTOP,
						WinQueryDesktopWindow(init.animatehab,NULL),
						animatedialog,0,666,NULL);
				}
				break;
			default:
				DosSemClear(&pause);
				DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,threadid);
				DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
				(desktop[desktype].keychar)((char)SHORT1FROMMP(mp2));
				DosSemClear(&init.hpssemaphore);
				DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,threadid);
				break;
			}
			return MRFROMSHORT(TRUE);
		}
		break;
	case WM_USER:
		paintready = TRUE;
		WinInvalidateRect(init.screenhwnd,NULL,FALSE);
		break;
	case WM_SEM1:
		if (saveractive) {
			saveractive = FALSE;
			WinShowPointer(HWND_DESKTOP,TRUE);
			WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
		}
		break;
	case WM_TIMER:
		if (!saveractive && behind == HWND_BOTTOM &&
			framehndl != WinQueryWindow(HWND_DESKTOP,QW_BOTTOM,FALSE))
			WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,SWP_ZORDER);
		if (profile.enabled && !saveractive && visible && !pausing &&
			!bideing && module) {
			if (!*dllstatus) {
				*dllstatus = 1;
				timedown = profile.timeout;
				break;
			}
			if (--timedown) break;
			saveractive = TRUE;
			WinShowPointer(HWND_DESKTOP,FALSE);
			if (behind != HWND_TOP)
				WinSetWindowPos(framehndl,HWND_TOP,0,0,0,0,SWP_ZORDER);
			*dllstatus = 5;
		}
		break;
	case WM_BUTTON1DBLCLK:
		(desktop[desktype].dblclk)(mp1);
		return MRFROMSHORT(TRUE);
	case WM_BUTTON2DBLCLK:
	case WM_BUTTON3DBLCLK:
	case WM_BUTTON2DOWN:
	case WM_BUTTON2UP:
	case WM_BUTTON3DOWN:
	case WM_BUTTON3UP:
		return WinSendMsg(WinQueryWindow(framehndl,QW_PARENT,FALSE),
			message,mp1,mp2);
	case WM_FOCUSCHANGE:
		if (!SHORT1FROMMP(mp2) && memcmp((char *)&profile,&temppro,sizeof(profile))) {
			WinWriteProfileData(init.animatehab,animate,opt,(PBYTE)&profile,sizeof(profile));
			temppro = profile;
		}
	case WM_BUTTON1DOWN:
		mouse = 0;
	case WM_MOUSEMOVE:
		if (mouse) mouse--;
		if (behind != HWND_BOTTOM && !mouse) {
			behind = HWND_BOTTOM;
			if (!saveractive) WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,SWP_ZORDER);
		}
	default:
		return WinDefWindowProc(hwnd,message,mp1,mp2);
	}
	return FALSE;
}

MRESULT EXPENTRY _export animateframeproc(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	if (message == WM_SYSCOMMAND && SHORT1FROMMP(mp1) == SC_CLOSE)
		WinSendMsg(init.screenhwnd,WM_CLOSE,0,0);
	else if (!saveractive && message == WM_ADJUSTWINDOWPOS && behind == HWND_BOTTOM) {
  		((PSWP)mp1)->fs |= SWP_ZORDER;
 		((PSWP)mp1)->hwndInsertBehind = HWND_BOTTOM;
	}
	return (pfnwp)(hwnd,message,mp1,mp2);
}

static BOOL far pascal _loadds screenvisible()
{
	HPS hps;
	POINTL pnt;

	pnt.x = pnt.y = 0;
	hps = WinGetScreenPS(HWND_DESKTOP);
	visible = (GpiPtVisible(hps,&pnt) == PVIS_VISIBLE);
	WinReleasePS(hps);
	if (!visible || pausing || bideing) {
		DosSemSetWait(&pause,1000L);
		return FALSE;
	}
	return TRUE;
}

void cdecl _setenvp()
{
}
