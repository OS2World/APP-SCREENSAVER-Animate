/***************************************************************************\
* Module Name: anspace.c
\***************************************************************************/

#define MINSPEED 300
#define MAXSPEED 2000
#define EYEPOINT 512
#define VANISHING 45000
#define NEWSTARS 3

#define INCL_DOSPROCESS
#define INCL_GPIPRIMITIVES
#define INCL_GPIBITMAPS

#include <os2.h>
#include <stdlib.h>

#define MAXSTARS ((VANISHING/MINSPEED)*NEWSTARS/2)

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
	long x;
	long y;
	long z;
	long color;
	POINTL oldpoint;
} STAR;

FIXED far pascal FixMul(FIXED, FIXED);
char far pascal spacename(void);
BOOL far pascal spaceinit(INITBLOCK far *);
void far pascal spacechar(char);
void far pascal spaceclick(MPARAM);
void far pascal spacepaint(HPS, RECTL far *);
void far pascal spaceclose(void);
void far pascal spacethread(void);

extern INITBLOCK near init;
extern char near progname[];
extern PROFILEREC volatile near profile;

static SEL near starsel;
static STAR volatile far * near star;

char far pascal spacename()
{
	return 'S';
}

BOOL far pascal spaceinit(initptr)
INITBLOCK far *initptr;
{
	int i;

	initptr;
	DosAllocSeg((USHORT)(sizeof(STAR)*MAXSTARS),&starsel,SEG_NONSHARED);
	star = MAKEP(starsel,0);
	for (i = 0; i < MAXSTARS; i++) star[i].z = -1;
	return FALSE;
}

void far pascal spacechar(c)
char c;
{
	switch(c) {
	case '+':
	case '=':
		if (profile.starspeed < MAXSPEED) profile.starspeed += 50;
		break;
	case '-':
	case '_':
		if (profile.starspeed > MINSPEED) profile.starspeed -= 50;
		break;
	}
}
	
void far pascal spaceclick(mp)
MPARAM mp;
{
	mp;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Spaceflight animated desktop.\n\nWhen the desktop has the focus, "
		"use '+' to speed up and '-' to slow down.\n\nBy John Ridges",
		progname,0,MB_OK|MB_NOICON|MB_DEFBUTTON1|MB_APPLMODAL);
}

void far pascal spacepaint(hps,rectup)
HPS hps;
RECTL far *rectup;
{
	int i;

	WinFillRect(hps,rectup,CLR_BLACK);
	GpiSetMix(hps,FM_XOR);
	for (i = 0; i < MAXSTARS; i++) if (star[i].z > 0) {
		GpiSetColor(hps,star[i].color);
		GpiSetPel(hps,(PPOINTL)&star[i].oldpoint);
	}
}

void far pascal spaceclose()
{
	DosFreeSeg(starsel);
}

void far pascal spacethread()

{
	int i,extra;
	long xsize,ysize,xcenter,ycenter,x,y;
	HAB hab;
	HPS hps;

	hab = WinInitialize(0);
	WinPostMsg(init.screenhwnd,WM_USER,0,0);
	xcenter = init.screenrectl.xRight+init.screenrectl.xLeft>>1;
	ycenter = init.screenrectl.yTop+init.screenrectl.yBottom>>1;
	xsize = ((init.screenrectl.xRight-init.screenrectl.xLeft>>1)+1)*
		VANISHING/EYEPOINT;
	ysize = ((init.screenrectl.yTop-init.screenrectl.yBottom>>1)+1)*
		VANISHING/EYEPOINT;
	extra = 0;
	while (!init.closesemaphore) if ((init.screenvisible)()) {
		extra += NEWSTARS;
		DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinGetPS(init.screenhwnd);
		GpiSetMix(hps,FM_XOR);
		GpiSetColor(hps,CLR_WHITE);
		for (i = 0; i < MAXSTARS; i++) {
			if (star[i].z > 0) {
				if (star[i].color != CLR_WHITE) GpiSetColor(hps,star[i].color);
				GpiSetPel(hps,(PPOINTL)&star[i].oldpoint);
				star[i].z -= profile.starspeed;
				if (star[i].z > 0) {
					x = xcenter+(EYEPOINT*star[i].x)/star[i].z;
					y = ycenter+(EYEPOINT*star[i].y)/star[i].z;
					if (x > init.screenrectl.xRight || x < init.screenrectl.xLeft ||
						y > init.screenrectl.yTop || y < init.screenrectl.yBottom)
						star[i].z = -1;
					else {
						star[i].oldpoint.x = x;
						star[i].oldpoint.y = y;
						GpiSetPel(hps,(PPOINTL)&star[i].oldpoint);
					}
				}
				if (star[i].color != CLR_WHITE) GpiSetColor(hps,CLR_WHITE);
			}
			if (extra && star[i].z <= 0) {
				extra--;
				star[i].z = VANISHING+EYEPOINT;
				star[i].x =	FixMul((FIXED)xsize<<1,(FIXED)rand()<<1)-xsize;
				star[i].y =	FixMul((FIXED)ysize<<1,(FIXED)rand()<<1)-ysize;
				if (rand()&63) star[i].color = CLR_WHITE;
				else star[i].color = rand()&1 ? CLR_CYAN : CLR_RED;
				star[i].oldpoint.x = star[i].oldpoint.y = -1;
			}
		}
		WinReleasePS(hps);
		DosSemClear(&init.hpssemaphore);
	}
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init.closesemaphore);
}
