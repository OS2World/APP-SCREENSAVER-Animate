/***************************************************************************\
* BOXES.C - An example animated desktop extension by John Ridges
\***************************************************************************/

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_GPIPRIMITIVES
#define INCL_WINDIALOGS
#define INCL_WINMESSAGEMGR
#define INCL_WINSYS
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <stdlib.h>   /* Needed because rand and labs are called */

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

char far pascal _loadds animatename()
{
	return 'B';   /* Tell ANIMATE that we are B for Boxes */
}

BOOL far pascal _loadds animateinit(initptr)
INITBLOCK far *initptr;
{
	init = initptr;   /* Save the INITBLOCK address locally */

	/* Tell ANIMATE that we will be using the shadow HPS */
	return TRUE;
}

void far pascal _loadds animatechar(c)
char c;
{
	c;   /* Reference the argument so we don't get compiler warnings */
}
	
void far pascal _loadds animatedblclk(mp1)
MPARAM mp1;
{
	mp1;

	WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,
		"Boxes example animated desktop extension.\n\nBy John Ridges",
		"ANIMATE",0,MB_OK|MB_NOICON);
}

void far pascal _loadds animatepaint(hps,prclpaint)
HPS hps;
RECTL far *prclpaint;
{
	hps;
	prclpaint;
}

void far pascal _loadds animateclose()
{
}

void far pascal _loadds animatethread()
{
	HAB hab;
	HPS hps;
	POINTL pnt1,pnt2;
	long color,roundh,roundv;
	static long colorset[] = { CLR_WHITE, CLR_BLUE, CLR_RED, CLR_PINK, CLR_GREEN,
		CLR_CYAN, CLR_YELLOW, CLR_DARKGRAY, CLR_DARKBLUE, CLR_DARKRED, CLR_BLACK,
		CLR_DARKPINK, CLR_DARKGREEN, CLR_DARKCYAN, CLR_BROWN, CLR_PALEGRAY };

	/* Get an HAB for this thread (since we make PM calls) */
	hab = WinInitialize(0);

	/* Enter the critical section so we can use the shadow HPS */
	DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);

	/* Paint the shadow screen with the background color */
	WinFillRect(init->shadowhps,&init->screenrectl,SYSCLR_BACKGROUND);

	/* Exit the critical section */
	DosSemClear(&init->hpssemaphore);

	/* Tell ANIMATE that we are done initializing and painting may proceed */
	WinPostMsg(init->screenhwnd,WM_USER,0,0);
	
	/* Loop until ANIMATE tells us to stop */
	while (!init->closesemaphore)
	
		/* Don't do anything if the screen is not visible */
		if ((init->screenvisible)()) {

		/* Pick a random color for the box */
		color = colorset[(long)rand()*sizeof(colorset)/sizeof(long)>>15];

		/* Pick two random corner positions for the box */
		pnt1.x = init->screenrectl.xLeft+(long)rand()*
			(init->screenrectl.xRight-init->screenrectl.xLeft)>>15;
		pnt1.y = init->screenrectl.yBottom+(long)rand()*
			(init->screenrectl.yTop-init->screenrectl.yBottom)>>15;
		pnt2.x = init->screenrectl.xLeft+(long)rand()*
			(init->screenrectl.xRight-init->screenrectl.xLeft)>>15;
		pnt2.y = init->screenrectl.yBottom+(long)rand()*
			(init->screenrectl.yTop-init->screenrectl.yBottom)>>15;

		/* Pick a random roundness for the corners */
		roundh = labs(pnt1.x-pnt2.x)*rand()>>15;
		roundv = labs(pnt1.y-pnt2.y)*rand()>>15;

		/* Enter the critical section so we can get an HPS of the screen */
		DosSemRequest(&init->hpssemaphore,SEM_INDEFINITE_WAIT);
		hps = WinGetPS(init->screenhwnd);

		/* Set the color of both the screen HPS and the shadow screen HPS */
		GpiSetColor(hps,color);
		GpiSetColor(init->shadowhps,color);

		/* Move to the random corner */
		GpiMove(hps,&pnt1);
		GpiMove(init->shadowhps,&pnt1);

		/* Draw the box */
		GpiBox(hps,DRO_FILL,&pnt2,roundh,roundv);
		GpiBox(init->shadowhps,DRO_FILL,&pnt2,roundh,roundv);

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
