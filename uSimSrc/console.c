/* console.c
 * Copyright (C) 2000, Tsurishaddai Williamson, tsuri@earthlink.net
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**********************************************************************/

#include "ustdio.h"
#include "stack.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/**********************************************************************/
#pragma mark *** CONSOLE ***

#pragma mark gConsoleInputStack
#define kConsoleInputStackSize 256
static Stack gConsoleInputStack;
static char gConsoleInputStackData[kConsoleInputStackSize];

enum {
#ifdef COLOR
	kConsoleBlack          = 0x00,
	kConsoleWhite          = 0x01,
	kConsoleRed            = 0x02,
	kConsoleGreen          = 0x03,
	kConsoleBlue           = 0x04,
	kConsoleCyan           = 0x05,
	kConsoleMagenta        = 0x06,
	kConsoleYellow         = 0x07,
#endif
#ifdef ADM31
	kConsoleModeReverse    = 0x10,
	kConsoleModeUnderline  = 0x20,
	kConsoleModeBlink      = 0x40,
#endif
	kConsoleModeCursor     = 0x08
};

#ifdef COLOR
enum {
	kBackgroundColor       = kConsoleBlack,
	kConsoleColor          = kConsoleGreen
};
#endif
#define kBackColor blackColor
#define kForeColor greenColor

#pragma mark gConsole
static struct {
	char *windowTitle;
	char *aboutTitle;
	char *aboutText;
	unsigned isOpen;
	unsigned isQuit;
	unsigned escapeState;
	unsigned row;
	unsigned mouseX;
	unsigned mouseY;
	unsigned cursorX;
	unsigned cursorY;
	unsigned cursorMode;
	unsigned drawChar;
#if defined(ADM31) || defined(COLOR)
	unsigned drawMode;
	unsigned charMode;
	unsigned short _buffer[kConsoleMaxX * kConsoleMaxY];
#else
	unsigned char _buffer[kConsoleMaxX * kConsoleMaxY];
#endif
	struct {
		unsigned cursorX;
		unsigned cursorY;
		unsigned cursorMode;
#if defined(ADM31) || defined(COLOR)
		unsigned charMode;
		unsigned short _buffer[kConsoleMaxX * kConsoleMaxY];
#else
		unsigned char _buffer[kConsoleMaxX * kConsoleMaxY];
#endif
	} saved;
} gConsole;
#define buffer(x, y) _buffer[((y) * kConsoleMaxX) + (x)]

static void ConsoleDraw(unsigned c);

/**********************************************************************/
#pragma mark *** MACOS CONSOLE ***

#ifdef MACINTOSH

#include <Menus.h>
#include <Dialogs.h>
#include <QuickDraw.h>
#include <OSUtils.h>
#include <Memory.h>
#include <Devices.h>
#include <Events.h>
#include <Fonts.h>
#include <Windows.h>
#include <Sound.h>

#define kMaxBlinkTickCount 30

#define kTextFont     kFontIDMonaco
#define kTextSize     9
#define kFontHeight   11
#define kFontWidth    6
#define kFontAscent   9

#define kV1 (kFontHeight / 2)
#define kV2 (kFontHeight - kV1 - 1)
#define kV3 (kV1 + kV2)
#define kH1 (kFontWidth / 2)
#define kH2 (kFontWidth - kH1 - 1)
#define kH3 (kH1 + kH2)

#define kLeftMargin   2
#define kRightMargin  (((kConsoleMaxX - 1) * kFontWidth) + kLeftMargin)
#define kTopMargin    2
#define kBottomMargin (((kConsoleMaxY - 1) * kFontHeight) + kTopMargin)

int gConsoleInputCharCode;
int gConsoleInputKeyCode;

static int gIsFirstUpdateSkipped;
static WindowPtr gConsolePort;
static Rect gScrollRect;
static RgnHandle gUpdateRegion;
static MenuHandle gAppleMenu;
static MenuHandle gFileMenu;
static Rect gCursorRect;
static int gBlinkState;
static unsigned long gBlinkTickCount;

extern unsigned char *CStrToPStr(unsigned char *p, const char *c);

#ifdef COLOR

/* _SetColor() sets the foreground and background colors. */
static void
	_SetColor(unsigned foregroundColor, unsigned backgroundColor)
{

	switch (backgroundColor & 0x07) {
	case kConsoleBlack:
		BackColor(blackColor);
		break;
	case kConsoleWhite:
		BackColor(whiteColor);
		break;
	case kConsoleRed:
		BackColor(redColor);
		break;
	case kConsoleGreen:
		BackColor(greenColor);
		break;
	case kConsoleBlue:
		BackColor(blueColor);
		break;
	case kConsoleCyan:
		BackColor(cyanColor);
		break;
	case kConsoleMagenta:
		BackColor(magentaColor);
		break;
	case kConsoleYellow:
		BackColor(yellowColor);
		break;
	}

	switch (foregroundColor & 0x07) {
	case kConsoleBlack:
		ForeColor(blackColor);
		break;
	case kConsoleWhite:
		ForeColor(whiteColor);
		break;
	case kConsoleRed:
		ForeColor(redColor);
		break;
	case kConsoleGreen:
		ForeColor(greenColor);
		break;
	case kConsoleBlue:
		ForeColor(blueColor);
		break;
	case kConsoleCyan:
		ForeColor(cyanColor);
		break;
	case kConsoleMagenta:
		ForeColor(magentaColor);
		break;
	case kConsoleYellow:
		ForeColor(yellowColor);
		break;
	}

}

#endif

/* _ConsoleBeep() beeps the beep-beep. */
static inline void _ConsoleBeep(void)
{

	/* The System Beeper. */
	SysBeep(60);

}

/* _ConsoleDraw() draws a character. */
static inline void _ConsoleDraw(void)
{
	unsigned c = gConsole.drawChar;
#if defined(ADM31) || defined(COLOR)
	unsigned mode = gConsole.drawMode;
#endif

#if defined(ADM31) && defined(COLOR)
	/* Set the foreground and background colors. */
	if (mode & kConsoleModeReverse)
		/* Reverse video. */
		_SetColor(kBackgroundColor, mode & 0x07);
	else
		/* Normal video. */
		_SetColor(mode & 0x07, kBackgroundColor);
#endif

#if defined(ADM31) && !defined(COLOR)
	/* Set the foreground and background colors. */
	if (mode & kConsoleModeReverse) {
		/* Reverse video. */
		BackColor(kForeColor);
		ForeColor(kBackColor);
	}
	else {
		/* Normal video. */
		BackColor(kBackColor);
		ForeColor(kForeColor);
	}
#endif

#if !defined(ADM31) && defined(COLOR)
	/* Set the foreground and background colors. */
	_SetColor(mode & 0x07, kBackgroundColor);
#endif

#if !defined(ADM31) && !defined(COLOR)
	/* Set the foreground and background colors. */
	BackColor(kBackColor);
	ForeColor(kForeColor);
#endif

	/* Erase and existing character. */
	EraseRect(&gCursorRect);

	/* Draw any printable character. */
	if (isprint(c)) {
		MoveTo(gCursorRect.left, gCursorRect.top + kFontAscent);
		DrawChar(c);
#ifdef ADM31
		if (mode & kConsoleModeUnderline) {
			/* Underline. */
			MoveTo(gCursorRect.left, gCursorRect.bottom - 1);
			Line(kH3, 0);
		}
#endif
	}
	/* Otherwise draw the line graphics characters. */
	else {
		MoveTo(gCursorRect.left, gCursorRect.top);
		switch (c) {
		case LOWERRIGHTCORNER:
			Move(0, kV1); Line(kH1, 0); Line(0, -kV1);
			break;
		case UPPERRIGHTCORNER:
			Move(0, kV1); Line(kH1, 0); Line(0, kV2);
			break;
		case UPPERLEFTCORNER:
			Move(kH3, kV1); Line(-kH2, 0); Line(0, kV2);
			break;
		case LOWERLEFTCORNER:
			Move(kH3, kV1); Line(-kH2, 0); Line(0, -kV1);
			break;
		case CROSS:
			Move(0, kV1); Line(kH3, 0);
			Move(-kH2, -kV1); Line(0, kV3);
			break;
		case HORIZONTALLINE:
			Move(0, kV1); Line(kH3, 0);
			break;
		case TEERIGHT:
			Move(kH1, 0); Line(0, kV3);
			Move(0, -kV2); Line(kH2, 0);
			break;
		case TEELEFT:
			Move(kH1, 0); Line(0, kV3);
			Move(0, -kV2); Line(-kH1, 0);
			break;
		case TEEUP:
			Move(0, kV1); Line(kH3, 0);
			Move(-kH2, 0); Line(0, -kV1);
			break;
		case TEEDOWN:
			Move(0, kV1); Line(kH3, 0);
			Move(-kH2, 0); Line(0, kV2);
			break;
		case VERTICALLINE:
			Move(kH1, 0); Line(0, kV3);
			break;
		default: /* An empty box. */
			break;
			Line(0, kV3); Line(kH3, 0);
			Line(0, -kV3); Line(-kH3, 0);
			break;
		}
	}

}

