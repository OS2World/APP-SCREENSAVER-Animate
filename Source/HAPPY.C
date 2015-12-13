/***************************************************************************\
* HAPPY.C - An example animated desktop extension by John Ridges
\***************************************************************************/

#define NUMBEROFPOINTERS 5   /* How many happy faces on the screen */
#define SPEED 3              /* How many pels a happy face moves each turn */

#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_WINDIALOGS
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <stdlib.h>   /* Needed because SRAND and RAND are called */

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

/*
   This is the record that describes the position and velocity of
   a happy face
*/
typedef struct {
	FIXED xvelocity,yvelocity;
	FIXED xposition,yposition;
	BOOL invert;
} POINTERREC;

static SEL pointersel;                     /* Selector for the array of POINTERRECs */
static HPOINTER hptr;                      /* Handle of the happy face pointer */
static POINTERREC volatile far *pointers;  /* Address of the array of POINTERRECs */
static BOOL volatile invert = FALSE;       /* The 'invert the faces' flag */

char far pascal _loadds animatename()
{
	return 'H';   /* Tell ANIMATE that we are H for Happy face */
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	int i;

	init = initptr;   /* Save the INITBLOCK address locally */

	/* Get memory for the array of POINTERRECs */
	DosAllocSeg(sizeof(POINTERREC)*NUMBEROFPOINTERS,&pointersel,SEG_NONSHARED);
	pointers = MAKEP(pointersel,0);

	/* Flag all the happy faces as 'position unknown' */
	for (i = 0; i < NUMBEROFPOINTERS; i++) pointers[i].xposition = -1L;

	/* Get the handle to the happy face */
	hptr = WinLoadPointer(HWND_DESKTOP,init->thismodule,1);

	/* Tell ANIMATE that we won't be using the shadow hps */
	return FALSE;
}

void far pascal _loadds animatechar(c)
char c;
{
	int i;
	
	switch (c) {
	case 'i':
	case 'I':
		invert = !invert;   /* Change the 'invert the faces' flag */
		break;
	case 'r':
	case 'R':
		/* Reverse direction of all the happy faces */
		for (i = 0; i < NUMBEROFPOINTERS; i++) {
			pointers[i].xvelocity = -pointers[i].xvelocity;
			pointers[i].yvelocity = -pointers[i].yvelocity;
		}
		break;
	}
}
	
void far pascal _loadds animatedblclk(mp1)
MPARAM mp1;
{
	mp1;   /* Reference the argument so we don't get compiler warnings */

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Happy faces example animated desktop extension.\n\n"
		"When the desktop has the focus, use 'I' to invert the happy faces "
		"and 'R' to reverse their direction.\n\nBy John Ridges",
		"ANIMATE",0,MB_OK|MB_NOICON);
}

void far pascal _loadds animatepaint(hps,prclpaint)
HPS hps;
RECTL far *prclpaint;
{
	int i;

	/* Paint the screen with the background color */
	WinFillRect(hps,prclpaint,SYSCLR_BACKGROUND);

	/* Redraw all the happy faces with known positions */
	for (i = 0; i < NUMBEROFPOINTERS; i++) if (pointers[i].xposition >= 0)
		WinDrawPointer(hps,FIXEDINT(pointers[i].xposition),
			FIXEDINT(pointers[i].yposition),hptr,
			pointers[i].invert ? DP_INVERTED : DP_NORMAL);
}

void far pascal _loadds animateclose()
{
	/* Release the memory for the array of POINTERRECs */
	DosFreeSeg(pointersel);
}

