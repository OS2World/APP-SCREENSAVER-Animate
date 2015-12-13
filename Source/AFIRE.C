/***************************************************************************\
* Module Name: afire.c
\***************************************************************************/

#define MAXSPEED 150

#define INCL_DOSPROCESS
#define INCL_WINSHELLDATA
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
	int xc,yc,xv,yv;
	int xm,ym,xvm,yvm;
	long c,cm;
} SPARK;

FIXED far pascal FixMul(FIXED, FIXED);
FIXED far pascal FixDiv(FIXED, FIXED);
static int near intrnd(int);
static FIXED near fixrnd(FIXED);
static FIXED near fixsqr(int);
static FIXED near fixsin(FIXED);
static FIXED near fixatn(FIXED);
static int near rand(void);

static ULONG near seed;
static SEL near sparksel;
static SPARK far * near spark;
static int volatile firespeed = 25;
static char name[] = "Animate";
static char opt[] = "Fireworks";
static BOOL dirty = FALSE;

char far pascal _loadds animatename()
{
	return 'F';
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	int i;

	init = initptr;
	i = sizeof(firespeed);
	WinQueryProfileData(init->animatehab,name,opt,(PBYTE)&firespeed,&i);
	DosAllocSeg(sizeof(SPARK)*MAXSPEED,&sparksel,SEG_NONSHARED);
	spark = MAKEP(sparksel,0);
	return TRUE;
}

void far pascal _loadds animatechar(c)
char c;
{
	switch(c) {
	case '+':
	case '=':
		if (firespeed <= MAXSPEED-10) firespeed += 10;
		break;
	case '-':
	case '_':
		if (firespeed > 10) firespeed -= 10;
		break;
	}
	dirty = TRUE;
}
	
void far pascal _loadds animatedblclk(mp)
MPARAM mp;
{
	mp;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,"Fireworks display animated desktop."
		"\n\nWhen the desktop has the focus, use '+' for more missles and '-' for less."
		"\n\nBy John Ridges\n\nInspired by the DOS version from Hand Crafted "
		"Software.","ANIMATE",0,MB_OK|MB_NOICON|MB_DEFBUTTON1|MB_APPLMODAL);
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
	DosFreeSeg(sparksel);
	if (dirty) WinWriteProfileData(init->animatehab,name,opt,
		(PBYTE)&firespeed,sizeof(firespeed));
}