/* _ConsoleCursor() draws the cursor. */
static void _ConsoleCursor(int entry)
{
	static GrafPtr savedPort;

	/* Hide cursor at ConsoleOutput() entry. */
	if (entry) {
		/* Save the Current Port. */
		GetPort(&savedPort);
		/* Use the Console Port. */
		SetPort(gConsolePort);
		/* Redraw the character under the cursor. */
		if (gConsole.cursorMode & kConsoleModeCursor)
			ConsoleDraw(gConsole.buffer(gConsole.cursorX,
			                            gConsole.cursorY));
	}
	/* Display cursor at ConsoleOutput() exit. */
	else if (gBlinkState) {
		/* Draw the cursor if it is not to be hidden. */
		if (gConsole.cursorMode & kConsoleModeCursor) {
#ifdef COLOR
			_SetColor(gConsole.charMode & 0x07, kBackgroundColor);
#endif
			MoveTo(gCursorRect.left, gCursorRect.bottom - 2);
			Line(kH3, 0); Move(0, -1); Line(-kH3, 0);
		}
		/* Restore the Saved Port. */
		SetPort(savedPort);
	}

}

/* _ConsoleMove() moves the cursor. */
static inline void _ConsoleMove(void)
{

	/* Reset the Cursor Rectangle. */

	gCursorRect.left =
		(gConsole.cursorX * kFontWidth) + kLeftMargin;

	gCursorRect.right =
		gCursorRect.left + kFontWidth;

	gCursorRect.top =
		(gConsole.cursorY * kFontHeight) + kTopMargin;

	gCursorRect.bottom =
		gCursorRect.top + kFontHeight;

}

/* _ConsoleClearScreen() clears the screen. */
static inline void _ConsoleClearScreen(void)
{

	/* Erase the Console Port Rectangle. */
	EraseRect(&gConsolePort->portRect);

}

/* _ConsoleScrollUp() scrolls the screen up one line. */
static inline void _ConsoleScrollUp(void)
{

	/* Scroll in the Update Region. */
	ScrollRect(&gScrollRect, 0, -kFontHeight, gUpdateRegion);

}

/* _ConsoleBlink() makes things blink. */
static void _ConsoleBlink(void)
{
	GrafPtr savedPort;

	if ((TickCount() - gBlinkTickCount) > kMaxBlinkTickCount) {
		gBlinkState = !gBlinkState;
		GetPort(&savedPort);
		SetPort(gConsolePort);
		_ConsoleCursor(1);
		_ConsoleCursor(0);
		SetPort(savedPort);
		gBlinkTickCount = TickCount();
	}

}

/* _ConsoleInput() inputs a keystroke. */
static inline int _ConsoleInput(unsigned long waitSeconds)
{
	GrafPtr savedPort;
	EventRecord event;
	unsigned long timeoutTicks;
	long sleep;
	WindowPtr windowPtr;
	long menuSelection;
	long menuIndex;
	short itemIndex;
	Str255 itemText;
	int key;

	/* Convert waitSeconds to timeoutTicks. */
	timeoutTicks = TickCount() + (waitSeconds * 60);

	/* Loop until a key is pressed or timed out. */
	for (;;) {

		/* Blinky Blink? */
		_ConsoleBlink();

		/* How long should WaitNextEvent() sleep? */
		if ((sleep = timeoutTicks - TickCount()) < 0)
			sleep = 0;

		/* Wait for something to happen or time out. */
		if (!WaitNextEvent(everyEvent, &event, sleep, 0))
			if (TickCount() >= timeoutTicks)
				goto consoleNotReady;

		/* What happened? */
		switch (event.what) {

		/* A key was pressed... */
		case keyDown:
		case autoKey:
			/* Extract the character and key codes. */
			gConsoleInputCharCode =
				event.message & charCodeMask;
			gConsoleInputKeyCode =
				(event.message & keyCodeMask) >> 8;
			/* Error if an Option Key was pressed. */
			if (event.modifiers & optionKey)
				goto error;
			/* A Command Key was pressed. */
			if (event.modifiers & cmdKey) {
				switch (gConsoleInputCharCode) {
				/* Command-Q => QUIT */
				case 'q':
				case 'Q':
					goto consoleQuit;
				/* Command-M => MONITOR */
				case 'm':
				case 'M':
					goto consoleMonitor;
				/* Error if unsupported Command Key. */
				default:
					goto error;
				}
			}
			/* Check for Special Keys. */
			switch (gConsoleInputKeyCode) {
			case 0x33: key = DELETEKEY; break;
			case 0x75: key = BACKSPACEKEY; break;
			case 0x24: key = RETURNKEY; break;
			case 0x4C: key = RETURNKEY; break;
			case 0x74: key = PAGEUPKEY; break;
			case 0x79: key = PAGEDOWNKEY; break;
			case 0x77: key = F1KEY; break;
			case 0x7A: key = F1KEY; break;
			case 0x72: key = HELPKEY; break;
			case 0x78: key = F2KEY; break;
			case 0x63: key = F3KEY; break;
			case 0x76: key = F4KEY; break;
			case 0x60: key = F5KEY; break;
			case 0x61: key = F6KEY; break;
			case 0x62: key = F7KEY; break;
			case 0x64: key = F8KEY; break;
			case 0x65: key = F9KEY; break;
			case 0x7E: key = UPARROWKEY; break;
			case 0x7C: key = RIGHTARROWKEY; break;
			case 0x7B: key = LEFTARROWKEY; break;
			case 0x7D: key = DOWNARROWKEY; break;
			case 0x73: key = HOMEKEY; break;
			/* Error if unsupported Special Key. */
			case 0x6D:
			case 0x67:
			case 0x6F:
				goto error;
			/* A Standard Key was pressed. */
			default:
				key = gConsoleInputCharCode;
				break;
			}
			goto consoleKeyPressed;

		/* The mouse button was pressed... */
		case mouseDown:
			/* Where is the mouse cursor? */
			switch (FindWindow(event.where, &windowPtr)) {
			/* In the System Window... */
			case inSysWindow:
				SystemClick(&event, windowPtr);
				break;
			/* In the Menu Bar... */
			case inMenuBar:
				menuSelection = MenuSelect(event.where);
				menuIndex = (menuSelection & 0xFFFF0000) >> 16;
				itemIndex = (menuSelection & 0x0000FFFF) >> 0;
				HiliteMenu(0);
				/* Which menu? */
				switch (menuIndex) {
				/* The Apple Menu */
				case 1:
					/* Which menu item? */
					switch (itemIndex) {
					/* ABOUT... */
					case 1:
						printf(kConsoleAbout);
						break;
					/* Some other desk accessory. */
					default:
						GetMenuItemText(gAppleMenu,
						                itemIndex,
						                itemText);
						OpenDeskAcc(itemText);
					}
					break;
				/* The File Menu */
				case 2:
					/* Which menu item? */
					switch (itemIndex) {
					/* QUIT */
					case 1:
						goto consoleQuit;
					/* MONITOR */
					case 2:
						goto consoleMonitor;
					/* Error if unsupported menu item. */
					default:
						goto error;
					}
				/* Error if unsupported menu. */
				default:
					goto error;
				}
				break;
			/* In Drag... */
			case inDrag:
				/* Do nothing if not the Console Window. */
				if (windowPtr != gConsolePort)
					break;
				/* Drag the Console Window. */
				DragWindow(gConsolePort,
				           event.where,
				           &qd.screenBits.bounds);
				break;
			/* In the Console Window... */
			case inContent:
				/* Do nothing if not the Console Window. */
				if (windowPtr != gConsolePort)
					break;
				/* Bring Console Window to the front if it is not. */
				if (windowPtr != FrontWindow()) {
					SelectWindow(gConsolePort);
					break;
				}
				/* Locate the Mouse Cursor. */
				GetPort(&savedPort);
				SetPort(gConsolePort);
				GlobalToLocal(&event.where);
				SetPort(savedPort);
				gConsole.mouseX = event.where.h / kFontWidth;
				if (gConsole.mouseX >= kConsoleMaxX)
					gConsole.mouseX = kConsoleMaxX - 1;
				gConsole.mouseY = event.where.v / kFontHeight;
				if (gConsole.mouseY >= kConsoleMaxY)
					gConsole.mouseY = kConsoleMaxY - 1;
				key = MOUSEKEY;
				goto consoleKeyPressed;
			}
			break;

		/* Update the Console Window... */
		case updateEvt:
			/* Do nothing if not the Console Window. */
			if ((WindowPtr)event.message != gConsolePort)
				break;
			BeginUpdate(gConsolePort);
			if (gIsFirstUpdateSkipped)
				printf(kConsoleRefreshScreen);
			else
				gIsFirstUpdateSkipped = 1;
			EndUpdate(gConsolePort);
			break;

		}

	}

	/* Not ready if there was an error. */
error:
	SysBeep(60);
	goto consoleNotReady;

	/* Timed out and nothing is ready. */
consoleNotReady:
	return kConsoleNotReady;

	/* QUIT */
consoleQuit:
	return kConsoleQuit;

	/* MONITOR */
consoleMonitor:
	return kConsoleMonitor;

	/* A key was pressed. */
consoleKeyPressed:
	return key;

}

