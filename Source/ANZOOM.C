/***************************************************************************\
* Module Name: anzoom.c
\***************************************************************************/

#define FIFOMAX 100
#define MAXSPEED 3277
#define COLORSPEED 1000
#define SHAPESPEED 6666
#define DELAYTIME 50

#define INCL_DOSPROCESS
#define INCL_GPILOGCOLORTABLE
#define INCL_WINSYS
#define INCL_WINTIMER

#include <os2.h>
#include <stdlib.h>

#define ONELINE 0
#define TWOLINES 1
#define BOX 2
#define STAR 3
#define TRIANGLE 4
#define DIAMOND 5
#define VEE 6
#define PLUS 7
#define HOURGLASS 8
#define CIRCLE 9

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

typedef struct {
	POINTL pt1;
	POINTL pt2;
	ULONG color;
	int shape;
} FIFO;

char far pascal zoomname(void);
BOOL far pascal zoominit(INITBLOCK far *);
void far pascal zoomchar(char);
void far pascal zoomclick(MPARAM);
void far pascal zoompaint(HPS, RECTL far *);
void far pascal zoomclose(void);
void far pascal zoomthread(void);
static void near plotshape(POINTL, POINTL, int, long, HPS);

extern INITBLOCK near init;
extern char near progname[];
extern PROFILEREC volatile near profile;

static long volatile near backcolor;
static long near deskcolor;
static SEL near fifosel;
static FIFO volatile far * near fifo;
static int volatile ffpnt = 0;
static long volatile ccol = CLR_RED;
static long col[] = { CLR_WHITE, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
	CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED, CLR_BLACK,
	CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY };

char far pascal zoomname()
{
	return 'Z';
}

BOOL far pascal zoominit(initptr)
INITBLOCK far *initptr;
{
	int i;
	HPS hps;

	initptr;
	hps = WinGetPS(init.screenhwnd);
	deskcolor = GpiQueryColorIndex(hps,0L,
		WinQuerySysColor(HWND_DESKTOP,SYSCLR_BACKGROUND,0L));
	if (profile.background < 0) backcolor = deskcolor;
	else backcolor = col[profile.background];
	WinReleasePS(hps);
	DosAllocSeg(sizeof(FIFO)*FIFOMAX,&fifosel,SEG_NONSHARED);
	fifo = MAKEP(fifosel,0);
	for (i = 0; i < profile.zoomlength; i++) fifo[i].pt1.x = -1;
	return FALSE;
}

void far pascal zoomchar(c)
char c;
{
	int i;
	HPS hps;

	switch(c) {
	case '+':
	case '=':
		if (profile.zoomlength+1 < FIFOMAX) {
			for (i = profile.zoomlength++; i > ffpnt; i--) fifo[i] = fifo[i-1];
			fifo[ffpnt].pt1.x = -1;
		}
		break;
	case '-':
	case '_':
		if (profile.zoomlength > 1) {
			if (fifo[ffpnt].pt1.x >= 0) {
				hps = WinGetPS(init.screenhwnd);
				plotshape(fifo[ffpnt].pt1,fifo[ffpnt].pt2,
					fifo[ffpnt].shape,backcolor,hps);
				WinReleasePS(hps);
			}
			profile.zoomlength--;
			for (i = ffpnt; i < profile.zoomlength; i++) fifo[i] = fifo[i+1];
			if (ffpnt >= profile.zoomlength) ffpnt = 0;
		}
		break;
	case 'B':
	case 'b':
		if (++profile.background >= 16) profile.background = -1;
		if (profile.background < 0) backcolor = deskcolor;
		else backcolor = col[profile.background];
		while (ccol == backcolor) ccol = col[rand()>>11];
		WinInvalidateRect(init.screenhwnd,NULL,FALSE);
		break;
	}
}
	
void far pascal zoomclick(mp)
MPARAM mp;
{
	mp;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Zooming figure animated desktop.\n\nWhen the desktop has the focus, "
		"use '+' to make the trail longer, '-' to make the trail shorter, and "
		"'B' to change the background color.\n\nBy John Ridges",progname,0,
		MB_OK|MB_NOICON|MB_DEFBUTTON1|MB_APPLMODAL);
}

void far pascal zoompaint(hps,rectup)
HPS hps;
RECTL far *rectup;
{
	int i;

	WinFillRect(hps,rectup,backcolor);
	for (i = 0; i < profile.zoomlength; i++) if (fifo[i].pt1.x >= 0)
		plotshape(fifo[i].pt1,fifo[i].pt2,fifo[i].shape,fifo[i].color,hps);
}

void far pascal zoomclose()
{
	DosFreeSeg(fifosel);
}

