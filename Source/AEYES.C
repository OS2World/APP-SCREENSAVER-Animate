/***************************************************************************\
* AEYES.C - A very silly animated desktop extension by John Ridges
\***************************************************************************/

#define MAXEYES 25
#define SPEED 3
#define VIEWPOINT 25

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPATHS
#define INCL_GPIPRIMITIVES
#define INCL_WINPOINTERS
#define INCL_WINRECTANGLES
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINTIMER

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

char far pascal _loadds animatename(void);
BOOL far pascal _loadds animateinit(INITBLOCK far *);
void far pascal _loadds animatechar(char);
void far pascal _loadds animatedblclk(MPARAM);
void far pascal _loadds animatepaint(HPS, RECTL far *);
void far pascal _loadds animateclose(void);
void far pascal _loadds animatethread(void);

static INITBLOCK far * near init;

typedef struct {
	int background;
	int volatile numeyes;
} PROFILEREC;

typedef struct {
	FIXED xvelocity,yvelocity;
	FIXED xposition,yposition;
	HDC hdc;
	HPS hps;
	HBITMAP hbm,eyemap;
	BOOL painted;
	BOOL volatile repaint;
	BOOL volatile collide[MAXEYES];
} BITMAPREC;

static ULONG sqr(ULONG);
static int near rand(void);

static long near deskcolor;
static long volatile near backcolor;
static SEL near bitmapsel;
static ULONG near seed;
static BITMAPREC far * near bitmaps;
static char name[] = "Animate";
static char opt[] = "Eyes";
static BOOL dirty = FALSE;
static PROFILEREC profile = { -1, 5 };
static long col[] = { CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
	CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED, CLR_BLACK,
	CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY };

char far pascal _loadds animatename()
{
	return 'E';
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	int i,j;
	HPS hps;

	init = initptr;
	i = sizeof(profile);
	WinQueryProfileData(init->animatehab,name,opt,&profile,&i);
	hps = WinGetPS(init->screenhwnd);
	deskcolor = GpiQueryColorIndex(hps,0L,
		WinQuerySysColor(HWND_DESKTOP,SYSCLR_BACKGROUND,0L));
	if (profile.background < 0) backcolor = deskcolor;
	else backcolor = col[profile.background];
	WinReleasePS(hps);
	DosAllocSeg(sizeof(BITMAPREC)*MAXEYES,&bitmapsel,SEG_NONSHARED);
	bitmaps = MAKEP(bitmapsel,0);
	for (i = 0; i < MAXEYES; i++) {
		bitmaps[i].xposition = -1L;
		bitmaps[i].repaint = TRUE;
		for (j = 0; j < MAXEYES; j++) bitmaps[i].collide[j] = FALSE;
	}
	return FALSE;
}

void far pascal _loadds animatechar(c)
char c;
{
	int i;
	HPS hps;
	RECTL rect;

	switch (c) {
	case 'B':
	case 'b':
		if (++profile.background >= sizeof(col)/sizeof(col[0]))
			profile.background = -1;
		if (profile.background < 0) backcolor = deskcolor;
		else backcolor = col[profile.background];
		for (i = 0; i < MAXEYES; i++) bitmaps[i].repaint = TRUE;
		WinInvalidateRect(init->screenhwnd,NULL,FALSE);
		break;
	case '+':
	case '=':
		if (profile.numeyes < MAXEYES-1) profile.numeyes++;
		break;
	case '-':
	case '_':
		if (profile.numeyes > 1) {
			profile.numeyes--;
			hps = WinGetPS(init->screenhwnd);
			WinSetRect(init->animatehab,&rect,
				FIXEDINT(bitmaps[profile.numeyes].xposition)-1,
				FIXEDINT(bitmaps[profile.numeyes].yposition)-1,
				FIXEDINT(bitmaps[profile.numeyes].xposition)+33,
				FIXEDINT(bitmaps[profile.numeyes].yposition)+33);
			WinFillRect(hps,&rect,backcolor);
			WinReleasePS(hps);
		}
	}
	dirty = TRUE;
}
	
void far pascal _loadds animatedblclk(mp)
MPARAM mp;
{
	mp;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Eyes animated desktop.\n\nWhen the desktop has the focus, use "
		"'+' for more eyes, '-' for less, and "
		"'B' to change the background color.\n\nBy John Ridges","ANIMATE",0,
		MB_OK|MB_NOICON|MB_DEFBUTTON1|MB_APPLMODAL);
}