/* _ConsoleClose() terminates access to the console. */
static inline void _ConsoleClose(void)
{

	/* Deallocate the Apple Menu. */
	if (gAppleMenu != 0) {
		DeleteMenu(1);
		DisposeMenu(gAppleMenu);
		gAppleMenu = 0;
	}

	/* Deallocate the File Menu. */
	if (gFileMenu != 0) {
		DeleteMenu(2);
		DisposeMenu(gFileMenu);
		gFileMenu = 0;
	}

	/* Deallocate the Update Region. */
	if (gUpdateRegion != 0) {
		DisposeRgn(gUpdateRegion);
		gUpdateRegion = 0;
	}

	/* Deallocate the Console Window. */
	if (gConsolePort != 0) {
		DisposeWindow(gConsolePort);
		gConsolePort = 0;
	}

}

/* _ConsoleOpen() prepares access to the console. */
static inline int _ConsoleOpen(void)
{
	static Str255 pWindowTitle;
	static Str255 pAboutTitle;
	static int isInit;
	static WindowRecord windowRecord;
	GrafPtr savedPort;
	Rect boundsRect;
	FontInfo fontInfo;
	static int macInit;
	short fontHeight;
	short fontWidth;
	short fontAscent;
	short top;
	short left;
	short right;
	short bottom;
	short dh;
	short dv;

	gAppleMenu = 0;
	gFileMenu = 0;
	gUpdateRegion = 0;
	gConsolePort = 0;

	/* Do initializations once. */
	if (!isInit) {
		MaxApplZone();
		InitGraf(&qd.thePort);
		InitFonts();
		InitWindows();
		InitDialogs(0);
		InitMenus();
		InitCursor();
		isInit = 1;
	}

	/* Save the Window Title as a Pascal String. */
	CStrToPStr(pWindowTitle, gConsole.windowTitle);

	/* Save the About Title as a Pascal String. */
	CStrToPStr(pAboutTitle, gConsole.aboutTitle);

	/* Set the dimensions of the Bounds Rectangle. */
	top = 0;
	left = 0;
	right = kConsoleMaxX * kFontWidth + kLeftMargin * 2;
	bottom = kConsoleMaxY * kFontHeight + kTopMargin * 2;
	SetRect(&boundsRect, top, left, right, bottom);

	/* Center the Bounds Rectangle on the Screen. */
	dh = (qd.screenBits.bounds.right - boundsRect.right) / 2;
	dv = (qd.screenBits.bounds.bottom - boundsRect.bottom) / 2 + 10;
	OffsetRect(&boundsRect, dh, dv);

	/* Allocate the Console Port. */
	gConsolePort =
		NewWindow(&windowRecord,
	              &boundsRect,
	              pWindowTitle,
	              0,
	              noGrowDocProc,
	              (WindowPtr)-1,
	              0,
	              0);
	if (gConsolePort == 0)
		goto error;

	/* Set the dimensions of the Scroll Rectangle. */
	gScrollRect = gConsolePort->portRect;
	gScrollRect.top += kTopMargin;
	gScrollRect.bottom -= kTopMargin;
	gScrollRect.left += kLeftMargin;
	gScrollRect.right -= kLeftMargin;

	/* Allocate the Update Region. */
	gUpdateRegion = NewRgn();
	if (gUpdateRegion == 0)
		goto error;

	/* Save the current port. */
	GetPort(&savedPort);

	/* Use the Console Port. */
	SetPort(gConsolePort);

	/* Set the Text Font, Size, Face and Mode. */
	TextFont(kTextFont);
	TextSize(kTextSize);
	TextFace(0);
	TextMode(srcCopy);

	/* Set the Pen Size. */
	PenSize(1, 1);

	/* Verify the kFontHeight, kFontWidth and kFontAscent. */
	GetFontInfo(&fontInfo);
	fontHeight =
		fontInfo.ascent + fontInfo.descent + fontInfo.leading;
	fontWidth = fontInfo.widMax;
	fontAscent = fontInfo.ascent;
	if (kFontHeight != fontHeight)
		goto error;
	if (kFontWidth != fontWidth)
		goto error;
	if (kFontAscent != fontAscent)
		goto error;

	/* Set the dimensions of the Cursor Rectangle. */
	SetRect(&gCursorRect, 0, 0, kFontWidth, kFontHeight);

	/* Prepare the Apple Menu. */
	gAppleMenu = NewMenu(1, "\p\024");
	AppendMenu(gAppleMenu, pAboutTitle);
	AppendResMenu(gAppleMenu, 'DRVR');
	InsertMenu(gAppleMenu, 0);

	/* Prepare the File Menu. */
	gFileMenu = NewMenu(2, "\pFile");
	AppendMenu(gFileMenu, "\pQuit/Q");
	AppendMenu(gFileMenu, "\pMonitor/M");
	InsertMenu(gFileMenu, 0);

	/* Draw the Menu Bar. */
	DrawMenuBar();	

	/* Set the background color. */
	BackColor(kBackColor);

	/* Display the blank window. */
	ShowWindow(gConsolePort);

	/* Skip the first window update. */
	gIsFirstUpdateSkipped = 0;

	/* Flush any pre-existing events. */
	FlushEvents(everyEvent, 0);

	/* Prepare the Blink State. */
	gBlinkState = 0;
	gBlinkTickCount = TickCount();

	/* Restore the Saved Port. */
	SetPort(savedPort);

	/* The MacOS Console is open. */
	gConsole.isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	_ConsoleClose();
	return 0;

}

#endif

/**********************************************************************/
#pragma mark *** DOS CONSOLE ***

#ifdef DOS

/* _ConsoleBeep() beeps the beep-beep. */
static inline void _ConsoleBeep(void)
{
}

/* _ConsoleDraw() draws a character. */
static inline void _ConsoleDraw(void)
{
}

/* _ConsoleCursor() draws the cursor. */
static void _ConsoleCursor(int entry)
{
#pragma unused(entry)
}

/* _ConsoleMove() moves the cursor. */
static inline void _ConsoleMove(void)
{
}

/* _ConsoleClearScreen() clears the screen. */
static inline void _ConsoleClearScreen(void)
{
}

/* _ConsoleScrollUp() scrolls the screen up one line. */
static inline void _ConsoleScrollUp(void)
{
}

