/***************************************************************************\
* AQUA.C - An aquarium animated desktop extension by John Ridges
\***************************************************************************/

#define NUMFISH 7
#define MAXFISH 15
#define MAXSPEED 3
#define FISHTYPE 3
#define DELAYTIME 50
#define BACKGROUND CLR_DARKBLUE

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_WINRECTANGLES
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
	BOOL (far pascal *screenvisible)(void);
} INITBLOCK;

char far pascal _loadds animatename(void);
BOOL far pascal _loadds animateinit(INITBLOCK far *);
void far pascal _loadds animatechar(char);
void far pascal _loadds animatedblclk(MPARAM);
void far pascal _loadds animatepaint(HPS, RECTL far *);
void far pascal _loadds animateclose(void);
void far pascal _loadds animatethread(void);

static INITBLOCK far *init;

typedef struct {
	int velocity,tempvel;
	int xposition,yposition,ytop;
	int fishtype,flip;
	int countdown;
} FISHREC;

int far cdecl abs(int);
static int near collide(int, int);
static void near plot(HPS, int, int);
static int near rand(void);

static int near sizex[FISHTYPE*4];
static int near sizey[FISHTYPE*4];
static HPS near memhps[FISHTYPE*4];
static SEL near fishsel;
static ULONG near seed;
static FISHREC volatile far * near fish;
static int volatile numfish = NUMFISH;
static char name[] = "Animate";
static char opt[] = "Aquarium";
static BOOL dirty = FALSE;

char far pascal _loadds animatename()
{
	return 'A';
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	USHORT i;

	init = initptr;
	i = sizeof(numfish);
	WinQueryProfileData(init->animatehab,name,opt,(PBYTE)&numfish,&i);
	DosAllocSeg(sizeof(FISHREC)*MAXFISH,&fishsel,SEG_NONSHARED);
	fish = MAKEP(fishsel,0);
	for (i = 0; i < MAXFISH; i++)	fish[i].yposition = fish[i].ytop = -1000;
	return FALSE;
}

void far pascal _loadds animatechar(c)
char c;
{
	switch(c) {
	case '+':
	case '=':
		if (numfish < MAXFISH) numfish++;
		break;
	case '-':
	case '_':
		if (numfish > 1) numfish--;
	}
	dirty = TRUE;
}
	
void far pascal _loadds animatedblclk(mp1)
MPARAM mp1;
{
	mp1;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Aquarium animated desktop.\n\nWhen the desktop has the focus, "
		"use '+' for more fish and '-' for less.\n\nBy John Ridges\n\n"
		"Inspired by the Mac version.","ANIMATE",0,MB_OK|MB_NOICON);
}

void far pascal _loadds animatepaint(hps,prclpaint)
HPS hps;
RECTL far *prclpaint;
{
	int i;

	WinFillRect(hps,prclpaint,BACKGROUND);
	for (i = 0; i < MAXFISH; i++) if (fish[i].yposition != -1000)
		plot(hps,i,fish[i].fishtype);
}

void far pascal _loadds animateclose()
{
	DosFreeSeg(fishsel);
	if (dirty) WinWriteProfileData(init->animatehab,name,opt,(PBYTE)&numfish,sizeof(numfish));
}