void far pascal _loadds animatethread()
{
	HAB hab;
	HPS hps;
	int i;
	FIXED tempx,tempy;
	ULONG sqrt;

	/* Get an HAB for this thread (since we make PM calls) */
	hab = WinInitialize(0);

	/* Randomize RAND using the time */
	srand((unsigned int)WinGetCurrentTime(hab));

	/* Tell ANIMATE that we are done initializing and painting may proceed */
	WinPostMsg(init->screenhwnd,WM_USER,0,0);

	/* Loop until ANIMATE tells us to stop */
	while (!init->closesemaphore)

		/* Don't do anything if the screen is not visible */
		if ((init->screenvisible)()) {

		/* Enter the critical section so we can get an HPS of the screen */
		DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinGetPS(init->screenhwnd);

		/* Process for each happy face */
		for (i = 0; i < NUMBEROFPOINTERS; i++) {

			/* If the happy face's position is unknown, initialize it */
			if (pointers[i].xposition < 0) {

				/* Pick a random x velocity between -1 and 1 (fixed) */
				tempx = (long)rand()<<1;
				pointers[i].xvelocity = rand()&1 ? tempx : -tempx;

				/*
				   Make the total velocity 1 by computing:
				   yvelocity = sqrt(1 - xvelocity * xvelocity)
				*/
				tempy = MAKEFIXED(0,65535)-((ULONG)tempx*(ULONG)tempx>>16);

				/* Cheesy sqrt routine to avoid linking in floating point */
				sqrt = 0;
				tempx = 1L<<15;
				do {
					sqrt ^= tempx;
					if (sqrt*sqrt>>16 > (ULONG)tempy) sqrt ^= tempx;
					tempx >>= 1;
				} while (tempx);

				/* Randomly set y velocity sign */
				pointers[i].yvelocity = rand()&1 ? sqrt : -sqrt;

				/* Multiply the velocities by the requested speed */
				pointers[i].xvelocity *= SPEED;
				pointers[i].yvelocity *= SPEED;

				/* Randomly choose the x and y position of the happy face */
				tempx = MAKEFIXED(init->screenrectl.xLeft,0)+(rand()*
					(init->screenrectl.xRight-init->screenrectl.xLeft-32)<<1);
				tempy = MAKEFIXED(init->screenrectl.yBottom,0)+(rand()*
					(init->screenrectl.yTop-init->screenrectl.yBottom-32)<<1);
			}
			else {
				/* Find the new position of the happy face */
				tempx = pointers[i].xposition+pointers[i].xvelocity;
				tempy = pointers[i].yposition+pointers[i].yvelocity;

				/* See if the happy face has hit an edge of the screen */
				if (FIXEDINT(tempx) < (int)init->screenrectl.xLeft) {

					/* Bounced off the left edge */
					tempx = (init->screenrectl.xLeft<<17)-tempx;
					pointers[i].xvelocity = -pointers[i].xvelocity;
				}
				else if (FIXEDINT(tempx) > (int)init->screenrectl.xRight-32) {

					/* Bounced off the right edge */
					tempx = (init->screenrectl.xRight-32<<17)-tempx;
					pointers[i].xvelocity = -pointers[i].xvelocity;
				}
				if (FIXEDINT(tempy) < (int)init->screenrectl.yBottom) {

					/* Bounced off the bottom edge */
					tempy = (init->screenrectl.yBottom<<17)-tempy;
					pointers[i].yvelocity = -pointers[i].yvelocity;
				}
				else if (FIXEDINT(tempy) >= (int)init->screenrectl.yTop-32) {

					/* Bounced off the top edge */
					tempy = (init->screenrectl.yTop-32<<17)-tempy;
					pointers[i].yvelocity = -pointers[i].yvelocity;
				}
				/* Undraw the happy face in the old position */
				WinDrawPointer(hps,FIXEDINT(pointers[i].xposition),
					FIXEDINT(pointers[i].yposition),hptr,
					pointers[i].invert ? DP_INVERTED : DP_NORMAL);
			}
			/* Draw the happy face in the new position */
			WinDrawPointer(hps,FIXEDINT(tempx),FIXEDINT(tempy),hptr,
				invert ? DP_INVERTED : DP_NORMAL);

			/* Save the position and invert value of the happy face */
			pointers[i].xposition = tempx;
			pointers[i].yposition = tempy;
			pointers[i].invert = invert;
		}
		/* Release the HPS of the screen and leave the critical section */
		WinReleasePS(hps);
		DosSemClear(&init->hpssemaphore);
	}
	/* We've been told to close, so get rid of the HAB */
	WinTerminate(hab);

	/* Make sure the stack doesn't vanish before we're completely gone */
	DosEnterCritSec();

	/* Tell ANIMATE that we're gone */
	DosSemClear((HSEM)&init->closesemaphore);
}