/* _ConsoleInput() inputs a keystroke. */
static inline int _ConsoleInput(unsigned waitSeconds)
{
#pragma unused(waitSeconds)
	int key = 0;

	goto consoleQuit;

	/* Timed out and nothing is ready. */
consoleNotReady:
	return kConsoleNotReady;

	/* QUIT */
consoleQuit:
	return kConsoleQuit;

	/* MONITOR */
consoleMonitor:
	return kConsoleMonitor;

	/* A key was pressed. */
consoleKeyPressed:
	return key;

}

/* _ConsoleClose() terminates access to the console. */
static inline void _ConsoleClose(void)
{
}

/* _ConsoleOpen() prepares access to the console. */
static inline int _ConsoleOpen(void)
{

	goto error;

	/* The MacOS Console is open. */
	gConsole.isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	_ConsoleClose();
	return 0;

}

#endif

/**********************************************************************/
#pragma mark *** UNIX CONSOLE ***

#if defined(SGTTY) || defined(TERMIOS)

#include <fcntl.h>
#include <sys/file.h>

#ifndef SYSV
#include <sys/time.h>
#endif

#ifdef SGTTY
#include <sh>
#endif

#ifdef TERMIOS
#include <curses.h>
#include <term.h>
#include <unistd.h>
#include <termio.h>
#endif

#ifdef SYSV
#include <sys/select.h>
#endif

static char *gTERM = 0;
static char *gBAUD = "9600";
static char *gTTY = "/dev/tty";

typedef struct KeySequence KeySequence;
typedef struct KeySequence {
	char *sequence;
	char key;
} KeySequence, *KeySequencePtr;

static int fd = -1;

static int tc_count = 0;
static char tc_buffer[256];

static char tc_graphic[11];
static char *tc_ae;
static char *tc_as;
static char *tc_bc;
static char *tc_bl;
static char *tc_cl;
static char *tc_cm;
static int tc_co;
static int tc_li;
static char *tc_se;
static char *tc_so;
static char *tc_up;
static char *tc_do;
static char *tc_nd;
static char *tc_le;

static int tc_x;
static int tc_y;

static int isStandout;
static int isGraphic;

static KeySequencePtr keys;

#ifdef SGTTY
static struct sgttyb savedSgttyb;
#endif
#ifdef TERMIOS
static struct termios savedTermios;
#endif

#if 0
extern char *getenv(char *);
extern char *tgoto(char *, int, int);
extern void tputs(char *, int, void (*)(char));
extern int tgetent(char *, char *);
extern int tgetnum(char *);
extern char *tgetstr(char *, char **);
#endif

static void tc_flush(int dropBuffer)
{
	int offset;
	int n;

	if (dropBuffer)
		tc_count = 0;

	for (offset = 0; tc_count > 0; tc_count -= n) {
		n = write(fd, &tc_buffer[offset], tc_count);
		if (n < 0)
			n = 0;
		offset += n;
	}

}

static int tc_putc(int c)
{

	if (tc_count >= sizeof(tc_buffer))
		tc_flush(0);

	tc_buffer[tc_count++] = c;

  return 0;
}

static int tc_read(long msec, char *buffer, int count)
{
	fd_set readfds;
	struct timeval timeout;

	/* Wait until a key has been pressed. */
	if (msec > 0) {
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		timeout.tv_sec = msec / 1000;
		timeout.tv_usec = (msec % 1000) * 1000;
		select(fd + 1, &readfds, 0, 0, &timeout);
	}

	/* Read the key code. */
	if ((count = read(fd, buffer, count)) < 0)
		goto error;

	/* All done, no error, return non-zero. */
	return count;

	/* Return zero if there was an error. */
error:
	return 0;

}

static void tc_ANSI(void)
{
	static KeySequence _keys[] = {
		"\033\133\115", F1KEY,
		"\033\133\116", F2KEY,
		"\033\133\117", F3KEY,
		"\033\133\120", F4KEY,
		"\033\133\121", F5KEY,
		"\033\133\122", F6KEY,
		"\033\133\123", F7KEY,
		"\033\133\124", F8KEY,
		"\033\133\125", F9KEY,
		"\033\133\111", PAGEUPKEY,
		"\033\133\107", PAGEDOWNKEY,
		"\033\133\101", UPARROWKEY,
		"\033\133\102", DOWNARROWKEY,
		"\033\133\103", RIGHTARROWKEY,
		"\033\133\104", LEFTARROWKEY,
		"\033\033", kConsoleMonitor,
		"\033\033\033", kConsoleMonitor,
		0, 0
	};
	int base;

	keys = _keys;

	tc_ae = 0;
	tc_as = 0;
	tc_bc = 0;
	tc_bl = "\007";
	tc_cl = "\033[2J\033[H";
	tc_cm = "\033[%i%d;%dH";
	tc_co = 80;
	tc_li = 25;
	tc_se = "\033[m";
	tc_so = "\033[7m";
	tc_up = "\033[A";
	tc_do = "\033[B";

	base = LOWERRIGHTCORNER;
	tc_graphic[LOWERRIGHTCORNER - base] = '+';
	tc_graphic[UPPERRIGHTCORNER - base] = '+';
	tc_graphic[UPPERLEFTCORNER - base] = '+';
	tc_graphic[LOWERLEFTCORNER - base] = '+';
	tc_graphic[CROSS - base] = '+';
	tc_graphic[HORIZONTALLINE - base] = '-';
	tc_graphic[TEERIGHT - base] = '+';
	tc_graphic[TEELEFT - base] = '+';
	tc_graphic[TEEUP - base] = '+';
	tc_graphic[TEEDOWN - base] = '+';
	tc_graphic[VERTICALLINE - base] = '|';

}

static void tc_VT100(void)
{
	int base;

	keys = 0;

	tc_ae = "\033(B";
	tc_as = "\033(0";
	tc_bc = 0;
	tc_bl = "\007";
	tc_cl = "50\033[;H\033[2J";
	tc_cm = "5\033[%i%d;%dH";
	tc_co = 80;
	tc_li = 24;
	tc_se = "2\033[m";
	tc_so = "2\033[7m";
	tc_up = "2\033[A";
	tc_do = "2\033[B";

	base = LOWERRIGHTCORNER;
	tc_graphic[LOWERRIGHTCORNER - base] = 'j';
	tc_graphic[UPPERRIGHTCORNER - base] = 'k';
	tc_graphic[UPPERLEFTCORNER - base] = 'l';
	tc_graphic[LOWERLEFTCORNER - base] = 'm';
	tc_graphic[CROSS - base] = 'n';
	tc_graphic[HORIZONTALLINE - base] = 'q';
	tc_graphic[TEERIGHT - base] = 't';
	tc_graphic[TEELEFT - base] = 'u';
	tc_graphic[TEEUP - base] = 'v';
	tc_graphic[TEEDOWN - base] = 'w';
	tc_graphic[VERTICALLINE - base] = 'x';

}

static int tc_TERMCAP(char *term)
{
	static char area[1024];
	char *ap = area;
	char bp[1024];
	int base;
#ifdef TERMCAP
	extern char *UP;
	extern char *BC;
#endif

	/* Get the TERMCAP entry. */
	if (tgetent(bp, term) != 1)
		goto error;

	keys = 0;

	tc_ae = 0;
	tc_as = 0;
	tc_bc = tgetstr("bc", &ap);
	if ((tc_bl = tgetstr("bl", &ap)) == 0)
		goto error;
	if ((tc_cl = tgetstr("cl", &ap)) == 0)
		goto error;
	if ((tc_cm = tgetstr("cm", &ap)) == 0)
		goto error;
	if ((tc_co = tgetnum("co")) == -1);
		goto error;
	if ((tc_li = tgetnum("li")) == -1);
		goto error;
	if ((tc_se = tgetstr("se", &ap)) == 0)
		goto error;
	if ((tc_so = tgetstr("so", &ap)) == 0)
		goto error;
	if ((tc_up = tgetstr("up", &ap)) == 0)
		goto error;
	if ((tc_do = tgetstr("do", &ap)) == 0)
		goto error;
	if ((tc_nd = tgetstr("nd", &ap)) == 0)
		goto error;
	if ((tc_le = tgetstr("le", &ap)) == 0)
		goto error;

	base = LOWERRIGHTCORNER;
	tc_graphic[LOWERRIGHTCORNER - base] = '+';
	tc_graphic[UPPERRIGHTCORNER - base] = '+';
	tc_graphic[UPPERLEFTCORNER - base] = '+';
	tc_graphic[LOWERLEFTCORNER - base] = '+';
	tc_graphic[CROSS - base] = '+';
	tc_graphic[HORIZONTALLINE - base] = '-';
	tc_graphic[TEERIGHT - base] = '+';
	tc_graphic[TEELEFT - base] = '+';
	tc_graphic[TEEUP - base] = '+';
	tc_graphic[TEEDOWN - base] = '+';
	tc_graphic[VERTICALLINE - base] = '|';

#ifdef TERMCAP
	BC = tc_bc;
	UP = tc_up;
#endif

	return 1;

error:
	return 0;

}