void far pascal zoomthread()
{
	int i;
	HAB hab;
	HPS hps;
	ULONG time;
	int count = 0;
	int scount = 0;
	int cshape = ONELINE;
	static POINTL mypt[2] = {-1, -1, -1, -1};
	static POINTL p[2] = {0, 0, 0, 0};

	hab = WinInitialize(0);
	WinPostMsg(init.screenhwnd,WM_USER,0,0);
	while (!init.closesemaphore) if ((init.screenvisible)()) {
		time = WinGetCurrentTime(hab);
		DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
		for (i = 0; i < 2; i++) {
			mypt[i].x += p[i].x;
			mypt[i].y += p[i].y;
			if (mypt[i].x < init.screenrectl.xLeft) {
				mypt[i].x = init.screenrectl.xLeft;
				p[i].x = 1+rand()/MAXSPEED;
			}
			else if (mypt[i].x >= init.screenrectl.xRight) {
				mypt[i].x = init.screenrectl.xRight-1;
				p[i].x = -1-rand()/MAXSPEED;
			}
			if (mypt[i].y < init.screenrectl.yBottom) {
				mypt[i].y = init.screenrectl.yBottom;
				p[i].y = 1+rand()/MAXSPEED;
			}
			else if (mypt[i].y >= init.screenrectl.yTop) {
				mypt[i].y = init.screenrectl.yTop-1;
				p[i].y = -1-rand()/MAXSPEED;
			}
		}
		hps = WinGetPS(init.screenhwnd);
		plotshape(mypt[0],mypt[1],cshape,ccol,hps);
		if (fifo[ffpnt].pt1.x >= 0)
			plotshape(fifo[ffpnt].pt1,fifo[ffpnt].pt2,fifo[ffpnt].shape,
				backcolor,hps);
		WinReleasePS(hps);
		fifo[ffpnt].pt1 = mypt[0];
		fifo[ffpnt].pt2 = mypt[1];
		fifo[ffpnt].color = ccol;
		fifo[ffpnt].shape = cshape;
		if (++ffpnt >= profile.zoomlength) ffpnt = 0;
		if (++count >= COLORSPEED) {
			count = 0;
			while ((ccol = col[rand()>>11]) == backcolor);
		}
		if (++scount >= SHAPESPEED ||
			(cshape == CIRCLE && scount >= SHAPESPEED/3)) {
			scount = 0;
			cshape = rand()/3450;
		}
		DosSemClear(&init.hpssemaphore);
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init.closesemaphore);
}

static void near plotshape(mypt1,mypt2,shape,color,hps)
POINTL mypt1;
POINTL mypt2;
int shape;
long color;
HPS hps;

{
	long radius_x,radius_y;
	POINTL tmppt1,tmppt2,tmppt3,tmppt4;

	GpiSetColor(hps,color);
	switch(shape) {
	case ONELINE:
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		break;
	case TWOLINES:
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		break;
	case BOX:
		GpiMove(hps,&mypt1);
		GpiBox(hps,DRO_OUTLINE,&mypt2,0L,0L);
		break;
	case CIRCLE:
		GpiMove(hps,&mypt1);
		GpiBox(hps,DRO_OUTLINE,&mypt2,
			labs(mypt2.x-mypt1.x),labs(mypt2.y-mypt1.y));
		break;
	case TRIANGLE:
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt1.x+mypt2.x>>1;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&mypt2);
		GpiLine(hps,&tmppt1);
		break;
	case DIAMOND:
		tmppt1.x = tmppt3.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt1.y;
		tmppt2.x = mypt1.x;
		tmppt2.y = tmppt4.y = mypt1.y+mypt2.y>>1;
		tmppt3.y = mypt2.y;
		tmppt4.x = mypt2.x;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&tmppt3);
		GpiLine(hps,&tmppt4);
		GpiLine(hps,&tmppt1);
		break;
	case VEE:
		tmppt1.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&mypt1);
		GpiLine(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		break;
	case PLUS:
		tmppt1.x = tmppt3.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt1.y;
		tmppt2.x = mypt1.x;
		tmppt2.y = tmppt4.y = mypt1.y+mypt2.y>>1;
		tmppt3.y = mypt2.y;
		tmppt4.x = mypt2.x;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt3);
		GpiMove(hps,&tmppt2);
		GpiLine(hps,&tmppt4);
		break;
	case HOURGLASS:
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		GpiLine(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&mypt1);
		break;
	case STAR:
		tmppt4.x = mypt1.x+mypt2.x>>1;
		tmppt4.y = mypt1.y+mypt2.y>>1;
		radius_x = mypt2.x-mypt1.x>>1;
		radius_y = mypt2.y-mypt1.y>>1;
		tmppt1.x = tmppt4.x;
		tmppt1.y = tmppt4.y+radius_y;
		GpiMove(hps,&tmppt1);
		tmppt2.x = tmppt4.x+(radius_x*-38521>>16);
		tmppt2.y = tmppt3.y = tmppt4.y+(radius_y*-53020>>16);
		GpiLine(hps,&tmppt2);
		tmppt2.x = tmppt4.x+(radius_x*62328>>16);
		tmppt2.y = tmppt4.y+(radius_y*20251>>16);
		GpiLine(hps,&tmppt2);
		tmppt2.x = tmppt4.x+(radius_x*-62328>>16);
		GpiLine(hps,&tmppt2);
		tmppt3.x = tmppt4.x+(radius_x*38521>>16);
		GpiLine(hps,&tmppt3);
		GpiLine(hps,&tmppt1);
		break;
	}
}