void far pascal _loadds animatethread()
{
	int shots,xm,vxm,vym,i,tm,t,x,y,vx,vy,xo,yo,tmpspeed;
	long c1,c2,c3;
	FIXED ang,f,a,v;
	HAB hab;
	HPS hps;
	POINTL pnt;
	BOOL firsttime = TRUE;
	static long bright[] = { CLR_WHITE, CLR_PINK, CLR_GREEN, CLR_CYAN,
		CLR_YELLOW, CLR_RED };
	static long dark[] = { CLR_DARKBLUE, CLR_DARKPINK, CLR_DARKCYAN,
		CLR_DARKGRAY, CLR_BLACK };

	hab = WinInitialize(0);
	seed = WinGetCurrentTime(hab);
	shots = 10;
	xm = intrnd((int)(init->screenrectl.xRight-init->screenrectl.xLeft)-40)+20;
	while (!init->closesemaphore) {
		tmpspeed = firespeed;
		i = (int)(init->screenrectl.yTop-init->screenrectl.yBottom)-15;
		vym = FIXEDINT(fixsqr((i-FIXEDINT(fixrnd(fixrnd(49152L*i))))*2));
		vxm = (intrnd((int)(init->screenrectl.xRight-init->screenrectl.xLeft)+
			160)-80-xm)/(2*vym);
		ang = fixatn(FixDiv(MAKEFIXED(vxm,0),MAKEFIXED(vym,0)));
		f = fixsqr(vxm*vxm+vym*vym);
		for (i = 0; i < tmpspeed; i++) {
			spark[i].xm = xm+(i&1);
			spark[i].ym = 15+((i>>1)&1);
			spark[i].cm = CLR_RED;
			if (!i || (rand()&1)) {
				spark[i].xvm = vxm;
				spark[i].yvm = vym;
				if (rand()&1) spark[i].cm = CLR_DARKGRAY;
			}
			else {
				a = ang+32768-fixrnd(65536);
				v = FixMul(fixrnd(fixrnd(26214L))+6554,f);
				spark[i].xvm = FIXEDINT(FixMul(v,fixsin(a)));
				spark[i].yvm = FIXEDINT(FixMul(v,fixsin(a+102944)));
			}
		}
		tm = 3+intrnd(2*vym-6);
		f = fixrnd(fixrnd(262144))+65536;
		c1 = bright[intrnd(sizeof(bright)/sizeof(long))];
		c2 = bright[intrnd(sizeof(bright)/sizeof(long))];
		c3 = dark[intrnd(sizeof(dark)/sizeof(long))];
		for (i = 0; i < tmpspeed; i++) {
			a = fixrnd(411775);
			v = fixrnd(f);
			spark[i].xv = FIXEDINT(10*FixMul(v,fixsin(a+102944)));
			spark[i].yv = FIXEDINT(8*FixMul(v,fixsin(a)));
			if (rand()&1) spark[i].c = c1;
			else spark[i].c = c2;
		}
		if (shots > 2+intrnd(5)) {
			DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
			hps = WinGetPS(init->screenhwnd);
			WinFillRect(hps,&init->screenrectl,CLR_BLACK);
			WinFillRect(init->shadowhps,&init->screenrectl,CLR_BLACK);
			WinReleasePS(hps);
			DosSemClear(&init->hpssemaphore);
			shots = 0;
			if (firsttime) WinPostMsg(init->screenhwnd,WM_USER,0,0);
			firsttime = FALSE;
		}
		for (t = 1; t <= tm; t++) {
		wait1:
			if (init->closesemaphore) goto giveup;
			if (!(init->screenvisible)()) goto wait1;
			DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
			hps = WinGetPS(init->screenhwnd);
			for (i = 0; i < tmpspeed; i++) {
				x = spark[i].xm;
				vx = spark[i].xvm;
				y = spark[i].ym;
				vy = spark[i].yvm;
				if (t > 1) {
					GpiSetColor(hps,CLR_DARKGRAY);
					GpiSetColor(init->shadowhps,CLR_DARKGRAY);
					pnt.x = x-vx+init->screenrectl.xLeft;
					pnt.y = y-vy+init->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					GpiMove(init->shadowhps,&pnt);
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					GpiLine(init->shadowhps,&pnt);
				}
				else {
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					GpiMove(init->shadowhps,&pnt);
				}
				if (y < 0) {
					if (vy < 0) {
						vy = vx = 0;
						spark[i].cm = CLR_DARKGRAY;
					}
				}
				else vy -= 1;
				if (t < tm) {
					x += vx;
					y += vy;
					GpiSetColor(hps,spark[i].cm);
					GpiSetColor(init->shadowhps,spark[i].cm);
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					GpiLine(init->shadowhps,&pnt);
					spark[i].xm = x;
					spark[i].xvm = vx;
					spark[i].ym = y;
					spark[i].yvm = vy;
				}
			}
			WinReleasePS(hps);
			DosSemClear(&init->hpssemaphore);
		}
		xo = spark[0].xm;
		vxm = spark[0].xvm/4;
		yo = spark[0].ym;
		vym = spark[0].yvm/8;
		tm = 90+intrnd(20);
		for (t = 1; t <= tm; t++) {
		wait2:
			if (init->closesemaphore) goto giveup;
			if (!(init->screenvisible)()) goto wait2;
			DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
			hps = WinGetPS(init->screenhwnd);
			for (i = 0; i < tmpspeed; i++) {
				vx = spark[i].xv;
				vy = spark[i].yv;
				if (t > 1) {
					x = spark[i].xc;
					y = spark[i].yc;
					GpiSetColor(hps,c3);
					GpiSetColor(init->shadowhps,c3);
					pnt.x = x-vx+init->screenrectl.xLeft;
					pnt.y = y-vy+init->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					GpiMove(init->shadowhps,&pnt);
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					GpiLine(init->shadowhps,&pnt);
				}
				else {
					x = xo;
					vx += vxm;
					y = yo;
					vy += vym;
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					GpiMove(init->shadowhps,&pnt);
				}
				if (t < tm) {
					if (y < 25) {
						if (vy < 0) {
							vy = -vy/4;
							if (vy) vx = vx/4;
							else {
								vx = 0;
								spark[i].c = c3;
							}
						}
					}
					else vy -= (t&1);
					x += vx;
					y += vy;
					GpiSetColor(hps,spark[i].c);
					GpiSetColor(init->shadowhps,spark[i].c);
					pnt.x = x+init->screenrectl.xLeft;
					pnt.y = y+init->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					GpiLine(init->shadowhps,&pnt);
					spark[i].xc = x;
					spark[i].xv = vx;
					spark[i].yc = y;
					spark[i].yv = vy;
				}
			}
			WinReleasePS(hps);
			DosSemClear(&init->hpssemaphore);
		}
		shots++;
		if (!(rand()&3))
			xm = intrnd((int)(init->screenrectl.xRight-init->screenrectl.xLeft)-
				40)+20;
	}
giveup:
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init->closesemaphore);
}

static int near intrnd(arg)
int arg;

{
	return FIXEDINT(fixrnd(MAKEFIXED(arg,0)));
}

static FIXED near fixrnd(arg)
FIXED arg;

{
	return FixMul(arg,(FIXED)rand()<<1);
}

static FIXED near fixsqr(arg)
int arg;

{
	int i;
	FIXED a,b,c;

	a = 0L;
	b = 8388608;
	c = MAKEFIXED(arg,0);
	for (i = 0; i < 23; i++) {
		a ^= b;
		if (FixMul(a,a) > c) a ^= b;
		b >>= 1;
	}
	return a;
}

static FIXED near fixsin(arg)
FIXED arg;

{
	int i;
	FIXED r,s;
	static FIXED c[] = { -5018418, 5347884, -2709368, 411775 };

	arg = FixDiv(arg,411775);
	arg -= arg&0xffff0000;
	if (arg > 49152L) arg -= 65536L;
	else if (arg > 16384L) arg = 32768L-arg;
	s = FixMul(arg,arg);
	r = 2602479;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	return FixMul(arg,r);
}

static FIXED near fixatn(arg)
FIXED arg;

{
	int i;
	FIXED a,r,s;
	static FIXED c[] = { -20L, 160L, -1553L, 27146L };

	a = arg;
	if (arg < 0L) a = -a;
	if (a > 65536L) a = FixDiv(65536L,a);
	r = FixMul(a,92682);
	a = FixDiv(r+a-65536L,r-a+65536L);
	s = FixMul(a,a);
	r = 2L;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	a = 25736+FixMul(a,r);
	if (arg > 65536L || arg < -65536L) a = 102944-a;
	if (arg < 0L) a = -a;
	return a;
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