void far pascal _loadds animatethread()
{
	int i,j,k,m,type;
	HAB hab;
	HPS hps;
	SIZEL sizl;
	RECTL rectup;
	HDC memhdc[FISHTYPE*4];
	HBITMAP bitmap,memhbm[FISHTYPE*4];
	ULONG time,aldata[2];
	POINTL aptl[4];
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;

	hab = WinInitialize(0);
	WinPostMsg(init->screenhwnd,WM_USER,0,0);
	seed = WinGetCurrentTime(hab);
	for (i = 0; i < FISHTYPE*4; i++) {
		dop.pszLogAddress = NULL;
		dop.pszDriverName = "DISPLAY";
		dop.pdriv = NULL;
		dop.pszDataType = NULL;
		memhdc[i] = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,
			NULL);
		sizl.cx = sizl.cy = 0;
		memhps[i] = GpiCreatePS(hab,memhdc[i],&sizl,
			PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
		GpiQueryDeviceBitmapFormats(memhps[i],2L,aldata);
		bmi.cbFix = sizeof(BITMAPINFOHEADER);
		sizex[i] = 32+32*(~i&1)+MAXSPEED;
		bmi.cx = sizex[i]*8;
		bmi.cy = sizey[i] = 32+16*(~i&2);
		bmi.cPlanes = (USHORT)aldata[0];
		bmi.cBitCount = (USHORT)aldata[1];
		memhbm[i] = GpiCreateBitmap(memhps[i],&bmi,0L,NULL,NULL);
		GpiSetBitmap(memhps[i],memhbm[i]);
		if (i%4 < 2) {
			WinSetRect(hab,&rectup,0,0,bmi.cx,bmi.cy);
			WinFillRect(memhps[i],&rectup,BACKGROUND);
			bitmap = GpiLoadBitmap(memhps[i],init->thismodule,
				i/4+1,0L,0L);
			for (j = 0; j < 8; j++) {
				aptl[0].x = j*sizex[i]+MAXSPEED*(j < 2 || j == 4);
				aptl[0].y = aptl[2].y = 0;
				aptl[1].x = aptl[0].x+sizex[i]-MAXSPEED-1;
				aptl[1].y = sizey[i]-1;
				aptl[2].x = j*64;
				aptl[2].y = 0;
				aptl[3].x = aptl[2].x+64;
				aptl[3].y = 64;
				GpiWCBitBlt(memhps[i],bitmap,4L,aptl,ROP_SRCCOPY,BBO_IGNORE);
			}
			GpiDeleteBitmap(bitmap);
		}
		else {
			aptl[0].x = aptl[0].y = aptl[2].x = aptl[2].y = 0;
			aptl[1].x = bmi.cx;
			aptl[1].y = bmi.cy;
			aptl[3].x = sizex[i-2]*8;
			aptl[3].y = sizey[i-2];
			GpiBitBlt(memhps[i],memhps[i-2],4L,aptl,ROP_SRCCOPY,
				BBO_IGNORE);
		}
	}
	while (!init->closesemaphore) if ((init->screenvisible)()) {
		time = WinGetCurrentTime(hab);
		DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinGetPS(init->screenhwnd);
		for (i = 0; i < MAXFISH; i++)
			fish[i].xposition += fish[i].velocity;
		for (i = 0; i < MAXFISH; i++) {
			if (fish[i].yposition == -1000) {
				for (j = k = 0; j < MAXFISH && k < numfish; j++)
					k += fish[j].yposition != -1000;
				if (j == MAXFISH) {
					fish[i].fishtype = type = (int)((long)rand()*FISHTYPE*4>>15);
					k = rand()&1;
					j = (int)(((long)rand()*MAXSPEED>>15))+1;
					fish[i].velocity = k ? j : -j;
					fish[i].yposition = (int)((rand()*(init->screenrectl.yTop-
						init->screenrectl.yBottom-64)>>15)+
						init->screenrectl.yBottom);
					fish[i].ytop = fish[i].yposition+sizey[type];
					fish[i].xposition = k ? -sizex[type] : (int)init->screenrectl.xRight;
					fish[i].flip = fish[i].countdown = 0;
					if (collide(i,0) >= 0) fish[i].yposition = fish[i].ytop = -1000;
				}
			}
			if (fish[i].yposition != -1000) {
				type = fish[i].fishtype;
				j = i+1;
			retrycollide:
				if ((j = collide(i,j)) >= 0) {
					if (!((fish[i].velocity < 0 && fish[j].velocity > 0) ||
						(fish[i].velocity > 0 && fish[j].velocity < 0))) {
						if ((fish[i].velocity+fish[j].velocity > 0 &&
							fish[i].xposition < fish[j].xposition) ||
							(fish[i].velocity+fish[j].velocity < 0 &&
							fish[i].xposition > fish[j].xposition)) k = j;
						else k = i;
						if (fish[k].velocity) fish[k].velocity = -fish[k].velocity;
						else fish[k].velocity = fish[i].velocity+fish[j].velocity > 0 ?
							-abs(fish[k].tempvel) : abs(fish[k].tempvel);
					}
					if (type%4 != fish[j].fishtype%4) k = type%4 < fish[j].fishtype%4;
					else k = abs(fish[i].velocity) > abs(fish[j].velocity);
					WinSetRect(hab,&rectup,fish[i].xposition,fish[i].yposition,
						fish[i].xposition+sizex[type],fish[i].ytop);
					WinFillRect(hps,&rectup,BACKGROUND);
					WinSetRect(hab,&rectup,fish[j].xposition,fish[j].yposition,
						fish[j].xposition+sizex[fish[j].fishtype],fish[j].ytop);
					WinFillRect(hps,&rectup,BACKGROUND);
					m = k ? i : j;
					k = k ? j : i;
					fish[m].yposition = fish[k].yposition;
					fish[m].ytop = fish[m].yposition+sizey[fish[m].fishtype];
					if (fish[m].velocity > 0)
						fish[m].xposition += sizex[fish[k].fishtype]>>1;
					else fish[m].xposition -= sizex[fish[k].fishtype]>>1;
					fish[k].yposition = fish[k].ytop = -1000;
					fish[m].countdown = -2;
					j = 0;
					goto retrycollide;
				}
				if (fish[i].velocity) {
					if (!(rand()>>5)) {
						fish[i].tempvel = fish[i].velocity;
						fish[i].velocity = 0;
						fish[i].countdown = 32+(rand()>>10);
					}
				}
				else if (!--fish[i].countdown)
					fish[i].velocity = rand()&1 ? fish[i].tempvel : -fish[i].tempvel;
				if (fish[i].xposition < -sizex[type] ||
					fish[i].xposition > (int)init->screenrectl.xRight)
					fish[i].yposition = fish[i].ytop = -1000;
				else {
					fish[i].flip++;
					plot(hps,i,type);
				}
			}
		}
		WinReleasePS(hps);
		DosSemClear(&init->hpssemaphore);
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
	for (i = 0; i < FISHTYPE*4; i++) {
		GpiSetBitmap(memhps[i],NULL);
		GpiDeleteBitmap(memhbm[i]);
		GpiDestroyPS(memhps[i]);
		DevCloseDC(memhdc[i]);
	}
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&init->closesemaphore);
}

static int near collide(i,start)
int i,start;
{
	int low,high;

	low = fish[i].yposition;
	high = fish[i].ytop;
	if (low != -1000) for (; start < MAXFISH; start++) if (i != start &&
		high > fish[start].yposition && low < fish[start].ytop &&
		fish[i].xposition < fish[start].xposition+sizex[fish[start].fishtype] &&
		fish[i].xposition+sizex[fish[i].fishtype] > fish[start].xposition)
		return start;
	return -1;
}

static void near plot(hps,i,type)
HPS hps;
int i,type;
{
	int k;
	POINTL aptl[3];

	aptl[0].x = fish[i].xposition;
	aptl[0].y = fish[i].yposition;
	aptl[1].x = aptl[0].x+sizex[type];
	aptl[1].y = aptl[0].y+sizey[type];
	if (fish[i].countdown < 0) {
		fish[i].countdown++;
		k = 4+(fish[i].velocity < 0);
	}
	else k = ((fish[i].velocity <= 0)+(!fish[i].velocity<<1)<<1)+
		((fish[i].flip>>2)&1);
	aptl[2].x = k*sizex[type];
	aptl[2].y = 0;
	GpiBitBlt(hps,memhps[type],3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
