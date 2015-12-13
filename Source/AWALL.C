/***************************************************************************\
* Module Name: awall.c
\***************************************************************************/

#define REPS 8

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
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

static int near rand(void);

static ULONG near seed;

char far pascal _loadds animatename()
{
	return 'W';
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	init = initptr;
	return TRUE;
}

void far pascal _loadds animatechar(c)
char c;
{
	c;
}
	
void far pascal _loadds animatedblclk(mp)
MPARAM mp;
{
	mp;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Wallpaper animated desktop.\n\nBy John Ridges","ANIMATE",0,
		MB_OK|MB_NOICON|MB_DEFBUTTON1|MB_APPLMODAL);
}

void far pascal _loadds animatepaint(hps,rectup)
HPS hps;
RECTL far *rectup;
{
	hps;
	rectup;
}

void far pascal _loadds animateclose()
{
}

void far pascal _loadds animatethread()
{
	int i,size,x,y,xpos,ypos;
	long color;
	HAB hab;
	HPS hps;
	POINTL pnt;
	static long colors[] = { CLR_WHITE, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
		CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED,
		CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY,
		CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK, CLR_BLACK };

	hab = WinInitialize(0);
	DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
	WinFillRect(init->shadowhps,&init->screenrectl,SYSCLR_BACKGROUND);
	DosSemClear(&init->hpssemaphore);
	WinPostMsg(init->screenhwnd,WM_USER,0,0);
	seed = WinGetCurrentTime(hab);
	size = ((int)(init->screenrectl.xRight-init->screenrectl.xLeft>>1)+
		REPS-1)/REPS<<1;
	while (!init->closesemaphore) {
		x = (int)((long)rand()*size>>15);
		y = (int)((long)rand()*size>>15);
		color = colors[(long)rand()*(sizeof(colors)/sizeof(long))>>15];
		for (ypos = (int)init->screenrectl.yTop-size;
			ypos > (int)init->screenrectl.yBottom-size; ypos -= size) {
		wait:
			if (init->closesemaphore) break;
			if (!(init->screenvisible)()) goto wait;
			DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
			hps = WinGetPS(init->screenhwnd);
			GpiSetColor(hps,color);
			GpiSetColor(init->shadowhps,color);
			for (xpos = (int)init->screenrectl.xLeft; xpos <
				(int)init->screenrectl.xRight;
				xpos += size) for (i = 0; i < 4; i++) {
				if (i&1) pnt.x = xpos+size-x-1;
				else pnt.x = xpos+x;
				if (i&2) pnt.y = ypos+size-y-1;
				else pnt.y = ypos+y;
				GpiSetPel(hps,&pnt);
				GpiSetPel(init->shadowhps,&pnt);
				if (i&1) pnt.x = xpos+size-y-1;
				else pnt.x = xpos+y;
				if (i&2) pnt.y = ypos+size-x-1;
				else pnt.y = ypos+x;
				GpiSetPel(hps,&pnt);
				GpiSetPel(init->shadowhps,&pnt);
			}
			WinReleasePS(hps);
			DosSemClear(&init->hpssemaphore);
		}
	}
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init->closesemaphore);
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