/* _ConsoleBeep() beeps the beep-beep. */
static inline void _ConsoleBeep(void)
{

	tputs(tc_bl, 1, tc_putc);

}

/* _ConsoleDraw() draws a character. */
static inline void _ConsoleDraw(void)
{
	unsigned c = gConsole.drawChar;
#if defined(ADM31) || defined(COLOR)
	unsigned mode = gConsole.drawMode;
	int standout = (mode & kConsoleModeReverse);
#else
	int standout = 0;
#endif

	if (standout != isStandout)
		tputs((isStandout = standout) ? tc_so : tc_se, 1, tc_putc);

	if ((c >= LOWERRIGHTCORNER) && (c <= VERTICALLINE)) {
		c = tc_graphic[c - LOWERRIGHTCORNER];
		if (!isGraphic) {
			if (tc_as)
				tputs(tc_as, 1, tc_putc);
			isGraphic++;
		}
	}
	else if (isGraphic) {
		if (tc_ae)	
			tputs(tc_ae, 1, tc_putc);
		isGraphic = 0;;
	}

	if (standout)
		tputs(tc_so, 1, tc_putc);

	tc_putc((char)c);

	tc_x++;

}

/* _ConsoleCursor() draws the cursor. */
static void _ConsoleCursor(int entry)
{

	/* Display or hide the cursor at ConsoleOutput() exit. */
	if (!entry) {
		/* Hide the cursor? */
		if ((gConsole.cursorMode & kConsoleModeCursor) == 0) {
			tputs(tgoto(tc_cm,
			            tc_x = 0,
			            tc_y = 0),
			      1,
			      tc_putc);
		}
		/* Make it all happen now. */
		tc_flush(0);
	}

}

/* _ConsoleMove() moves the cursor. */
static inline void _ConsoleMove(void)
{
	int dx = gConsole.cursorX - tc_x;
	int dy = gConsole.cursorY - tc_y;

	if ((dx < -1) || (dx > 1) || (dy < -1) || (dy > 1)) {
		tputs(tgoto(tc_cm,
		            tc_x = gConsole.cursorX,
		            tc_y = gConsole.cursorY),
		      1,
		      tc_putc);
	}
	else {
		if (dx > 0)
			tputs(tc_nd, 1, tc_putc);
		else if (dx < 0)
			tputs(tc_le, 1, tc_putc);
		if (dy > 1)
			tputs(tc_do, 1, tc_putc);
		else if (dy < 0)
			tputs(tc_up, 1, tc_putc);
	}
}

/* _ConsoleClearScreen() clears the screen. */
static inline void _ConsoleClearScreen(void)
{

	tc_flush(1);

	tputs(tc_cl, tc_li, tc_putc);
	tputs(tc_se, 1, tc_putc);
	if (tc_ae != 0)
		tputs(tc_ae, 1, tc_putc);

	isStandout = 0;
	isGraphic = 0;

	gConsole.cursorX = tc_x = 0;
	gConsole.cursorY = tc_y = 0;

	tc_flush(0);

}

/* _ConsoleScrollUp() scrolls the screen up one line. */
static inline void _ConsoleScrollUp(void)
{

	tputs(tc_do,1, tc_putc);

}

#define seconds 1000000

static void tty_beep() {
  fprintf(stderr, "\007");
}

/* _ConsoleInput() inputs a keystroke. */
static inline int _ConsoleInput(unsigned waitSeconds)
{
	char sequence[6];
	KeySequencePtr s;
	int n;
	int i;
	int key;

	tc_flush(0);

	memset(sequence, 0, sizeof(sequence));

	if (!tc_read(seconds * 1000, sequence, 1))
		goto consoleNotReady;

	if (keys == 0) {
		key = *sequence & 0xFF;
		goto consoleKeyPressed;
	}

	for (s = keys; s->sequence; s++) {
		if (*(s->sequence) != *sequence)
			continue;
		for (i = 1; i < sizeof(sequence); i += n) {
			for (s = keys; s->sequence; s++)
				if (!strncmp(s->sequence, sequence, sizeof(sequence))) {
					key = s->key & 0xFF;
					goto consoleKeyPressed;
				}
			n = tc_read(50, &sequence[i], sizeof(sequence) - i);
			if (n = 0) {
				if (i == 1) {
					key = *sequence & 0xFF;
					goto consoleKeyPressed;
				}
				tty_beep();
				goto consoleNotReady;
			}
		}
		key = *sequence & 0xFF;
		goto consoleKeyPressed;
	}

	key = *sequence & 0xFF;
	goto consoleKeyPressed;

	/* Timed out and nothing is ready. */
consoleNotReady:
	return kConsoleNotReady;

	/* QUIT */
consoleQuit:
	return kConsoleQuit;

	/* MONITOR */
consoleMonitor:
	return kConsoleMonitor;

	/* A key was pressed. */
consoleKeyPressed:
	return key;

}

/* _ConsoleClose() terminates access to the console. */
static inline void _ConsoleClose(void)
{

	if (fd >= 0) {
		tc_flush(0);
#ifdef SGTTY
		(void)ioctl(fd, TIOCSETP, (char *)&savedSgttyb);
#endif
#ifdef TERMIOS
		(void)tcsetattr(fd, 0, &savedTermios);
#endif
		(void)close(fd);
		fd = -1;
	}

}

/* _ConsoleOpen() prepares access to the console. */
static inline int _ConsoleOpen(void)
{
#ifdef SGTTY
	struct sgttyb sgttyb;
#endif
#ifdef TERMIOS
	struct termios termios;
#endif
	int fl;
#ifndef SYSV
	extern short ospeed;
#endif

	if (gTERM == 0)
		gTERM = getenv("TERM");

	if (gTERM == 0)
		goto error;

	if (!strcmp(gTERM, "ansi"))
		tc_ANSI();
	else if (!strcmp(gTERM, "vt100"))
		tc_VT100();
	else if (!strcmp(gTERM, "xterm"))
		tc_VT100();
	else if (!tc_TERMCAP(gTERM))
		goto error;

	if (tc_co < kConsoleMaxX)
		goto error;

	if (tc_li < kConsoleMaxY)
		goto error;

	if ((fd = open(gTTY, O_RDWR)) == -1)
		goto error;

	if ((fl = fcntl(fd, F_GETFL, 0)) < 0)
		goto error;
	if (fcntl(fd, F_SETFL, fl | FNDELAY) < 0)
		goto error;

#ifdef SGTTY
	if (ioctl(fd, TIOCGETP, (char *)&savedSgttyb) < 0)
		goto error;
	if (ioctl(fd, TIOCGETP, (char *)&sgttyb) < 0)
		goto error;
	sgttyb.sg_flags |= RAW;
	sgttyb.sg_flags |= CBREAK;
	sgttyb.sg_flags &= ~ECHO;
	sgttyb.sg_flags &= ~XTABS;
	if (gBAUD == 0)
		goto error;
	else if (!strcmp(gBAUD, "2400"))
		sgttyb.sg_ospeed = B2400;
	else if (!strcmp(gBAUD, "9600"))
		sgttyb.sg_ospeed = B9600;
	else if (!strcmp(gBAUD, "19200"))
		sgttyb.sg_ospeed = EXTA;
	else
		goto error; 
	if (ioctl(fd, TIOCSETP, (char *)&sgttyb) < 0)
		goto error;
	ospeed = sgttyb.sg_ospeed;
#endif

#ifdef TERMIOS
	if (tcgetattr(fd, &savedTermios) < 0)
		goto error;
	if (tcgetattr(fd, &termios) < 0)
		goto error;
	termios.c_lflag = 0;
	termios.c_iflag = IGNBRK | IGNPAR | ISTRIP;
	termios.c_oflag = 0;
	termios.c_cc[VINTR] = 0;
	termios.c_cc[VMIN] = 0;
	termios.c_cc[VTIME] = 0;
	termios.c_cflag &= ~CBAUD;
	if (gBAUD == 0)
		goto error;
	else if (!strcmp(gBAUD, "2400"))
		termios.c_cflag |= B2400;
	else if (!strcmp(gBAUD, "9600"))
		termios.c_cflag |= B9600;
	else if (!strcmp(gBAUD, "19200"))
		termios.c_cflag |= B19200;
	else
		goto error; 
	if (tcsetattr(fd, TCSAFLUSH, &termios) < 0)
		goto error;
#endif

	tc_count = 0;

	/* The Unix Console is open. */
	gConsole.isOpen = 1;

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	_ConsoleClose();
	return 0;

}