void far pascal _loadds animatepaint(hps,rectup)
HPS hps;
RECTL far *rectup;
{
	WinFillRect(hps,rectup,backcolor);
}

void far pascal _loadds animateclose()
{
	DosFreeSeg(bitmapsel);
	if (dirty) WinWriteProfileData(init->animatehab,name,opt,
		&profile,sizeof(profile));
}

void far pascal _loadds animatethread()
{
	HAB hab;
	HPS hps;
	int i,j;
	long x,y,d;
	FIXED temp,tempx,tempy;
	SIZEL sizl;
	ULONG aldata[2];
	POINTL pnt,mouse,aptl[4];
	RECTL rectup;
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;
	BOOL flip = FALSE;
	BOOL firsttime = TRUE;
	static ARCPARAMS arc = { 7, 15, 0, 0 };

	hab = WinInitialize(0);
	WinPostMsg(init->screenhwnd,WM_USER,0,0);
	seed = WinGetCurrentTime(hab);
	while (!init->closesemaphore) if ((init->screenvisible)()) {
		WinQueryPointerPos(HWND_DESKTOP,&mouse);
		DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinGetPS(init->screenhwnd);
		for (i = 0; i < profile.numeyes; i++) {
			if (bitmaps[i].xposition < 0) {
				dop.pszLogAddress = NULL;
				dop.pszDriverName = "DISPLAY";
				dop.pdriv = NULL;
				dop.pszDataType = NULL;
				bitmaps[i].hdc = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,
					NULL);
				sizl.cx = sizl.cy = 0;
				bitmaps[i].hps = GpiCreatePS(hab,bitmaps[i].hdc,&sizl,
					PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
				GpiQueryDeviceBitmapFormats(bitmaps[i].hps,2L,aldata);
				bmi.cbFix = sizeof(BITMAPINFOHEADER);
				bmi.cx = 48+(SPEED<<1);
				bmi.cy = 32+(SPEED<<1);
				bmi.cPlanes = (USHORT)aldata[0];
				bmi.cBitCount = (USHORT)aldata[1];
				bitmaps[i].hbm = GpiCreateBitmap(bitmaps[i].hps,&bmi,0L,NULL,NULL);
				GpiSetBitmap(bitmaps[i].hps,bitmaps[i].hbm);
				bitmaps[i].eyemap = GpiLoadBitmap(bitmaps[i].hps,
					init->thismodule,1,0L,0L);
				GpiSetArcParams(bitmaps[i].hps,&arc);
			}
			if (bitmaps[i].repaint) {
				GpiSetClipPath(bitmaps[i].hps,0L,SCP_RESET);
				WinSetRect(hab,&rectup,0,0,48+(SPEED<<1),32+(SPEED<<1));
				WinFillRect(bitmaps[i].hps,&rectup,backcolor);
				GpiBeginPath(bitmaps[i].hps,1L);
				pnt.y = 15+SPEED;
				for (j = 23; j < 55; j += 16) {
					pnt.x = j+SPEED;
					GpiMove(bitmaps[i].hps,&pnt);
					GpiFullArc(bitmaps[i].hps,DRO_OUTLINE,MAKEFIXED(1,0));
				}
				GpiEndPath(bitmaps[i].hps);
				GpiSetClipPath(bitmaps[i].hps,1L,SCP_ALTERNATE|SCP_AND);
			}
			if (bitmaps[i].xposition < 0) {
				tempx = (long)rand()<<1;
				bitmaps[i].xvelocity = (rand()&1 ? tempx : -tempx)*SPEED;
				tempx = MAKEFIXED(0,65535)-((ULONG)tempx*(ULONG)tempx>>16)-1;
				tempy = tempx<<16;
				bitmaps[i].yvelocity = (rand()&1 ? sqr(tempy) : -sqr(tempy))*
					SPEED;
				bitmaps[i].xposition = MAKEFIXED(init->screenrectl.xLeft,0)+(rand()*
					(init->screenrectl.xRight-init->screenrectl.xLeft-32)<<1);
				bitmaps[i].yposition = MAKEFIXED(init->screenrectl.yBottom,0)+(rand()*
					(init->screenrectl.yTop-init->screenrectl.yBottom-32)<<1);
				bitmaps[i].painted = flip = !flip;
			}
			else {
				tempx = bitmaps[i].xposition+bitmaps[i].xvelocity;
				tempy = bitmaps[i].yposition+bitmaps[i].yvelocity;
				for (j = i+1; j < profile.numeyes; j++)
					if (tempy+MAKEFIXED(32+SPEED,0) > bitmaps[j].yposition &&
					tempy < bitmaps[j].yposition+MAKEFIXED(32+SPEED,0) &&
					tempx < bitmaps[j].xposition+MAKEFIXED(32+SPEED,0) &&
					tempx+MAKEFIXED(32+SPEED,0) > bitmaps[j].xposition) {
					if (!bitmaps[i].collide[j]) {
						bitmaps[i].collide[j] = TRUE;
						temp = bitmaps[i].xvelocity;
						bitmaps[i].xvelocity = bitmaps[j].xvelocity;
						bitmaps[j].xvelocity = temp;
						temp = bitmaps[i].yvelocity;
						bitmaps[i].yvelocity = bitmaps[j].yvelocity;
						bitmaps[j].yvelocity = temp;
					}
				}
				else bitmaps[i].collide[j] = FALSE;
				if (FIXEDINT(tempx) < (int)init->screenrectl.xLeft) {
					tempx = (init->screenrectl.xLeft<<17)-tempx;
					bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
				}
				else if (FIXEDINT(tempx) >= (int)init->screenrectl.xRight-32) {
					tempx = (init->screenrectl.xRight-32<<17)-tempx;
					bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
				}
				if (FIXEDINT(tempy) < (int)init->screenrectl.yBottom) {
					tempy = (init->screenrectl.yBottom<<17)-tempy;
					bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
				}
				else if (FIXEDINT(tempy) >= (int)init->screenrectl.yTop-32) {
					tempy = (init->screenrectl.yTop-32<<17)-tempy;
					bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
				}
				if (bitmaps[i].painted || bitmaps[i].repaint || firsttime)
					for (j = 16; j < 48; j += 16) {
					x = FIXEDINT(tempx)-9+j-mouse.x;
					y = FIXEDINT(tempy)+15-mouse.y;
					d = sqr(VIEWPOINT*VIEWPOINT+x*x+y*y);
					x = (x*7+(d>>1))/d;
					y = (y*15+(d>>1))/d;
					aptl[0].x = j+SPEED;
					aptl[0].y = SPEED;
					aptl[1].x = j+SPEED+15;
					aptl[1].y = SPEED+31;
					aptl[2].x = x+8;
					aptl[2].y = y+16;
					aptl[3].x = x+24;
					aptl[3].y = y+48;
					GpiWCBitBlt(bitmaps[i].hps,bitmaps[i].eyemap,4L,aptl,
						ROP_SRCCOPY,BBO_IGNORE);
				}
				bitmaps[i].painted = !bitmaps[i].painted;
				aptl[0].x = FIXEDINT(tempx)-SPEED;
				aptl[0].y = FIXEDINT(tempy)-SPEED;
				aptl[1].x = FIXEDINT(tempx)+32+SPEED;
				aptl[1].y = FIXEDINT(tempy)+32+SPEED;
				aptl[2].x = 16;
				aptl[2].y = 0;
				GpiBitBlt(hps,bitmaps[i].hps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
				bitmaps[i].xposition = tempx;
				bitmaps[i].yposition = tempy;
			}
			bitmaps[i].repaint = FALSE;
		}
		DosSemClear(&init->hpssemaphore);
		WinReleasePS(hps);
		firsttime = FALSE;
	}
	for (i = 0; i < MAXEYES; i++) if (bitmaps[i].xposition >= 0) {
		GpiDeleteBitmap(bitmaps[i].eyemap);
		GpiSetBitmap(bitmaps[i].hps,NULL);
		GpiDeleteBitmap(bitmaps[i].hbm);
		GpiDestroyPS(bitmaps[i].hps);
		DevCloseDC(bitmaps[i].hdc);
	}
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init->closesemaphore);
}

static ULONG sqr(arg)
ULONG arg;
{
	ULONG sqrt,temp,range;

	sqrt = 0;
	temp = 1L<<15;
	for (range = 1L<<30; range > arg; range >>= 2) temp >>= 1;
	while (temp) {
		sqrt ^= temp;
		if (sqrt*sqrt > arg) sqrt ^= temp;
		temp >>= 1;
	}
	return sqrt;
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
