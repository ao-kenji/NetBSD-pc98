/*
 * xsystm - display all informations about pccs and apm
 *
 * from "xload" sources in X11R5/6 contrib
 *
 * Naofumi Honda (I do NOT claim any copyright)
 */

/* $XConsortium: xload.c,v 1.37 94/04/17 20:43:44 converse Exp $ */
/*

Copyright (c) 1989  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/

#include <stdio.h> 
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/StripChart.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Sme.h>
#include <X11/Xaw/SmeBSB.h>

#include "xsystm.bit"

char *ProgramName;

void LoadSysinfo();
int InitAll(void);
extern void exit(), GetBattPoint(), hookGetBattPoint();
static void quit(), ApmSuspendCallback();
static void PccsSuspendCallback(), PccsResumeCallback();

/*
 * Command line options table.  Only resources are entered here...there is a
 * pass over the remaining options after XtParseCommand is let loose. 
 */
static XrmOptionDescRec options[] = {
	{"-update",		"*chart.update",		XrmoptionSepArg,	NULL},
};

static XtActionsRec xsystm_actions[] = {
	{"quit", quit},
};

static Atom wm_delete_window;
static Widget ChartWid, BattSBSBWid, BattSWid, BattWid;
static Widget PccsSWid[NSLOT], PccsWid[NSLOT];
static int PccsID[NSLOT];

int
main(argc, argv)
	int argc;
	char **argv;
{
	XtAppContext app_con;
	Widget toplevel, pane, ptw, tw;
	Arg args[1];
	int sno;
	Pixmap icon_pixmap = None;

	ProgramName = argv[0];

	if (InitAll() != 0)
		return 1;

	toplevel = XtAppInitialize(&app_con, "XSystm", options, XtNumber(options),
			&argc, argv, NULL, NULL, (Cardinal) 0);

	/*
	 * This is a hack so that f.delete will do something useful in this
	 * single-window application.
	 */
	XtAppAddActions(app_con, xsystm_actions, XtNumber(xsystm_actions));
	XtOverrideTranslations(toplevel,
			XtParseTranslationTable("<Message>WM_PROTOCOLS: quit()"));

	XtSetArg(args[0], XtNiconPixmap, &icon_pixmap);
	XtGetValues(toplevel, args, ONE);
	if(icon_pixmap == None) {
		XtSetArg(args[0], XtNiconPixmap, XCreateBitmapFromData(
				XtDisplay(toplevel), XtScreen(toplevel)->root,
				(char *)xsystm_bits, xsystm_width, xsystm_height));
		XtSetValues(toplevel, args, ONE);
	}

	pane = XtCreateManagedWidget ("paned", panedWidgetClass,
					toplevel, NULL, ZERO);
	BattWid = XtVaCreateManagedWidget ("batt", menuButtonWidgetClass,
					 pane, NULL);

	ptw = XtCreatePopupShell("menu", simpleMenuWidgetClass,
					 BattWid, NULL, ZERO);
	tw = XtCreateManagedWidget ("Suspend now", smeBSBObjectClass,
				     ptw, NULL, ZERO);
	XtAddCallback(tw, XtNcallback, ApmSuspendCallback, NULL);
	tw = XtCreateManagedWidget ("Stanby now", smeBSBObjectClass,
				     ptw, NULL, ZERO);

	for (sno = 0; sno < NSLOT; sno ++)
	{
		u_char menu_name[128];
		u_char pccs_name[128];

		sprintf(menu_name, "menu_slot%d", sno, sno);
		sprintf(pccs_name, "slot%d", sno);
		PccsID[sno] = sno;
		PccsWid[sno] = XtVaCreateManagedWidget (pccs_name,
			menuButtonWidgetClass, pane, NULL);

		ptw = XtCreatePopupShell("menu", simpleMenuWidgetClass,
					 PccsWid[sno], NULL, ZERO);
		tw = XtCreateManagedWidget ("Slot down", smeBSBObjectClass,
				            ptw, NULL, ZERO);
		XtAddCallback(tw, XtNcallback, PccsSuspendCallback, &PccsID[sno]);
		tw = XtCreateManagedWidget ("Slot up", smeBSBObjectClass,
				            ptw, NULL, ZERO);
		XtAddCallback(tw, XtNcallback, PccsResumeCallback, &PccsID[sno]);
	}

	ChartWid = XtCreateManagedWidget ("chart", stripChartWidgetClass,
			pane, NULL, ZERO);    

	hookGetBattPoint(NULL, NULL, NULL);
	XtAddCallback(ChartWid, XtNgetValue, hookGetBattPoint, NULL);

	XtRealizeWidget(toplevel);
	wm_delete_window = XInternAtom (XtDisplay(toplevel), "WM_DELETE_WINDOW",
				False);
	(void) XSetWMProtocols (XtDisplay(toplevel), XtWindow(toplevel),
			&wm_delete_window, 1);

	XtAppMainLoop(app_con);
}

static void quit(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
	if(event->type == ClientMessage &&
		event->xclient.data.l[0] != wm_delete_window) {
			XBell(XtDisplay(w), 0);
			return;
	}

	XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
	exit (0);
}

void
ApmSuspendCallback(Widget w, caddr_t closure, caddr_t call_data)
{

	system("/usr/sbin/apmctrl suspend\n");
}

void
PccsSuspendCallback(Widget w, caddr_t closure, caddr_t call_data)
{
	u_char cmdstring[1024];
	int *data = (int *) closure;
	int sno = *data;

	sprintf(cmdstring, "/usr/sbin/slotctrl slot%d down\n", sno);
	system(cmdstring);
}

void
PccsResumeCallback(Widget w, caddr_t closure, caddr_t call_data)
{
	u_char cmdstring[1024];
	int *data = (int *) closure;
	int sno = *data;

	sprintf(cmdstring, "/usr/sbin/slotctrl slot%d up\n", sno);
	system(cmdstring);
}

void
hookGetBattPoint(Widget w, caddr_t closure, caddr_t call_data)
{
	extern u_char *apmstring, *pccsstring[];
	extern int nevent;
	Arg args[1];
	int sno;

	LoadSysinfo(w, closure, call_data);

	if (nevent != 0)
	{
		XtSetArg(args[0], XtNlabel, apmstring);
		XtSetValues(BattWid, args, ONE);

		for (sno = 0; sno < NSLOT; sno ++)
		{
			if (PccsWid[sno] == NULL)
				continue;

			XtSetArg(args[0], XtNlabel, pccsstring[sno]);
			XtSetValues(PccsWid[sno], args, ONE);
		}
	}

}