#endif

/**********************************************************************/
#pragma mark *** CONSOLE ***

/* ConsoleDraw() draws a character at the Cursor. */
static void ConsoleDraw(unsigned c)
{

	/* Save the character for ConsoleRefresh(). */
	gConsole.buffer(gConsole.cursorX, gConsole.cursorY) = c;

	/* Extract the display mode from the character. */
#if defined(ADM31) || defined(COLOR)
	gConsole.drawMode = c >> 8;
#endif
	gConsole.drawChar = c & 0xFF;

	/* Draw the character. */
	_ConsoleDraw();

}

/* ConsoleSetMode() sets the Display Mode. */
static void ConsoleSetMode(unsigned on, unsigned off)
{

	/* Set the Cursor Mode? */
	if ((on | off) & kConsoleModeCursor) {
		gConsole.cursorMode &= ~kConsoleModeCursor;
		gConsole.cursorMode |= on & kConsoleModeCursor;
	}
	/* Set the Display Mode? */
#if defined(ADM31) || defined(COLOR)
	else {
#ifdef ADM31
		if ((on | off) & kConsoleModeUnderline) {
			gConsole.charMode &= ~kConsoleModeUnderline;
			gConsole.charMode |= on & kConsoleModeUnderline;
		}
		if ((on | off) & kConsoleModeBlink) {
			gConsole.charMode &= ~kConsoleModeBlink;
			gConsole.charMode |= on & kConsoleModeBlink;
		}
		if ((on | off) & kConsoleModeReverse) {
			gConsole.charMode &= ~kConsoleModeReverse;
			gConsole.charMode |= on & kConsoleModeReverse;
		}
#endif
#ifdef COLOR
		if (on & 0x07) {
			gConsole.charMode &= ~0x07;
			gConsole.charMode |= on & 0x07;
		}
#endif
	}
#endif

}

/* ConsoleMove() moves the Cursor. */
static void ConsoleMove(unsigned x, unsigned y)
{

	if (x >= kConsoleMaxX)
		return;
	if (y >= kConsoleMaxY)
		return;

	gConsole.cursorX = x;

	gConsole.cursorY = y;

	_ConsoleMove();

}

/* ConsoleRefresh() refreshes the Screen. */
static void ConsoleRefresh(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;
	unsigned y;

	/* Clear the Screen. */
	_ConsoleClearScreen();

	/* Re-draw the Screen. */
	for (y = 0; y < kConsoleMaxY; y++) {
		for (x = 0; x < kConsoleMaxX; x++) {
			ConsoleMove(x, y);
			ConsoleDraw(gConsole.buffer(x, y));
		}
	}

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

/* ConsoleClearScreen() clears the Screen. */
static void ConsoleClearScreen(void)
{
	unsigned x;
	unsigned y;

#if defined(ADM31) || defined(COLOR)
#ifdef COLOR
	ConsoleSetMode(kConsoleColor, 0xF7);
	ConsoleSetMode(kConsoleModeCursor | kConsoleColor, 0xFF);
#else
	ConsoleSetMode(0, 0xF0);
	ConsoleSetMode(kConsoleModeCursor, 0xFF);
#endif
#else
	ConsoleSetMode(kConsoleModeCursor, 0);
#endif

	ConsoleMove(0, 0);

	for (y = 0; y < kConsoleMaxY; y++)
		for (x = 0; x < kConsoleMaxX; x++)
			gConsole.buffer(x, y) = ' ';

	ConsoleRefresh();

}

/* ConsoleSaveScreen() saves the Current Screen. */
static inline void ConsoleSaveScreen(void)
{
	unsigned x;
	unsigned y;

	/* Save the Current Screen for ConsoleRestoreScreen(). */

	for (y = 0; y < kConsoleMaxY; y++)
		for (x = 0; x < kConsoleMaxX; x++)
			gConsole.saved.buffer(x, y) = gConsole.buffer(x, y);

	gConsole.saved.cursorX = gConsole.cursorX;
	gConsole.saved.cursorY = gConsole.cursorY;

	gConsole.saved.cursorMode = gConsole.cursorMode;

#if defined(ADM31) || defined(COLOR)
	gConsole.saved.charMode = gConsole.charMode;
#endif

}

/* ConsoleRestoreScreen() restores the Previous Screen. */
static inline void ConsoleRestoreScreen(void)
{
	unsigned x;
	unsigned y;

	/* Restore the Previous Screen from ConsoleSaveScreen(). */

	for (y = 0; y < kConsoleMaxY; y++)
		for (x = 0; x < kConsoleMaxX; x++)
			gConsole.buffer(x, y) = gConsole.saved.buffer(x, y);

#if defined(ADM31) || defined(COLOR)
	gConsole.charMode = gConsole.saved.charMode;
#endif

	gConsole.cursorMode = gConsole.saved.cursorMode;

	ConsoleMove(gConsole.saved.cursorX, gConsole.saved.cursorY);

	ConsoleRefresh();

}

/* ConsoleMoveLeft() moves the cursor to the left. */
static inline void ConsoleMoveLeft(void)
{

	ConsoleMove(gConsole.cursorX - 1, gConsole.cursorY);

}

/* ConsoleMoveRight() moves the cursor to the right. */
static inline void ConsoleMoveRight(void)
{

	ConsoleMove(gConsole.cursorX + 1, gConsole.cursorY);

}

/* ConsoleTab() moves the Cursor right to the next Tab Stop. */
static inline void ConsoleTab(void)
{
	int x;

	ConsoleMove(gConsole.cursorX + 1, gConsole.cursorY);

	x = gConsole.cursorX;
	while (x % 8)
		x++;

	ConsoleMove(x, gConsole.cursorY);

}

/* ConsoleMoveUp() moves the Cursor Up. */
static inline void ConsoleMoveUp(void)
{

	ConsoleMove(gConsole.cursorX, gConsole.cursorY - 1);

}

/* ConsoleLineFeed() moves the Cursor Down, Scrolling Up if needed. */
static void ConsoleLineFeed(void)
{

	if (gConsole.cursorY == (kConsoleMaxY - 1)) {
		unsigned x;
		unsigned y;
		_ConsoleScrollUp();
		for (y = 0; y < (kConsoleMaxY - 1); y++)
			for (x = 0; x < kConsoleMaxX; x++)
				gConsole.buffer(x, y) = gConsole.buffer(x, y + 1);

		for (x = 0; x < kConsoleMaxX; x++)
		    gConsole.buffer(x, kConsoleMaxY - 1) = ' ';
	}
	
	ConsoleMove(gConsole.cursorX, gConsole.cursorY + 1);

}

/* ConsoleCarriageReturn() moves the Cursor to the left margin. */
static void ConsoleCarriageReturn(void)
{

	ConsoleMove(0, gConsole.cursorY);

}

/* ConsoleCleanLine() does a CRLF if not at the left margin. */
static inline void ConsoleCleanLine(void)
{

	if (gConsole.cursorX > 0) {
		ConsoleCarriageReturn();
		ConsoleLineFeed();
	}

}

#ifdef ADM31

/* ConsoleClearToEndOfLine() clears from Cursor to End of Line. */
static inline void ConsoleClearToEndOfLine(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;

	/* Clear from the Cursor until the End of Line. */
	for (x = cursorX; x < kConsoleMaxX; x++) {
		ConsoleMove(x, cursorY);
		ConsoleDraw(' ');
	}

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

/* ConsoleClearToEndOfScreen() clears from Cursor to End of Screen. */
static inline void ConsoleClearToEndOfScreen(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;
	unsigned y;

	/* Clear from the Cursor until the End of Line. */
	for (x = cursorX; x < kConsoleMaxX; x++) {
		ConsoleMove(x, cursorY);
		ConsoleDraw(' ');
	}

	/* Clear from the Cursor until the End of Screen. */
	for (y = cursorY + 1; y < kConsoleMaxY; y++) {
		for (x = 0; x < kConsoleMaxX; x++) {
			ConsoleMove(x, y);
			ConsoleDraw(' ');
		}
	}

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

/* ConsoleDeleteLine() deletes the line at the Cursor. */
static inline void ConsoleDeleteLine(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;
	unsigned y;

	/* Scroll up to fill the gap. */
	for (y = cursorY; y < (kConsoleMaxY - 1); y++) {
		for (x = 0; x < kConsoleMaxX; x++) {
			ConsoleMove(x, y);
			ConsoleDraw(gConsole.buffer(x, y + 1));
		}
	}

	/* Clear the uncovered line. */
	for (x = 0; x < kConsoleMaxX; x++) {
		ConsoleMove(x, kConsoleMaxY - 1);
		ConsoleDraw(' ');
	}

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

/* ConsoleInsertLine() inserts a line at the cursor. */
static inline void ConsoleInsertLine(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;
	unsigned y;

	/* Scroll down to make room. */
	for (y = kConsoleMaxY - 1; y > cursorY; y--) {
		for (x = 0; x < kConsoleMaxX; x++) {
			ConsoleMove(x, y);
			ConsoleDraw(gConsole.buffer(x, y - 1));
		}
	}

	/* Clear the uncovered line. */
	for (x = 0; x < kConsoleMaxX; x++) {
		ConsoleMove(x, cursorY);
		ConsoleDraw(' ');
	}

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

/* ConsoleDeleteChar() deletes the character at the Cursor. */
static inline void ConsoleDeleteChar(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;

	/* Scroll left to fill the gap. */
	for (x = cursorX; x < (kConsoleMaxX - 1); x++) {
		ConsoleMove(x, cursorY);
		ConsoleDraw(gConsole.buffer(x + 1, cursorY));
	}

	/* Clear the uncovered character. */
	ConsoleMove(x, cursorY);
	ConsoleDraw(' ');

	/* Restore the Cursor. */
	ConsoleMove(kConsoleMaxX - 1, cursorY);

}

/* ConsoleInsertChar() inserts a character at the Cursor. */
static inline void ConsoleInsertChar(void)
{
	unsigned cursorX = gConsole.cursorX;
	unsigned cursorY = gConsole.cursorY;
	unsigned x;

	/* Scroll right to make room. */
	for (x = kConsoleMaxX - 1; x > cursorX; x--) {
		ConsoleMove(x, cursorY);
		ConsoleDraw(gConsole.buffer(x - 1, cursorY));
	}

	/* Clear the uncovered character. */
	ConsoleMove(x, cursorY);
	ConsoleDraw(' ');

	/* Restore the Cursor. */
	ConsoleMove(cursorX, cursorY);

}

#endif

/* ConsoleAbout() displays the About Text. */
static inline void ConsoleAbout(void)
{
	static int lock;

	/* Aquire the Lock to prevent re-entry. */
	if (!lock++) {

		/* Save the Current Screen. */
		printf(kConsoleSaveScreen);

		/* Clear the Screen. */
		printf(kConsoleClearScreen);

		/* Turn the Cursor OFF. */
		printf(kConsoleCursorOFF);

		/* Display the About Text. */
		printf("%s", gConsole.aboutText);

		/* Wait for confirmation. */
		printf(kConsoleMoveCursor(0,23));
		printf("Press any key and smile to continue.");
		(void)getchar();

		/* Restore the Previous Screen. */
		printf(kConsoleRestoreScreen);

	}

	/* Release the Lock. */
	--lock;

}

/* ConsoleOutput() outputs a character to the Console Display. */
int ConsoleOutput(int c)
{

	if (!gConsole.isOpen)
		goto error;

	_ConsoleCursor(1);

	if (gConsole.escapeState == 1) {
		gConsole.escapeState = 0;
		switch (c) {
		case '=': /* STANDARD: ^[=RC -- Move Cursor */
			gConsole.escapeState = 2;
			break;
#ifdef ADM31
		case 'T': /* ADM31: ^[T -- Clear to End of Line */
		case 't': /* ADM31: ^[t -- Clear to End of Line */
			ConsoleClearToEndOfLine();
			break;
		case 'Y': /* ADM31: ^[Y -- Clear to End of Screen */
		case 'y': /* ADM31: ^[y -- Clear to End of Screen */
			ConsoleClearToEndOfScreen();
			break;
		case ':': /* ADM31: ^[: -- Home & Clear Screen */
		case '*': /* ADM31: ^[* -- Home & Clear Screen */
			ConsoleClearScreen();
			break;
		case ')': /* ADM31: ^[) -- Reverse Video */
			ConsoleSetMode(kConsoleModeReverse, 0);
			break;
		case '(': /* ADM31: ^[( -- Normal Video */
			ConsoleSetMode(0, kConsoleModeReverse);
			break;
		case 'E': /* ADM31: ^[E -- Insert Line */
			ConsoleInsertLine();
			break;
		case 'Q': /* ADM31: ^[Q -- Insert Character */
			ConsoleInsertChar();
			break;
		case 'R': /* ADM31: ^[R -- Delete Line */
			ConsoleDeleteLine();
			break;
		case 'W': /* ADM31: ^[W -- Delete Character */
			ConsoleDeleteChar();
			break;
		case 'r': /* ADM31: ^[r -- Leave Insert Mode */
			break;
		case 'q': /* ADM31: ^[r -- Enter Insert Mode */
			break;
		case 'u': /* ADM31: ^[u -- ??? */
			break;
#endif
#if defined(ADM31) || defined(COLOR)
		case 'G': /* STANDARD: ^[GM -- Set Mode */
			gConsole.escapeState = 4;
			break;
#endif
		case '0': /* NON-STANDARD: ^[0 -- Clean Line */
			ConsoleCleanLine();
			break;
		case '1': /* NON-STANDARD: ^[1 -- Refresh Screen */
			ConsoleRefresh();
			break;
		case '2': /* NON-STANDARD: ^[2 -- Cursor ON */
			ConsoleSetMode(kConsoleModeCursor, 0);
			break;
		case '3': /* NON-STANDARD: ^[3 -- Cursor OFF */
			ConsoleSetMode(0, kConsoleModeCursor);
			break;
		case '4': /* NON-STANDARD: ^[4 -- Save Screen */
			ConsoleSaveScreen();
			break;
		case '5': /* NON-STANDARD: ^[5 -- Restore Screen */
			ConsoleRestoreScreen();
			break;
		case '6': /* NON-STANDARD: ^[6 -- Display "About" Text */
			ConsoleAbout();
			break;
		}
	}
	else if (gConsole.escapeState == 2) {
		/* ^[=RC -- Move Cursor */
		/* R = row + ' ', 0,0 is Upper Left */
		gConsole.escapeState = 3;
		gConsole.row = c - ' ';
	}
	else if (gConsole.escapeState == 3) {
		/* ^[=RC -- Move Cursor */
		/* C = column + ' ', 0,0 is Upper Left */
		gConsole.escapeState = 0;
		ConsoleMove(c - ' ', gConsole.row);
	}
#if defined(ADM31) || defined(COLOR)
	else if (gConsole.escapeState == 4) {
		gConsole.escapeState = 0;
		switch (c) {
#ifdef ADM31
		case '0': /* ADM31: ^[G0 -- Reset Mode */
			ConsoleSetMode(0,
			               kConsoleModeReverse |
			               kConsoleModeBlink |
			               kConsoleModeUnderline);
			break;
		case '1': /* ADM31: ^[G1 -- Underline */
			ConsoleSetMode(kConsoleModeUnderline, 0);
			break;
		case '2': /* ADM31: ^[G2 -- Blinking */
			ConsoleSetMode(kConsoleModeBlink, 0);
			break;
		case '4': /* ADM31: ^[G4 -- Reverse Video */
			ConsoleSetMode(kConsoleModeReverse, 0);
			break;
#endif
#ifdef COLOR
		case 'C': /* NON-STANDARD: ^[GC -- Set Color */
			gConsole.escapeState = 5;
			break;
#endif
		}
	}
#ifdef COLOR
	else if (gConsole.escapeState == 5) {
		gConsole.escapeState = 0;
		switch (c) {
		case '0': /* NON-STANDARD: ^[GC0 -- Reset Color */
			ConsoleSetMode(kConsoleColor, 0);
			break;
		case '1': /* NON-STANDARD: ^[GC1 -- Set White */
			ConsoleSetMode(kConsoleWhite, 0);
			break;
		case '2': /* NON-STANDARD: ^[GC2 -- Set Red */
			ConsoleSetMode(kConsoleRed, 0);
			break;
		case '3': /* NON-STANDARD: ^[GC3 -- Set Green */
			ConsoleSetMode(kConsoleGreen, 0);
			break;
		case '4': /* NON-STANDARD: ^[GC4 -- Set Blue */
			ConsoleSetMode(kConsoleBlue, 0);
			break;
		case '5': /* NON-STANDARD: ^[GC5 -- Set Cyan */
			ConsoleSetMode(kConsoleCyan, 0);
			break;
		case '6': /* NON-STANDARD: ^[GC6 -- Set Magenta */
			ConsoleSetMode(kConsoleMagenta, 0);
			break;
		case '7': /* NON-STANDARD: ^[GC7 -- Set Yellow */
			ConsoleSetMode(kConsoleYellow, 0);
			break;
		}
	}
#endif
#endif
	else if (c == ('[' - '@')) /* STANDARD: ^[ -- Escape Sequence */
		gConsole.escapeState = 1;
	else if (c == ('Z' - '@')) /* STANDARD: ^Z -- Home, Clear Screen */
		ConsoleClearScreen();
	else if (c == ('G' - '@')) /* STANDARD: ^G -- Bell */
		_ConsoleBeep();
	else if (c == ('H' - '@')) /* STANDARD: ^H -- Cursor Left */
		ConsoleMoveLeft();
	else if (c == ('I' - '@')) /* STANDARD: ^I -- Tab */
		ConsoleTab();
	else if (c == ('J' - '@')) /* STANDARD: ^J -- Line Feed */
		ConsoleLineFeed();
	else if (c == ('K' - '@')) /* STANDARD: ^K -- Cursor Up */
		ConsoleMoveUp();
	else if (c == ('L' - '@')) /* STANDARD: ^L -- Cursor Right */
		ConsoleMoveRight();
	else if (c == ('M' - '@')) /* STANDARD: ^M -- Carriage Return */
		ConsoleCarriageReturn();
	else if (c == ('^' - '@')) /* STANDARD: ^^ -- Home Cursor */
		ConsoleMove(0, 0);
	else {
#if defined(ADM31) || defined(COLOR)
		ConsoleDraw((unsigned int)((gConsole.charMode << 8) | c));
#else
		ConsoleDraw(c);
#endif
		ConsoleMove(gConsole.cursorX + 1, gConsole.cursorY);
	}

	_ConsoleCursor(0);

	return 1;

error:
	return 0;

}

/* ConsolePushInput() pushes keystrokes onto the Input Stack. */
int ConsolePushInput(const char *format, ...)
{
	va_list ap;
	char buffer[256];
	int i;

	/* Error if the Console is not open.*/
	if (!gConsole.isOpen)
		goto error;

	/* Error if the Console is QUIT. */
	if (gConsole.isQuit)
		goto error;

	/* Format the arguments. */
	va_start(ap, format);
	ConsoleVSNPrintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	/* Push the keystrokes onto the Input Stack in reverse order. */
	for (i = strlen(buffer) - 1; i >= 0; i--) {
		if (!StackPush(&gConsoleInputStack, &buffer[i]))
			goto error;
	}

	/* All done, no error, return non-zero. */
	return 1;

	/* Return zero if there was an error. */
error:
	return 0;

}

/* ConsoleInput() inputs a character from the Console Keyboard. */
int ConsoleInput(unsigned waitSeconds)
{
	char key;

	/* QUIT if not open. */
	if (!gConsole.isOpen)
		key = kConsoleQuit;

	/* QUIT if was QUIT. */
	else if (gConsole.isQuit)
		key = kConsoleQuit;

	/* Input from the Console Stack if not empty. */
	else if (StackCount(&gConsoleInputStack) > 0)
		(void)StackPop(&gConsoleInputStack, &key);

	/* Otherwise input from the Console Keyboard. */
	else
		key = _ConsoleInput(waitSeconds);

	/* Remember QUIT. */
	if (key == kConsoleQuit)
		gConsole.isQuit = 1;

	/* ESCAPE-ESCAPE => MONITOR */
	if (key == '\033') {
		key = _ConsoleInput(1);
		if (key == '\033') {
			key = _ConsoleInput(1);
			if (key == '\033')
				key = kConsoleQuit;
			else
				key = kConsoleMonitor;
		}
		else {
			ConsolePushInput("%c", key);
			key = '\033';
		}
	}

	/* Map the delete key. */
	if (key == 0x7F)
		key = DELETEKEY;

	/* All done, return the input character. */
	return key;

}

/* ConsoleMouse() returns the (x,y) of the last mouse click. */
void ConsoleMouse(unsigned *x, unsigned *y)
{
	*x = gConsole.mouseX;
	*y = gConsole.mouseY;
}

/* ConsoleClose() terminates access to the Console. */
void ConsoleClose(void)
{

	if (gConsole.isOpen) {
		gConsole.isQuit = 0;
		printf(kConsoleCleanLine);
		if (ConsoleInput(2) != 0) {
			printf("PRESS ANY KEY TO EXIT.\n");
			(void)getchar();
		}
		_ConsoleClose();
	}
	gConsole.isOpen = 0;

	if (gConsole.windowTitle != 0) {
		free(gConsole.windowTitle);
		gConsole.windowTitle = 0;
	}

	if (gConsole.aboutTitle != 0) {
		free(gConsole.aboutTitle);
		gConsole.aboutTitle = 0;
	}

	if (gConsole.aboutText != 0) {
		free(gConsole.aboutText);
		gConsole.aboutText = 0;
	}

}

/* ConsoleOpen() prepares access to the console. */
int
	ConsoleOpen(const char *windowTitle,
	            const char *aboutTitle,
	            const char *aboutText)
{

	/* Terminate any existing Console. */
	ConsoleClose();

	gConsole.isOpen = 0;
	gConsole.isQuit = 0;
	gConsole.escapeState = 0;
	gConsole.cursorMode = 0;
#if defined(ADM31) || defined(COLOR)
	gConsole.charMode = 0;
#endif

	/* Reset the Console Input Stack. */
	StackReset(&gConsoleInputStack,
	           gConsoleInputStackData,
	           kConsoleInputStackSize,
	           sizeof(*gConsoleInputStackData));

	/* Save a copy of the Window Title. */
	gConsole.windowTitle = malloc(strlen(windowTitle) + 1);
	if (gConsole.windowTitle == 0)
		goto error;
	strcpy(gConsole.windowTitle, windowTitle);

	/* Save a copy of the About Title. */
	gConsole.aboutTitle = malloc(strlen(aboutTitle) + 1);
	if (gConsole.aboutTitle == 0)
		goto error;
	strcpy(gConsole.aboutTitle, aboutTitle);

	/* Save a copy of the About Text. */
	gConsole.aboutText = malloc(strlen(aboutText) + 1);
	if (gConsole.aboutText == 0)
		goto error;
	strcpy(gConsole.aboutText, aboutText);

	if (!_ConsoleOpen())
		goto error;

	printf(kConsoleClearScreen);

#ifdef ADM31
	printf("ADM31 ");
	printf("\033G1UNDERLINE\033G0 ");
	printf("\033G4REVERSE\033G0 ");
#else
	printf("CONSOLE ");
#endif
#ifdef COLOR
	printf(kConsoleColorRed "C");
	printf(kConsoleColorCyan "O");
	printf(kConsoleColorBlue "L");
	printf(kConsoleColorWhite "O");
	printf(kConsoleColorYellow "R");
	printf(kConsoleColorReset);
#endif
	printf("\n");

	printf(kConsoleSaveScreen);
	printf(kConsoleRestoreScreen);

	return 1;

error:
	ConsoleClose();
	return 0;

}
