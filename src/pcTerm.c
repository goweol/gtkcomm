/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Terminal
 *
 * NOTE: terminal buffer의 문자열은 항상 EUC-KR로 encoding되어있다.
 *
 * Copyright (C) 2000-2004, Nam SungHyun and various contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "config.h"
#include <ctype.h>	/* isalnum() */
#include <sys/ioctl.h>	/* ioctl() */

#include "pcMain.h"
#include "pcTerm.h"
#include "pcChat.h"
#include "mbyte.h"

/* definitions & declarations {{{1 */

#define TERM_DRAW_TIMEOUT  100

static int TermButtonPress(TermType *term, GdkEventButton *ev);
static int TermButtonRelease(TermType *term, GdkEventButton *ev);
static void TermExposeLine(TermType *term, int y, int x1, int x2);
static void TermMakeSane(void);
static int TermMotionNotify(TermType *term, GdkEventButton *ev);
static void TermQueueDraw(TermType *term);
static void TermReceived(TermType *term);
static void TermSane(TermType *term);
static int TermSelectionClear(TermType *term, GdkEventSelection *ev);
static void TermSelectionGet(TermType *term, GtkSelectionData *selData);
static void TermSizeChanged(TermType *term);
static int termGetWordStart(TermType *term, int x, int y);
static int termGetWordEnd(TermType *term, int x, int y);

guint32 EmulateMode = EMULATE_VT100;
gboolean MouseButtonSwap = FALSE;
guint MouseLeftButtonNum = 1, MouseRightButtonNum = 3;

TermType *Term = NULL;

/* terminal input handler tag */
GIOChannel *TermInputChannel = NULL;
guint TermInputID;

guint32 EnterConvert;
char *TermFontName;
gint32 TermRow;
gint32 TermCol;
gint32 TermSavedLines;
gint32 TermNormFG;
gint32 TermNormBG;
gboolean TermSilent;
gboolean TermAutoLineFeed;
gboolean TermStripLineFeed; /* when capture */
gboolean TermStripControlChar; /* when capture */
guint32 TermReadHack;
gboolean TermCanBlock;
gboolean TermUseBold;
gboolean TermUseUnder;
gboolean TermForce16Color;
char *LeftMouseClickStr;
gboolean BackspaceDeleteChar;

GdkColor *TermColor0;
GdkColor *TermColor1;
GdkColor *TermColor2;
GdkColor *TermColor3;
GdkColor *TermColor4;
GdkColor *TermColor5;
GdkColor *TermColor6;
GdkColor *TermColor7;
GdkColor *TermColor8;
GdkColor *TermColor9;
GdkColor *TermColor10;
GdkColor *TermColor11;
GdkColor *TermColor12;
GdkColor *TermColor13;
GdkColor *TermColor14;
GdkColor *TermColor15;
GdkColor *TermCursorFG;
GdkColor *TermCursorBG;
GdkColor *TermISelFG;

/* Cursor Blink */
gboolean TermCursorBlink;
guint32 TermCursorOnTime;
guint32 TermCursorOffTime;

char *RemoteCharset;

static GtkWidgetClass *ParentClass = NULL;
static GSList *TermKeyMapList = NULL;

enum { TERM_SIZE_CHANGED, TERM_LINE_CHANGED, TERM_RECEIVED, TERM_LAST_SIG };
static gint termSignals[TERM_LAST_SIG] = { 0 };

enum {
    TERM_GROUND, TERM_ESC, TERM_CSI, TERM_IYAGI,
    TERM_SCS0, TERM_IGNORE, TERM_DEC, TERM_OSC
};

#define TERM_UPDATE_MAX_REACHED(term) G_STMT_START{ \
	    if (term->maxReached < term->currY) \
	       term->maxReached = term->currY; \
	 }G_STMT_END
#define TERM_LINE_CHANGING(term) \
	    g_signal_emit(G_OBJECT(term), termSignals[TERM_LINE_CHANGED], 0)

#define SWAP(a,b)    G_STMT_START{ int _t_ = a; a = b; b = _t_; }G_STMT_END

static gboolean DoISelect = FALSE;
static gboolean DoSelect = FALSE;
static gboolean termSelectExist = FALSE;
static guint termSelectSize = 0;
static char *termSelectBuf = NULL;
char *TermWordSeperator;
static GdkColormap *ColorMap = NULL;

/* 현재 선택된 영역 (bx,by) <--> (ex,ey) */
static int termSelectBX, termSelectBY;
static int termSelectEX, termSelectEY;

/* 단어 선택 모드인 경우 최초에 선택한 단어의 (bx,ex)
 * 문자 선택 모드인 경우에는 최초에 선택한 문자가 한글인 경우 의미를 가짐
 */
static int termSelectWBX, termSelectWEX;

static int termSelectOldWBX, termSelectOldWEX;

/* 최초에 선택한 위치의 Y */
static int termSelectFY;

/* 문자 선택 모드인 경우 마우스가 움직한 후에 선택을 시작해야 하므로
 * 최초에 마우스를 누른 위치를 여기에 저장해 둔다.
 */
static GdkEventButton termSelClick;

/* 마우스의 xy 좌표를 추적 */
static int termMousePosX, termMousePosY;

/* 선택 모드 */
enum { TERM_SELECT_CHAR, TERM_SELECT_WORD, TERM_SELECT_LINE };
static int termSelectMode;

/* 이전에 어떤 방향으로 마우스가 움직이면서 선택했었는 지 */
enum { SEL_DIR_LEFT_UP, SEL_DIR_RIGHT_DOWN };
static int termSelDirection;

static int termCursorTimer = -1;

/* 현재는 _USE_POPUP_MENU_ 정의해도 하는 일이 없다. 훗날을 위한 것임. */
/* #define _USE_POPUP_MENU_ 1 */

static int termICSpotX = 0, termICSpotY = 0;
static int ximPos = 0;
static char oldximbuf[20];

/* TermGdkDrawText() {{{1 */
static void
TermGdkDrawText(GdkWindow *win, GdkGC *gc, int x, int y,
		guchar *buf, int len, TermType *term)
{
    register int i;
    int lineCenter;

    lineCenter = y - term->fontAscent + term->fontHeight / 2;
    while (len > 0)
    {
	for (i = 0; i < len && buf[i] > 10; i++)
	    ;
	if (i)
	{
	    gsize bw;
	    gchar *s;

	    if (gu_euckr_validate_conv((gchar*) buf, i, &s, &bw) == TRUE)
	    {
		x = gu_draw_string(term->gudraw, win, gc, x, y, (guchar*) s,
				   bw);
		g_free(s);
	    }

	    len -= i;
	    if (len <= 0)
		break;
	    buf += i;
	}

	switch (buf[0])
	{
	    case 1:	/* horizontal line */
		gdk_draw_line(win, gc, x, lineCenter, x + term->fontWidth,
			      lineCenter);
		break;

	    case 2:	/* upper right */
		gdk_draw_line(win, gc, x, lineCenter, x + term->fontWidth / 2,
			      lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight);
		break;

	    case 3:	/* upper left */
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight);
		break;

	    case 4:	/* vertical line */
		gdk_draw_line(win, gc, x + term->fontWidth / 2,
			      lineCenter - term->fontHeight / 2,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight / 2);
		break;

	    case 5:	/* lower right */
		gdk_draw_line(win, gc, x, lineCenter,
			      x + term->fontWidth / 2, lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter - term->fontHeight);
		break;

	    case 6:	/* lower left */
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth, lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter - term->fontHeight);
		break;

	    case 7:	/* vertical and c-r horizontal */
		gdk_draw_line(win, gc, x + term->fontWidth / 2,
			      lineCenter - term->fontHeight / 2,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight / 2);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth, lineCenter);
		break;

	    case 8:	/* vertical and l-c horizontal */
		gdk_draw_line(win, gc, x, lineCenter,
			      x + term->fontWidth / 2, lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2,
			      lineCenter - term->fontHeight / 2,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight / 2);
		break;

	    case 9:	/* horizontal and c-l vertical */
		gdk_draw_line(win, gc, x, lineCenter,
			      x + term->fontWidth, lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter + term->fontHeight / 2);
		break;

	    case 10:	/* horizontal and u-c vertical */
		gdk_draw_line(win, gc, x, lineCenter,
			      x + term->fontWidth, lineCenter);
		gdk_draw_line(win, gc, x + term->fontWidth / 2, lineCenter,
			      x + term->fontWidth / 2,
			      lineCenter - term->fontHeight / 2);
		break;

	    default:
		break;
	}
	buf++;
	len--;
	x += term->fontWidth;
    }
}

/* TermExposeLineNoPixmap() {{{1 */
static void
TermExposeLineNoPixmap(TermType *term, int y, int x1, int x2)
{
    int realLineOffset;
    guchar *buf, *color, *attr;
    int startX, x, bgColor, fgColor;
    char oldAttr, oldColor;
    GtkWidget *w;
    static int oldFgColor = -1;
    static int oldBgColor = -1;
    static GdkPixmap *pixmap = NULL;
    static int pixmapSize = 0;

    if (!term->drawFlag || term->timer
	|| !GTK_WIDGET_VISIBLE(term) || x2 < x1 || x1 < 0 || x2 >= term->col)
	return;

    w = GTK_WIDGET(term);

    realLineOffset = TERM_LINENUM(y) * term->col;
    buf = term->buf + realLineOffset;

    if (IsSecondMB((char*) buf, x1))
	x1--;
    if (IsFirstMB((char*) buf, x2))
	x2++;

    if (x1 < 0)
	x1 = 0;
    if (x2 >= term->col)
	x2 = term->col - 1;

    startX = x1;
    if (oldBgColor != TermNormBG)
    {
	gdk_gc_set_foreground(term->bgGC, &w->style->bg[GTK_STATE_NORMAL]);
	oldBgColor = TermNormBG;
    }
    if (x2 - x1 + 1 > pixmapSize)
    {
	if (pixmap)
	    g_object_unref(pixmap);
	pixmap = gdk_pixmap_new(w->window, term->fontWidth * term->col,
				term->fontHeight, -1);
	pixmapSize = term->col;
    }
    gdk_draw_rectangle(pixmap, term->bgGC, TRUE, 0, 0,
		       term->fontWidth * (x2 - x1 + 1), term->fontHeight);

    color = term->color + realLineOffset + x1;
    attr = term->attr + realLineOffset + x1;
    y *= term->fontHeight;
    x = x1;
    while (x1 <= x2)
    {
	oldAttr = *attr;
	oldColor = *color;
	do
	{
	    ++x;
	    ++attr;
	    ++color;
	} while (x <= x2 && *attr == oldAttr && *color == oldColor);

	if (oldAttr & (ATTR_SELECTION | ATTR_REVERSE))
	    bgColor = (oldColor >> 4) & 0x0f;
	else
	    bgColor = oldColor & 0x0f;

	if (bgColor != TermNormBG && oldBgColor != bgColor)
	{
	    gdk_gc_set_foreground(term->bgGC, term->colorTable[bgColor]);
	    oldBgColor = bgColor;
	}

	if (bgColor != TermNormBG)
	    gdk_draw_rectangle(pixmap, term->bgGC, TRUE,
			       (x1 - startX) * term->fontWidth, 0,
			       term->fontWidth * (x - x1), term->fontHeight);

	if (oldAttr & (ATTR_SELECTION | ATTR_REVERSE))
	    fgColor = oldColor & 0x0f;
	else
	    fgColor = (oldColor >> 4) & 0x0f;

	if (fgColor == TermNormBG && bgColor == TermNormBG)
	    fgColor = TermNormFG;
	else if (fgColor == bgColor)
	{
	    if (fgColor != TermNormFG)
		fgColor = TermNormFG;
	    else if (TermNormFG == TERM_COLOR7)
		fgColor = TERM_COLOR0;
	    else
		fgColor = TERM_COLOR7;
	}

	if (fgColor != oldFgColor)
	{
	    gdk_gc_set_foreground(term->gc, term->colorTable[fgColor]);
	    oldFgColor = fgColor;
	}

	TermGdkDrawText(pixmap, term->gc,
			(x1 - startX) * term->fontWidth, term->fontAscent,
			buf + x1, x - x1, term);

	if (TermUseBold && (oldAttr & ATTR_BOLD))
	    TermGdkDrawText(pixmap, term->gc,
			    (x1 - startX) * term->fontWidth + 1,
			    term->fontAscent, buf + x1, x - x1, term);

	if (TermUseUnder && (oldAttr & ATTR_UNDERLINE))
	    gdk_draw_line(pixmap, term->gc,
			  (x1 - startX) * term->fontWidth, term->fontHeight - 1,
			  (x - startX) * term->fontWidth, term->fontHeight - 1);

	x1 = x;
    }
    gdk_draw_drawable(w->window, term->gc, pixmap, 0, 0,
		      startX * term->fontWidth, y,
		      (x2 - startX + 1) * term->fontWidth, term->fontHeight);
}

/* TermExposeLine() {{{1 */
static void
TermExposeLine(TermType *term, int y, int x1, int x2)
{
    int realLineOffset;
    guchar *buf, *color, *attr;
    int x;
    int bgColor, fgColor;
    char oldAttr, oldColor;
    static int oldFgColor = -1;
    static int oldBgColor = -1;

    if (!term->haveBgPixmap)
    {
	TermExposeLineNoPixmap(term, y, x1, x2);
	return;
    }

    if (!term->drawFlag || term->timer
	|| !GTK_WIDGET_VISIBLE(term) || x2 < x1 || x1 < 0 || x2 >= term->col)
	return;

    realLineOffset = TERM_LINENUM(y) * term->col;
    buf = term->buf + realLineOffset;

    if (IsSecondMB((char*) buf, x1))
	x1--;
    if (IsFirstMB((char*) buf, x2))
	x2++;

    if (x1 < 0)
	x1 = 0;
    if (x2 >= term->col)
	x2 = term->col - 1;

    color = term->color + realLineOffset + x1;
    attr = term->attr + realLineOffset + x1;
    y *= term->fontHeight;
    x = x1;
    while (x1 <= x2)
    {
	oldAttr = *attr;
	oldColor = *color;
	do
	{
	    ++x;
	    ++attr;
	    ++color;
	}
	while (x <= x2 && *attr == oldAttr && *color == oldColor);

	if ((oldAttr & ATTR_SELECTION) || (oldAttr & ATTR_REVERSE))
	    bgColor = (oldColor >> 4) & 0x0f;
	else
	    bgColor = oldColor & 0x0f;

	if (bgColor != TermNormBG && oldBgColor != bgColor)
	{
	    gdk_gc_set_foreground(term->bgGC, term->colorTable[bgColor]);
	    oldBgColor = bgColor;
	}

	if (bgColor == TermNormBG)
	    gdk_window_clear_area(GTK_WIDGET(term)->window,
				  x1 * term->fontWidth, y,
				  term->fontWidth * (x - x1), term->fontHeight);
	else
	    gdk_draw_rectangle(GTK_WIDGET(term)->window, term->bgGC, TRUE,
			       x1 * term->fontWidth, y,
			       term->fontWidth * (x - x1), term->fontHeight);

	if ((oldAttr & ATTR_SELECTION) || (oldAttr & ATTR_REVERSE))
	    fgColor = oldColor & 0x0f;
	else
	    fgColor = (oldColor >> 4) & 0x0f;

	if (fgColor == TermNormBG && bgColor == TermNormBG)
	    fgColor = TermNormFG;
	else if (fgColor == bgColor)
	{
	    if (fgColor != TermNormFG)
		fgColor = TermNormFG;
	    else if (TermNormFG == TERM_COLOR7)
		fgColor = TERM_COLOR0;
	    else
		fgColor = TERM_COLOR7;
	}

	if (fgColor != oldFgColor)
	{
	    gdk_gc_set_foreground(term->gc, term->colorTable[fgColor]);
	    oldFgColor = fgColor;
	}

	TermGdkDrawText(GTK_WIDGET(term)->window, term->gc,
			x1 * term->fontWidth, y + term->fontAscent,
			buf + x1, x - x1, term);

	if (TermUseBold && (oldAttr & ATTR_BOLD))
	    TermGdkDrawText(GTK_WIDGET(term)->window, term->gc,
			    x1 * term->fontWidth + 1, y + term->fontAscent,
			    buf + x1, x - x1, term);

	if (TermUseUnder && (oldAttr & ATTR_UNDERLINE))
	    gdk_draw_line(GTK_WIDGET(term)->window, term->gc,
			  x1 * term->fontWidth, y + term->fontHeight - 1,
			  x * term->fontWidth, y + term->fontHeight - 1);

	x1 = x;
    }
}

/* TermShowCursor() {{{1 */
static void
TermShowCursor(TermType *term, gboolean fill)
{
    int n;
    GtkWidget *w;

    UNUSED(fill);

    if (term->timer
	|| !GTK_WIDGET_VISIBLE(term)
	|| term->visibleLineBegin != term->realVisibleLineBegin
	|| term->currX >= term->col)
	return;

    w = GTK_WIDGET(term);

    if (CURR_BYTE(term) & 0x80)
	n = 2;
    else
	n = 1;

    gdk_gc_set_foreground(term->cursorGC, term->colorTable[TERM_CURSOR_BG]);
    if (GTK_WIDGET_FLAGS(term) & GTK_HAS_FOCUS)
	gdk_draw_rectangle(w->window, term->cursorGC, TRUE,
			   term->currX * term->fontWidth,
			   term->currY * term->fontHeight,
			   term->fontWidth * n, term->fontHeight);
    else
	gdk_draw_rectangle(w->window, term->cursorGC, FALSE,
			   term->currX * term->fontWidth,
			   term->currY * term->fontHeight,
			   term->fontWidth * n - 1, term->fontHeight - 1);

    gdk_gc_set_foreground(term->cursorGC, term->colorTable[TERM_CURSOR_FG]);
    TermGdkDrawText(w->window, term->cursorGC,
		    term->currX * term->fontWidth,
		    term->currY * term->fontHeight + term->fontAscent,
		    CURR_BYTE_POS(term), n, term);

    termICSpotX = term->currX;
    termICSpotY = term->currY;
}

/* TermHideCursor() {{{1 */
static void
TermHideCursor(TermType *term)
{
    if (!term->timer
	&& term->currX < term->col
	&& term->visibleLineBegin == term->realVisibleLineBegin)
    {
	TermExposeLine(term, term->currY, term->currX, term->currX);
    }
}

/* termCursBlinkCB() {{{1 */
static gint
termCursBlinkCB(gpointer data)
{
    static gboolean on = TRUE;
    TermType *term = TERM(data);
    gint to;

    if (TermCursorBlink)
    {
	if (on)
	{
	    TermShowCursor(term, TRUE);
	    to = TermCursorOnTime;
	}
	else
	{
	    TermHideCursor(term);
	    to = TermCursorOffTime;
	}

	on = !on;
	termCursorTimer = gtk_timeout_add(to, termCursBlinkCB, term);
    }
    else
	TermShowCursor(term, TRUE);

    return FALSE;
}

static void
termCursorBlinkStart(TermType *term)
{
    if (TermCursorBlink && termCursorTimer < 0)
    {
	termCursorTimer = gtk_timeout_add(TermCursorOnTime, termCursBlinkCB,
					  term);
    }
    TermShowCursor(term, TRUE);
}

static void
termCursorBlinkStop(TermType *term)
{
    if (termCursorTimer >= 0)
    {
	gtk_timeout_remove(termCursorTimer);
	termCursorTimer = -1;
    }
    TermHideCursor(term);
    TermShowCursor(term, FALSE);
}

/* TermExpose() {{{1 */
static int
TermExpose(GtkWidget *w, GdkEventExpose *ev)
{
    int y, y1, y2, x1, x2;
    TermType *term;
    GdkRectangle *area;

    term = TERM(w);
    area = &ev->area;

    /* translate the position */
    y1 = area->y / term->fontHeight;
    y2 = (area->y + area->height - 1) / term->fontHeight;
    if (y2 >= term->row)
	y2 = term->row - 1;
    x1 = area->x / term->fontWidth;
    x2 = (area->x + area->width - 1) / term->fontWidth;
    if (x1 >= term->col)
	x1 = term->col - 1;

    for (y = y1; y <= y2; y++)
	TermExposeLine(term, y, x1, x2);

    x1--;
    x2++;
    if (y1 <= term->currY && y2 >= term->currY
	&& x1 <= term->currX && x2 >= term->currX)
	TermShowCursor(term, TRUE);

    return TRUE;
}

static const struct {
    int code;
    char *linuxCon;
    char *vt100App;
    char *ansi;
} vtKeys[] = {
    /* *INDENT-OFF* */
    { GDK_F1, "[[A", "[11~", "OP" },
    { GDK_F2, "[[B", "[12~", "OQ" },
    { GDK_F3, "[[C", "[13~", "OR" },
    { GDK_F4, "[[D", "[14~", "OS" },
    { GDK_F5, "[[E", "[15~", "OT" },
    { GDK_F6, "[17~", "[17~", "OU" },
    { GDK_F7, "[18~", "[18~", "OV" },
    { GDK_F8, "[19~", "[19~", "OW" },
    { GDK_F9, "[20~", "[20~", "OX" },
    { GDK_F10, "[21~", "[21~", "OY" },
    { GDK_F11, "[23~", "[23~", "OZ" },
    { GDK_F12, "[24~", "[24~", "OA" },
    { GDK_Up, "[A", "OA", "[A" },
    { GDK_Left, "[D", "OD", "[D" },
    { GDK_Right, "[C", "OC", "[C" },
    { GDK_Down, "[B", "OB", "[B" },
    { GDK_Home, "[1~", "[1~", "[H" },
    { GDK_End, "[7~", "[7~", "[Y" },
    { GDK_Prior, "[5~", "[5~", "[V" },
    { GDK_Next, "[6~", "[6~", "[U" },
    { GDK_Insert, "[2~", "[2~", "[@" },
    { GDK_Delete, "[3~", "[3~", "\177" },
    { 0, NULL, NULL, NULL }
    /* *INDENT-ON* */
};

/* scrollModeLineUp() {{{1 */
static void
scrollModeLineUp(TermType *term)
{
    if (term->adjustment->value)
    {
	--term->adjustment->value;
	if (term->adjustment->value < 0)
	    term->adjustment->value = 0;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
}

/* scrollModeLineDown() {{{1 */
static void
scrollModeLineDown(TermType *term)
{
    if (term->adjustment->value != term->adjustment->upper - term->row)
    {
	++term->adjustment->value;
	if (term->adjustment->value > term->adjustment->upper - term->row)
	    term->adjustment->value = term->adjustment->upper - term->row;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
}

/* scrollModePageUp() {{{1 */
static void
scrollModePageUp(TermType *term)
{
    if (term->adjustment->value)
    {
	term->adjustment->value -= term->row;
	if (term->adjustment->value < 0)
	    term->adjustment->value = 0;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
}

/* scrollModePageDown() {{{1 */
static void
scrollModePageDown(TermType *term)
{
    if (term->adjustment->value != term->adjustment->upper - term->row)
    {
	term->adjustment->value += term->row;
	if (term->adjustment->value > term->adjustment->upper - term->row)
	    term->adjustment->value = term->adjustment->upper - term->row;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
}

/* scrollModeEnd() {{{1 */
static void
scrollModeEnd(TermType *term)
{
    term->adjustment->value = term->adjustment->upper - term->row;
    g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
}

/* scrollModeKeyPress() {{{1 */
static void
scrollModeKeyPress(TermType *term, GdkEventKey *ev)
{
    switch (ev->keyval)
    {
	case GDK_Up:
	case GDK_KP_Up:
	    scrollModeLineUp(term);
	    break;

	case GDK_Down:
	case GDK_KP_Down:
	    scrollModeLineDown(term);
	    break;

	case GDK_Page_Up:
	case GDK_KP_Page_Up:
	    scrollModePageUp(term);
	    break;

	case GDK_Page_Down:
	case GDK_KP_Page_Down:
	    scrollModePageDown(term);
	    break;

	case GDK_space:
	case GDK_Return:
	case GDK_Escape:
	    scrollModeEnd(term);
	    break;
    }
}

/* TermScrollModePageUp() {{{1 */
static void
TermScrollModePageUp(TermType *term)
{
    if (term->adjustment->value)
    {
	term->adjustment->value -= term->row;
	if (term->adjustment->value < 0)
	    term->adjustment->value = 0;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
}

/* TermKeyPress() {{{1 */
static int
TermKeyPress(GtkWidget *w, GdkEventKey *ev)
{
    int i;
    guchar buf[512];
    GSList *slist;
    KeyMapType *keymap;
    int key;
    TermType *term = TERM(w);

    if (gtk_im_context_filter_keypress(term->ic, ev) != 0)
    {
	term->icReset = TRUE;
	return TRUE;
    }

    /* check if it is menu hotkey */
    if (gtk_accel_groups_activate(G_OBJECT(w), ev->keyval, ev->state))
	return TRUE;

    if (SChat)
    {
	/* chatting창이 열려있는 상태에서 사용자가 terminal 창을 마우스로
	 * 클릭한다든지의 이유로 terminal 창이 focus를 가지고 있게 되는 수가
	 * 있다. 그런 경우 event를 다시 chatting창으로 자동으로 돌린다.
	 */
	gtk_widget_event(GTK_WIDGET(SChat->chat->w), (GdkEvent *) ev);
	gtk_widget_grab_focus(GTK_WIDGET(SChat->chat->w));
	return TRUE;
    }

    if (TRxRunning)
	return TRUE;

    if (term->realVisibleLineBegin != term->visibleLineBegin)
    {
	/* I'm in scroll mode */
	scrollModeKeyPress(term, ev);
	return TRUE;
    }

    key = ev->keyval;

    if (key >= GDK_Shift_L && key <= GDK_Alt_R)
	return FALSE;

    for (slist = TermKeyMapList; slist;)
    {
	keymap = (KeyMapType *) slist->data;
	slist = slist->next;
	if (key == keymap->keyVal)
	{
	    if (ev->state & keymap->state)
	    {
		keymap->func(w, keymap->data);
		return TRUE;
	    }
	}
    }

    if ((ev->state & (GDK_MOD1_MASK | GDK_MOD4_MASK)) && ev->length == 1)
    {
	buf[0] = ESC;
	buf[1] = ev->string[0];
	term->remoteWrite(buf, 2);
	return TRUE;
    }
    else if (key == GDK_Return)
    {
	if (EnterConvert == ENTER_CR)
	{
	    buf[0] = '\n';
	    term->remoteWrite(buf, 1);
	}
	else if (EnterConvert == ENTER_CRLF)
	{
	    buf[0] = '\r';
	    buf[1] = '\n';
	    term->remoteWrite(buf, 2);
	}
	else	/* ENTER_LF or ENTER_NONE */
	{
	    buf[0] = '\r';
	    term->remoteWrite(buf, 1);
	}
    }
    else if (key == GDK_BackSpace)
    {
	buf[0] = '\b';
	term->remoteWrite(buf, 1);
    }
    else if (key == GDK_Tab)
    {
	buf[0] = '\t';
	term->remoteWrite(buf, 1);
    }
    else if (key == GDK_Escape)
    {
	buf[0] = ESC;
	term->remoteWrite(buf, 1);
    }
    else if (ev->length > 0)
    {
	term->remoteWrite((guchar*) ev->string, ev->length);
    }
    else
    {
	for (i = 0;; i++)
	{
	    if (!vtKeys[i].code)
		return FALSE;

	    if (vtKeys[i].code == key)
	    {
		if (term->mode & MODE_CUR_APL)
		    g_snprintf((char*) buf, sizeof(buf), "%c%s", ESC,
			       vtKeys[i].vt100App);
		else
		    g_snprintf((char*) buf, sizeof(buf), "%c%s", ESC,
			       vtKeys[i].linuxCon);
		term->remoteWrite((guchar*) buf, -1);
		break;
	    }
	}
    }
    return TRUE; /* eat event */
}

/* ScrollModePageUp() {{{1 */
static void
ScrollModePageUp(TermType *term, gpointer dummy)
{
    TermScrollModePageUp(term);
    dummy = NULL;
}

/* TermAddKeyMap() {{{1 */
static void
TermAddKeyMap(int keyVal, int state, KeyMapFuncType func, gpointer data)
{
    KeyMapType *keymap;

    keymap = g_new(KeyMapType, 1);
    keymap->keyVal = keyVal;
    keymap->state = state;
    keymap->func = (KeyMapFuncType) func;
    keymap->data = data;
    TermKeyMapList = g_slist_append(TermKeyMapList, keymap);
}

/* TermInitKeyMap() {{{1 */
static void
TermInitKeyMap(void)
{
    TermAddKeyMap(GDK_Page_Up, GDK_SHIFT_MASK,
		  (KeyMapFuncType) ScrollModePageUp, NULL);
}

/* TermLineClear() {{{1 */
static void
TermLineClear(TermType *term, int line)
{
    int offset = TERM_LINENUM(line) * term->col;
    guchar defaultColor = COLOR(TermNormFG, TermNormBG);

    memset(term->buf + offset, 0, term->col);
    memset(term->color + offset, defaultColor, term->col);
    memset(term->attr + offset, 0x00, term->col);
}

/* _termSetSize() {{{1 */
static void
_termSetSize(TermType *term, int newCol, int newRow)
{
    guchar *buf, *color, *attr;
    GtkAdjustment *adj;
    guchar *src, *dest;
    int i, savedLines;
    int minCol;
    guchar vColor = COLOR(TermNormFG, TermNormBG);

    OptPrintf(OPRT_TERM, "%s: col=%d, row=%d\n", __FUNCTION__, newCol, newRow);

    adj = term->adjustment;
    savedLines = TermSavedLines;
    minCol = MIN(newCol, term->col);

    buf = g_new(guchar, newCol * savedLines);
    color = g_new(guchar, newCol * savedLines);
    attr = g_new(guchar, newCol * savedLines);

    for (i = 0; i < savedLines; i++)
    {
	src = term->buf + term->col * i;
	dest = buf + newCol * i;
	memcpy(dest, src, minCol);
	if (minCol < newCol)
	    memset(dest + minCol, 0, newCol - minCol);

	src = term->color + term->col * i;
	dest = color + newCol * i;
	memcpy(dest, src, minCol);
	if (minCol < newCol)
	    memset(dest + minCol, vColor, newCol - minCol);

	src = term->attr + term->col * i;
	dest = attr + newCol * i;
	memcpy(dest, src, minCol);
	if (minCol < newCol)
	    memset(dest + minCol, 0x00, newCol - minCol);
    }

    g_free(term->buf);
    g_free(term->color);
    g_free(term->attr);

    term->buf = buf;
    term->color = color;
    term->attr = attr;

    term->col = newCol;

    if (term->row != newRow)
    {
	if (term->row < newRow)
	{
	    for (i = term->row; i < newRow; i++)
		TermLineClear(term, i);
	}
	term->row = newRow;
	if (term->currY >= newRow)
	{
	    term->realVisibleLineBegin += term->currY - newRow + 1;
	    if (term->realVisibleLineBegin >= savedLines)
		term->realVisibleLineBegin -= savedLines;
	    term->currY = newRow - 1;
	}
	adj->page_size = newRow;
	if (adj->upper < newRow)
	    adj->upper = newRow;
	adj->value = adj->upper - newRow;
    }

    term->visibleLineBegin = term->realVisibleLineBegin;
    if (term->currX >= newCol)
	term->currX = newCol - 1;
    term->bot = term->row;

    TermQueueDraw(term);

    g_signal_emit(G_OBJECT(term), termSignals[TERM_SIZE_CHANGED], 0);
}

/* TermSetSize() {{{1 */
void
TermSetSize(TermType *term, int newCol, int newRow)
{
    gint width = 0, height = 0;

    TermCol = newCol;
    TermRow = newRow;

    gtk_window_get_size(GTK_WINDOW(MainWin), &width, &height);
    width += (newCol - term->col) * term->fontWidth;
    height += (newRow - term->row) * term->fontHeight;
    gtk_window_resize(GTK_WINDOW(MainWin), width, height);

    OptPrintf(OPRT_TERM, "%s: width=%d, height=%d\n", __FUNCTION__,
	      width, height);
}

/* _termSizeFix() {{{1 */
static int
_termSizeFix(void)
{
    TermSetSize(Term, TermCol, TermRow);
    return FALSE;
}

/* TermSizeFix() {{{1 */
void
TermSizeFix(void)
{
    /* gtk+ 2.0에서는 MainWin의 크기로 terminal size를 바꾸는 데, control
     * bar나 toolbar 같은 걸 보이거나 숨기는 경우 mainwin 크기를 다시 조절해
     * 주어야 terminal size가 일정하게 유지된다.
     */
    gtk_timeout_add(300, (GtkFunction) _termSizeFix, NULL);
}

/* TermSizeAllocate() {{{1 */
static void
TermSizeAllocate(GtkWidget *w, GtkAllocation *allocation)
{
    int newCol, newRow;
    TermType *term;

    term = TERM(w);

    OptPrintf(OPRT_TERM, "%s: width=%d, height=%d\n", __FUNCTION__,
	      allocation->width, allocation->height);

    newCol = allocation->width / term->fontWidth;
    newRow = allocation->height / term->fontHeight;
    _termSetSize(term, newCol, newRow);

    w->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(w))
	gdk_window_move_resize(w->window, allocation->x, allocation->y,
			       allocation->width, allocation->height);
}

/* TermSizeRequest() {{{1 */
static void
TermSizeRequest(GtkWidget *w, GtkRequisition *requisition)
{
    TermType *term;

    term = TERM(w);
    requisition->width = term->col * term->fontWidth;
    requisition->height = term->row * term->fontHeight;
}

/* TermFocusIn() {{{1 */
static int
TermFocusIn(GtkWidget *w, GdkEventFocus *ev)
{
    UNUSED(ev);

    GTK_WIDGET_SET_FLAGS(w, GTK_HAS_FOCUS);

    TERM(w)->icReset = TRUE;
    gtk_im_context_focus_in(TERM(w)->ic);

    termCursorBlinkStart(TERM(w));

    return FALSE;
}

/* TermFocusOut() {{{1 */
static int
TermFocusOut(GtkWidget *w, GdkEventFocus *ev)
{
    UNUSED(ev);

    GTK_WIDGET_UNSET_FLAGS(w, GTK_HAS_FOCUS);

    TERM(w)->icReset = TRUE;
    gtk_im_context_focus_out(TERM(w)->ic);

    termCursorBlinkStop(TERM(w));

    return FALSE;
}

/* TermSelectionReceived() {{{1 */
static void
TermSelectionReceived(GtkWidget *w, GtkSelectionData * selData, guint time)
{
    UNUSED(time);
    TERM(w)->remoteWrite(selData->data, selData->length);
}

/* TermColorAlloc() {{{1 */
static GdkColor **
TermColorAlloc(GdkColormap *cmap)
{
    GdkColor **colorTable;
    int i;

    colorTable = g_new(GdkColor *, TERM_COLOR_END);
    colorTable[TERM_COLOR0] = TermColor0;
    colorTable[TERM_COLOR1] = TermColor1;
    colorTable[TERM_COLOR2] = TermColor2;
    colorTable[TERM_COLOR3] = TermColor3;
    colorTable[TERM_COLOR4] = TermColor4;
    colorTable[TERM_COLOR5] = TermColor5;
    colorTable[TERM_COLOR6] = TermColor6;
    colorTable[TERM_COLOR7] = TermColor7;
    colorTable[TERM_COLOR8] = TermColor8;
    colorTable[TERM_COLOR9] = TermColor9;
    colorTable[TERM_COLOR10] = TermColor10;
    colorTable[TERM_COLOR11] = TermColor11;
    colorTable[TERM_COLOR12] = TermColor12;
    colorTable[TERM_COLOR13] = TermColor13;
    colorTable[TERM_COLOR14] = TermColor14;
    colorTable[TERM_COLOR15] = TermColor15;
    colorTable[TERM_CURSOR_FG] = TermCursorFG;
    colorTable[TERM_CURSOR_BG] = TermCursorBG;
    colorTable[TERM_ISEL_FG] = TermISelFG;

    for (i = 0; i < TERM_COLOR_END; i++)
	gdk_colormap_alloc_color(cmap, colorTable[i], FALSE, TRUE);

    return colorTable;
}

/* TermRealize() {{{1 */
static void
TermRealize(GtkWidget *w)
{
    TermType *term;
    GdkWindowAttr attr;
    gint attrMask;
    int i;

    term = TERM(w);
    GTK_WIDGET_SET_FLAGS(w, GTK_REALIZED);

    attr.window_type = GDK_WINDOW_CHILD;
    attr.x = w->allocation.x;
    attr.y = w->allocation.y;
    attr.width = w->allocation.width;
    attr.height = w->allocation.height;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.visual = gtk_widget_get_visual(w);
    attr.colormap = gtk_widget_get_colormap(w);
    attr.event_mask = gtk_widget_get_events(w);
    attr.event_mask |= (GDK_EXPOSURE_MASK
			| GDK_KEY_PRESS_MASK
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_BUTTON1_MOTION_MASK
			| GDK_BUTTON3_MOTION_MASK);

    attrMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    w->window = gdk_window_new(gtk_widget_get_parent_window(w), &attr,
			       attrMask);
    gdk_window_set_user_data(w->window, term);
    w->style = gtk_style_attach(w->style, w->window);
    gtk_style_set_background(w->style, w->window, GTK_STATE_NORMAL);

    term->gc = gdk_gc_new(w->window);
    term->bgGC = gdk_gc_new(w->window);
    term->whiteGC = gdk_gc_new(w->window);
    term->blackGC = GTK_WIDGET(term)->style->black_gc;
    term->cursorGC = gdk_gc_new(w->window);
    term->iSelGC = gdk_gc_new(w->window);
    ColorMap = attr.colormap;
    term->colorTable = TermColorAlloc(attr.colormap);

    gdk_gc_set_foreground(term->whiteGC, term->colorTable[TERM_COLOR7]);
    gdk_gc_set_foreground(term->iSelGC, term->colorTable[TERM_ISEL_FG]);

    term->currFG = TermNormFG;
    term->currBG = TermNormBG;

    for (i = 0; i < term->row; i++)
	TermLineClear(term, i);

    if (w->style->bg_pixmap[GTK_STATE_NORMAL])
	term->haveBgPixmap = TRUE;
    else
	term->haveBgPixmap = FALSE;

    g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");

    gtk_im_context_set_client_window(term->ic, w->window);
}

/* TermUnrealize() {{{1 */
static void
TermUnrealize(GtkWidget *w)
{
    TermType *term;

    term = TERM(w);
    g_object_unref(term->gc);
    g_object_unref(term->bgGC);
    g_object_unref(term->whiteGC);
    g_object_unref(term->cursorGC);

    gtk_im_context_set_client_window(term->ic, w->window);
}

/* TermFinalize() {{{1 */
static void
TermFinalize(GtkObject *obj)
{
    TermExit();
    G_OBJECT_CLASS(ParentClass)->finalize(G_OBJECT(obj));
}

/* TermDestroy() {{{1 */
static void
TermDestroy(GtkObject *obj)
{
    if (GTK_OBJECT_CLASS(ParentClass)->destroy)
	(*GTK_OBJECT_CLASS(ParentClass)->destroy)(obj);
}

/* TermTmpBuffer() {{{1 */
static void
TermTmpBuffer(TermType *term, const guchar *buf, int len)
{
    if ((guint) term->tmpAlloc < (guint) (len + term->tmpLen))
    {
	guint n = 1;

	while (n < (guint) (len + term->tmpLen))
	    n <<= 1;

	term->tmpAlloc = n;
	term->tmpStr = g_realloc(term->tmpStr, term->tmpAlloc);
    }
    memcpy(term->tmpStr + term->tmpLen, buf, len);
    term->tmpLen += len;
}

/* TermLineCopy() {{{1 */
static void
TermLineCopy(TermType *term, int destLine, int srcLine)
{
    int realDestLine = TERM_LINENUM(destLine) * term->col;
    int realSrcLine = TERM_LINENUM(srcLine) * term->col;

    memcpy(term->buf + realDestLine, term->buf + realSrcLine, term->col);
    memcpy(term->color + realDestLine, term->color + realSrcLine, term->col);
    memcpy(term->attr + realDestLine, term->attr + realSrcLine, term->col);
}

/* TermScrollUp() {{{1 */
static void
TermScrollUp(TermType *term)
{
    register int y;
    int realBot;

    realBot = MIN(term->bot, term->maxReached + 1);

    if (term->top == 1)
    {
	/* This is global scroll. shift scrolled lines */
	++term->realVisibleLineBegin;
	term->realVisibleLineBegin %= TermSavedLines;
	if (term->realVisibleLineBegin > term->adjustment->value
	    && term->realVisibleLineBegin <= (TermSavedLines - term->row))
	{
	    term->adjustment->value = term->realVisibleLineBegin;
	    term->adjustment->upper = term->realVisibleLineBegin + term->row;
	    if (term->timer)
		term->needSlideUpdate = TRUE;
	    else
		g_signal_emit_by_name(G_OBJECT(term->adjustment), "changed");
	}
	term->visibleLineBegin = term->realVisibleLineBegin;
	if (term->bot != term->row)
	    term->maxReached = term->row;
	if (term->bot <= term->maxReached + 1)
	    for (y = term->maxReached; y >= term->bot; y--)
		TermLineCopy(term, y, y - 1);
    }
    else
    {
	/* local scroll. do not save */
	for (y = term->top; y < realBot; y++)
	    TermLineCopy(term, y - 1, y);
    }

    TermLineClear(term, realBot - 1);
    if (term->drawFlag)
	for (y = term->top - 1; y < realBot; y++)
	    TermExposeLine(term, y, 0, term->col - 1);
}

/* TermScrollDown() {{{1 */
static void
TermScrollDown(TermType *term)
{
    register int y;

    for (y = term->bot - 1; y >= term->top; y--)
	TermLineCopy(term, y, y - 1);
    TermLineClear(term, term->top - 1);
    if (term->drawFlag)
	for (y = term->top - 1; y < term->bot; y++)
	    TermExposeLine(term, y, 0, term->col - 1);
}

/* TermNewLine() {{{1 */
static void
TermNewLine(TermType *term)
{
    if (TermAutoLineFeed)
	term->currX = 0;
    if (++term->currY >= term->bot)
    {
	term->drawFlag = FALSE;
	TermScrollUp(term);
	term->currY = term->bot - 1;
    }
    TERM_UPDATE_MAX_REACHED(term);
}

/* TermCursorDown() {{{1 */
static void
TermCursorDown(TermType *term)
{
    if (term->argv[0] < 1)
	term->argv[0] = 1;
    term->currY = MIN(term->row - 1, term->currY + term->argv[0]);
    TERM_UPDATE_MAX_REACHED(term);
}

/* TermCursorUp() {{{1 */
static void
TermCursorUp(TermType *term)
{
    if (term->argv[0] < 1)
	term->argv[0] = 1;
    term->currY = MIN(term->row - 1, MAX(term->currY - term->argv[0], 0));
    TERM_UPDATE_MAX_REACHED(term);
}

/* TermTransGraphicMode() {{{1 */
static guchar
TermTransGraphicMode(const char c)
{
    if (c == 'q')
	return 1;
    if (c == 'k')
	return 2;
    if (c == 'l')
	return 3;
    if (c == 'x')
	return 4;
    if (c == 'j')
	return 5;
    if (c == 'm')
	return 6;
    if (c == 't')
	return 7;
    if (c == 'u')
	return 8;
    if (c == 'w')
	return 9;
    if (c == 'v')
	return 10;
    return c;
}

/* TermOverwriteSpaces() {{{1 */
static void
TermOverwriteSpaces(TermType *term, int n)
{
    register int i;
    guchar *buf, *color, *attr;

    if (n)
    {
	buf = term->buf + TERM_LINENUM(term->currY) * term->col;
	color = term->color + TERM_LINENUM(term->currY) * term->col;
	attr = term->attr + TERM_LINENUM(term->currY) * term->col;
	for (i = 0; i < n; i++)
	{
	    buf[term->currX + i] = 0;
	    color[term->currX + i] = COLOR(term->currFG, term->currBG);
	    attr[term->currX + i] = term->currAttr;
	}
	TermExposeLine(term, term->currY, term->currX, term->currX + n);
	term->currX += n;
    }
}

/* TermStateGround() {{{1 */
static void
TermStateGround(TermType *term, guchar c)
{
    switch (c)
    {
	case 128 + ESC:	/* ESC [ */
	    term->state = TERM_CSI;
	    break;
	case ESC:
	    term->state = TERM_ESC;
	    break;
	case '\b':
	    if (term->currX)
	    {
		--term->currX;
		if (BackspaceDeleteChar)
		{
		    TermOverwriteSpaces(term, 1);
		    --term->currX;
		}
	    }
	    break;
	case '\n':
	    if (term->currX)
		TERM_LINE_CHANGING(term);
	    TermNewLine(term);
	    if (TermStripControlChar)
		CaptureInputFilter((char*) &c, 1);
	    break;
	case '\r':
	    TERM_LINE_CHANGING(term);
	    term->currX = 0;
	    if (TermStripControlChar)
		CaptureInputFilter((char*) &c, 1);
	    break;
	case 7:	/* bell */
	    if (!TermSilent)
		if (SoundPlay(SoundBeepFile) < 0)
		    gdk_beep();
	    break;
	case 14:
	    /* ^N: NOTE: from gau: enter alternate charset ==> g1 */
	    term->charset = '0';
	    break;
	case 15:
	    /* ^O: NOTE: from gau: enter default charset ==> g0 */
	    term->charset = 'B';
	    break;
	case 12:
	    term->argv[0] = 0;
	    TermCursorDown(term);
	    break;
	case 9:	/* horizontal tab */
	    term->currX = ((term->currX + 8) / 8) * 8;
	    if (term->currX >= term->col)
	    {
		term->currX = 0;
		++term->currY;
		if (term->currY >= term->row)
		{
		    term->drawFlag = FALSE;
		    TermScrollUp(term);
		    term->currY = term->row - 1;
		}
		TERM_UPDATE_MAX_REACHED(term);
	    }
	    if (TermStripControlChar)
		CaptureInputFilter((char*) &c, 1);
	    break;
	case 0:
	    /* NOTE: from gau: fixme */
	    break;
	default:
	    if (term->currX >= term->col)
	    {
		TERM_LINE_CHANGING(term);
		term->currX = 0;
		++term->currY;
		if (term->currY >= term->row)
		{
		    term->drawFlag = FALSE;
		    TermScrollUp(term);
		    term->currY = term->row - 1;
		}
		TERM_UPDATE_MAX_REACHED(term);
	    }
	    if (term->charset == '0')
		CURR_BYTE(term) = TermTransGraphicMode(c);
	    else
	    {
		if (term->charset == 'C')
		    c |= 0x80;
		CURR_BYTE(term) = c;
	    }

	    if (TermStripControlChar)
		CaptureInputFilter((char*) CURR_BYTE_POS(term), 1);

	    CURR_COLOR(term) = COLOR(term->currFG, term->currBG);
	    TermExposeLine(term, term->currY, term->currX, term->currX);
	    ++term->currX;
	    break;
    }
}

/* TermSaveCursorPosition() {{{1 */
static void
TermSaveCursorPosition(TermType *term)
{
    term->oldX = term->currX;
    term->oldY = term->currY;
    term->oldFG = term->currFG;
    term->oldBG = term->currBG;
    term->oldAttr = term->currAttr;
    OptPrintf(OPRT_TERM, "SAVEPOS: x=%d, y=%d, fg=%d, bg=%d, attr=%x\n",
	      term->oldX, term->oldY, term->oldFG, term->oldBG, term->oldAttr);
}

/* TermRestoreCursorPosition() {{{1 */
static void
TermRestoreCursorPosition(TermType *term)
{
    term->currX = term->oldX;
    term->currY = term->oldY;
    term->currFG = term->oldFG;
    term->currBG = term->oldBG;
    term->currAttr = term->oldAttr;
    OptPrintf(OPRT_TERM, "RESTPOS: x=%d, y=%d, fg=%d, bg=%d, attr=%x\n",
	      term->oldX, term->oldY, term->oldFG, term->oldBG, term->oldAttr);
}

/* TermClearScreen() {{{1 */
void
TermClearScreen(TermType *term)
{
    register int i;

    if (term->maxReached == 0)
	return;

    gdk_window_clear_area(GTK_WIDGET(term)->window, 0, 0,
			  term->col * term->fontWidth,
			  (term->maxReached + 1) * term->fontHeight);

    term->realVisibleLineBegin += term->maxReached + 1;
    term->realVisibleLineBegin %= TermSavedLines;
    if (term->realVisibleLineBegin > term->adjustment->value
	&& term->realVisibleLineBegin <= (TermSavedLines - term->row))
    {
	term->adjustment->value = term->realVisibleLineBegin;
	term->adjustment->upper = term->realVisibleLineBegin + term->row;
	if (term->timer)
	    term->needSlideUpdate = TRUE;
	else
	    g_signal_emit_by_name(G_OBJECT(term->adjustment), "changed");
    }
    term->visibleLineBegin = term->realVisibleLineBegin;

    for (i = term->row - term->maxReached - 1; i < term->row; i++)
	TermLineClear(term, i);

    term->currFG = TermNormFG;
    term->currBG = TermNormBG;
    term->currAttr = 0;
    term->currX = 0;
    term->currY = 0;
    term->maxReached = 0;
}

/* TermSane() {{{1 */
static void
TermSane(TermType *term)
{
    term->state = TERM_GROUND;
    term->mode = 0;
    term->charset = 'B';
    term->top = 1;
    term->bot = term->row;
    term->currAttr = 0;
    term->currFG = TermNormFG;
    term->currBG = TermNormBG;
}

/* TermMakeSane() {{{1 */
static void
TermMakeSane(void)
{
    TermSane(Term);
}

/* TermReset() {{{1 */
void
TermReset(TermType *term)
{
    TermClearScreen(term);
    TermSane(term);
}

/* TermStateEsc() {{{1 */
static void
TermStateEsc(TermType *term, guchar c)
{
    char buf[20];

    switch (c)
    {
	case 7:
	    if (!TermSilent)
		if (SoundPlay(SoundBeepFile) < 0)
		    gdk_beep();
	    break;
	case 'Z':	/* report terminal type */
	    g_snprintf(buf, sizeof(buf), "\033[?c");
	    term->remoteWrite((guchar*) buf, -1);
	    break;
	case '[':	/* ESC [ */
	    term->argc = 0;
	    term->argv[0] = 0;
	    term->state = TERM_CSI;
	    return;
	case '(':	/* choose charset */
	    term->state = TERM_SCS0;
	    return;
	case 'M':
	    TERM_LINE_CHANGING(term);
	    TermScrollDown(term);
	    break;
	case '7':	/* save cursor position */
	    TermSaveCursorPosition(term);
	    break;
	case '8':	/* restore cursor position */
	    TermRestoreCursorPosition(term);
	    break;
	case '=':	/* enter keypad application mode */
	case '>':	/* leave keypad application mode */
	    /* maybe-NEVER-fix */
	    break;
	case ')':
	    term->state = TERM_IGNORE;
	    return;
	case ']':
	    term->argv[0] = 0;
	    term->state = TERM_OSC;
	    return;
	case 'c':	/* reset */
	    TermReset(term);
	    break;
	case 14:
	    term->charset = '0';
	    break;
	case 15:	/* reset */
	    term->charset = 'B';
	    break;
	default:
	    OptPrintf(OPRT_TERM, "%s: char = %d,%c\n", __FUNCTION__, c, c);
	    break;
    }
    term->state = TERM_GROUND;
}

/* TermSetAttr() {{{1 */
static void
TermSetAttr(TermType *term)
{
    register int i, arg;
    gint useFG16Color, useBG16Color;
    gboolean fgSet, bgSet;

    useFG16Color = useBG16Color = 0;
    fgSet = bgSet = FALSE;
    for (i = 0; i <= term->argc; i++)
    {
	arg = term->argv[i];
	if (arg >= 30 && arg <= 37)
	{
	    term->currFG = arg - 30 + useFG16Color;
	    fgSet = TRUE;
	}
	else if (arg >= 40 && arg <= 47)
	{
	    term->currBG = arg - 40 + useBG16Color;
	    bgSet = TRUE;
	}
	else
	{
	    switch (arg)
	    {
		case 0:
		case -1:
		case 100:
		    term->currFG = TermNormFG;
		    term->currBG = TermNormBG;
		    term->currAttr = 0;
		    break;
		case 1:
		    if (fgSet || TermForce16Color)
		    {
			useFG16Color = 8;
			term->currFG += useFG16Color;
		    }
		    else
			term->currAttr |= ATTR_BOLD;
		    break;
		case 4:
		    term->currAttr |= ATTR_UNDERLINE;
		    break;
		case 5:
		    if (bgSet || TermForce16Color)
		    {
			useBG16Color = 8;
			term->currBG += useBG16Color;
		    }
		    else
			term->currAttr |= ATTR_BLINK;
		    break;
		case 7:
		    term->currAttr |= ATTR_REVERSE;
		    break;
		case 24:
		    term->currAttr &= ~ATTR_UNDERLINE;
		    break;
		case 25:
		    term->currAttr &= ~ATTR_BLINK;
		    break;
		case 27:
		    term->currAttr &= ~ATTR_REVERSE;
		    break;
		case 39:
		    term->currFG = TermNormFG;
		    break;
		case 48:	/* lower index ?? */
		case 49:	/* upper index ?? */
		    term->currBG = TermNormBG;
		    break;
		default:
		    break;
	    }
	}
    }
}

/* TermCursorMove() {{{1 */
static void
TermCursorMove(TermType *term)
{
    register int x, y;

    y = MAX(1, term->argv[0]);
    x = (term->argc < 1) ? 1 : MAX(1, term->argv[1]);
    term->currX = MIN(term->col - 1, MAX(0, x - 1));
    term->currY = MIN(term->row - 1, MAX(0, y - 1));
    TERM_UPDATE_MAX_REACHED(term);
}

/* TermClear2EOL() {{{1 */
static void
TermClear2EOL(TermType *term)
{
    register int n = term->col - term->currX;
    register int offset = TERM_LINENUM(term->currY) * term->col + term->currX;

    memset(term->buf + offset, 0, n);
    memset(term->color + offset, COLOR(term->currFG, term->currBG), n);
    memset(term->attr + offset, 0x00, n);
    TermExposeLine(term, term->currY, term->currX, term->col - 1);
}

/* TermClear2BOL() {{{1 */
static void
TermClear2BOL(TermType *term)
{
    register int n = term->currX + 1;
    register int offset = TERM_LINENUM(term->currY) * term->col;

    memset(term->buf + offset, 0, n);
    memset(term->color + offset, COLOR(term->currFG, term->currBG), n);
    memset(term->attr + offset, 0x00, n);
    TermExposeLine(term, term->currY, 0, term->currX);
}

/* TermClearLine() {{{1 */
static void
TermClearLine(TermType *term)
{
    register int n = term->col;
    register int offset = TERM_LINENUM(term->currY) * term->col;

    memset(term->buf + offset, 0, n);
    memset(term->color + offset, COLOR(term->currFG, term->currBG), n);
    memset(term->attr + offset, 0x00, n);
    TermExposeLine(term, term->currY, term->currX, term->col - 1);
}

/* TermLineErase() {{{1 */
static void
TermLineErase(TermType *term)
{
    switch (term->argv[0])
    {
	case 0:	/* ESC [ 0 K      clear to end of line */
	    TermClear2EOL(term);
	    break;
	case 1:	/* ESC [ 1 K      clear to begin of line */
	    TermClear2BOL(term);
	    break;
	case 2:	/* ESC [ 2 K      clear line */
	    TermClearLine(term);
	    break;
	default:
	    break;
    }
}

/* TermClear2EOS() {{{1 */
static void
TermClear2EOS(TermType *term)
{
    register int y;

    if (term->currY == 0 && term->currX == 0)
	TermClearScreen(term);
    else
    {
	TermClear2EOL(term);
	y = term->currY;
	for (term->currY++; term->currY < term->row; term->currY++)
	    TermClearLine(term);
	term->currY = y;
    }
}

/* TermClear2BOS() {{{1 */
static void
TermClear2BOS(TermType *term)
{
    register int y;

    if (term->currY == term->row - 1)
	TermClearScreen(term);
    else
    {
	TermClear2BOL(term);
	y = term->currY;
	while (--term->currY >= 0)
	    TermClearLine(term);
	term->currY = y;
    }
}

/* TermScreenErase() {{{1 */
static void
TermScreenErase(TermType *term)
{
    switch (term->argv[0])
    {
	case 0:	/* ESC [ 0 J      clear to end of screen */
	    TermClear2EOS(term);
	    break;
	case 1:	/* ESC [ 1 J      clear to begin of screen */
	    TermClear2BOS(term);
	    break;
	case 2:	/* ESC [ 2 J      clear screen and cursor home */
	    TermClearScreen(term);
	    break;
	default:
	    break;
    }
}

/* TermInsertSpaces() {{{1 */
static void
TermInsertSpaces(TermType *term, int n)
{
    register int i;
    guchar *buf, *color, *attr;

    if (n)
    {
	buf = term->buf + TERM_LINENUM(term->currY) * term->col;
	color = term->color + TERM_LINENUM(term->currY) * term->col;
	attr = term->attr + TERM_LINENUM(term->currY) * term->col;
	for (i = term->currX; i < term->col - n; i++)
	{
	    buf[i + n] = buf[i];
	    color[i + n] = color[i];
	    attr[i + n] = attr[i];
	}
	TermExposeLine(term, term->currY, term->currX, term->currX + n);
	term->currX += n;
    }
}

/* TermDeleteChars() {{{1 */
static void
TermDeleteChars(TermType *term, int n)
{
    register int i;
    guchar *buf, *color, *attr;

    if (n)
    {
	buf = term->buf + TERM_LINENUM(term->currY) * term->col;
	color = term->color + TERM_LINENUM(term->currY) * term->col;
	attr = term->attr + TERM_LINENUM(term->currY) * term->col;
	for (i = term->currX + 1; i < term->col - n; i++)
	{
	    buf[i] = buf[i + n];
	    color[i] = color[i + n];
	    attr[i] = attr[i + n];
	}
	for (i = 1; i <= n; i++)
	{
	    buf[term->col - i] = 0;
	    color[term->col - i] = color[term->col - 1];
	    attr[term->col - i] = 0;
	}
	TermExposeLine(term, term->currY, term->currX, term->currX + n);
    }
}

/* TermInsertEmptyLines() {{{1 */
static void
TermInsertEmptyLines(TermType *term, int n)
{
    int oldTop, oldBot;

    if (n)
    {
	oldTop = term->top;
	oldBot = term->bot;
	term->top = term->currY + 1;
	term->bot = term->row;
	while (n--)
	    TermScrollDown(term);
	term->top = oldTop;
	term->bot = oldBot;
    }
}

/* TermReport() {{{1 */
static void
TermReport(TermType *term)
{
    char buf[20];

    switch (term->argv[0])
    {
	case 5:	/* ESC [ 5 n      status */
	    g_snprintf(buf, sizeof(buf), "\033[0n");
	    term->remoteWrite((guchar*) buf, -1);
	    break;

	case 6:	/* ESC [ 6 n      cursor position */
	    g_snprintf(buf, sizeof(buf), "\033[%d;%dR", term->currY + 1,
		       term->currX + 1);
	    term->remoteWrite((guchar*) buf, -1);
	    break;
    }
}

/* TermDeleteLines() {{{1 */
static void
TermDeleteLines(TermType *term, int n)
{
    int oldTop, oldBot;

    if (n)
    {
	oldTop = term->top;
	oldBot = term->bot;
	term->top = term->currY + 1;
	term->bot = term->row;
	while (n--)
	    TermScrollUp(term);
	term->top = oldTop;
	term->bot = oldBot;
    }
}

/* TermCompleteDraw() {{{1 */
static void
TermCompleteDraw(TermType *term)
{
    register int y;

    for (y = 0; y < term->row; y++)
	TermExposeLine(term, y, 0, term->col - 1);

    TermShowCursor(term, TRUE);
}

/* TermStateCsi() {{{1 */
static void
TermStateCsi(TermType *term, guchar c)
{
    switch (c)
    {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    term->argv[term->argc] = 10 * term->argv[term->argc] + c - '0';
	    return;	/* do not change state */
	case ';':
	    term->argc = MIN(term->argc + 1, TERM_MAX_ARGS - 1);
	    term->argv[term->argc] = 0;
	    return;	/* do not change state */
	case 'A':	/* ESC [ Pn A     cursor up */
	    TERM_LINE_CHANGING(term);
	    TermCursorUp(term);
	    break;
	case 'B':	/* ESC [ Pn B     cursor down */
	    TERM_LINE_CHANGING(term);
	    TermCursorDown(term);
	    break;
	case 'C':	/* ESC [ Pn C     cursor right */
	    term->argv[0] = MAX(1, term->argv[0]);
	    term->currX = MIN(term->col - 1, term->currX + term->argv[0]);
	    break;
	case 'D':	/* ESC [ Pn D     cursor left */
	    term->argv[0] = MAX(1, term->argv[0]);
	    term->currX = MAX(0, term->currX - term->argv[0]);
	    break;
	case 'm':	/* ESC [ Ps ; ... ; Ps m      select graphic rendition */
	    TermSetAttr(term);
	    break;
	case 'f':	/* ESC [ Py ; Px f   direct cursor addressing */
	case 'H':	/* ESC [ Py ; Px H   direct cursor addressing */
	    TERM_LINE_CHANGING(term);
	    TermCursorMove(term);
	    break;
	case 'K':	/* ESC [ Pn K     line erase */
	    TermLineErase(term);
	    break;
	case 'J':	/* ESC [ Pn J     screen erase */
	    TermScreenErase(term);
	    break;
	case 'n':	/* ESC [ Pn n     report */
	    TermReport(term);
	    break;
	case 'r':
	    term->top = term->argv[0];
	    if (term->argc >= 1)
		term->bot = term->argv[1];
	    else
		term->bot = term->row;
	    if (term->top < 1)
		term->top = 1;
	    if (term->bot > term->row || term->bot <= 0)
		term->bot = term->row;
	    break;
	case 's':	/* save cursor position */
	    TermSaveCursorPosition(term);
	    break;
	case 'u':	/* restore cursor position */
	    TermRestoreCursorPosition(term);
	    break;
	case '=':	/* iyagi ansi code */
	    term->state = TERM_IYAGI;
	    return;
	case 'X':	/* overwrite spaces at current position */
	    TermOverwriteSpaces(term, term->argv[0]);
	    break;
	case '@':	/* insert spaces at current position */
	    if (term->argv[0] < 1)
		term->argv[0] = 1;
	    TermInsertSpaces(term, term->argv[0]);
	    break;
	case 'P':
	    if (term->argv[0] < 1)
		term->argv[0] = 1;
	    TermDeleteChars(term, term->argv[0]);
	    break;
	case 'L':
	    if (term->argv[0] < 1)
		term->argv[0] = 1;
	    TermInsertEmptyLines(term, term->argv[0]);
	    break;
	case 'M':
	    if (term->argv[0] < 1)
		term->argv[0] = 1;
	    TermDeleteLines(term, term->argv[0]);
	    break;
	case '?':
	    term->argc = 0;
	    term->argv[0] = 0;
	    term->state = TERM_DEC;
	    return;
	case 14:
	    term->charset = '0';
	    break;
	case 15:
	    term->charset = 'B';
	    break;
	case ESC:
	    /* 몰지각한 한솔 안시 로고때문에? */
	    term->argc = 0;
	    term->argv[0] = 0;
	    term->state = TERM_ESC;
	    return;
	case '>':	/* xterm escape sequence? */
	    break;
	case 'l':	/* from gau: argv[0] == 4? leave insert mode */
	case 'h':	/* from gau: argv[0] == 4? enter insert mode */
	case 'O':	/* iyagi ansi line draw */
	default:
	    OptPrintf(OPRT_TERM, "%s: char = %d,%c\n", __FUNCTION__, c, c);
	    break;
    }
    term->argc = 0;
    term->argv[0] = 0;
    term->state = TERM_GROUND;
}

/* TermStateIyagi() {{{1 */
static void
TermStateIyagi(TermType *term, guchar c)
{
    static const int iyagiColorTable[] = {
	0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15
    };

    switch (c)
    {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    term->argv[term->argc] = 10 * term->argv[term->argc] + c - '0';
	    return;

	case ';':
	    term->argc = MIN(term->argc + 1, TERM_MAX_ARGS - 1);
	    term->argv[term->argc] = 0;
	    return;	/* do not change state */

	case 'F':	/* foreground color */
	    if ((guint) term->argv[0] >= G_N_ELEMENTS(iyagiColorTable))
		OptPrintf(OPRT_TERM, "Invalid foreground color %d",
			  term->argv[0]);
	    else
		term->currFG = iyagiColorTable[term->argv[0]];
	    break;

	case 'G':	/* background color */
	    if ((guint) term->argv[0] >= G_N_ELEMENTS(iyagiColorTable))
		OptPrintf(OPRT_TERM, "Invalid background color %d",
			  term->argv[0]);
	    else
		term->currBG = iyagiColorTable[term->argv[0]];
	    break;

	default:
	    OptPrintf(OPRT_TERM, "%s: char = %d,%c\n", __FUNCTION__, c, c);
	    break;
    }
    term->state = TERM_GROUND;
}

/* TermStateScs0() {{{1 */
static void
TermStateScs0(TermType *term, guchar c)
{
    switch (c)
    {
	case '0':
	case 'B':
	case 'C':
	    term->charset = c;
	    break;

	default:
	    OptPrintf(OPRT_TERM, "%s: char = %d,%c\n", __FUNCTION__, c, c);
	    break;
    }
    term->state = TERM_GROUND;
}

/* TermStateDec() {{{1 */
static void
TermStateDec(TermType *term, guchar c)
{
    switch (c)
    {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    term->argv[term->argc] = 10 * term->argv[term->argc] + c - '0';
	    return;

	case 'h':	/* set */
	    switch (term->argv[term->argc])
	    {
		case 1:	/* application cursor keys */
		    term->mode |= MODE_CUR_APL;
		    break;
		case 1000:	/* X11 mouse reporting */
		case 1001:
		    term->mode |= MODE_EDIT_BUTTON;
		    break;
		case 3:	/* 132 column */
		    TermSetSize(term, 132, term->row);
		    break;
		case 47:	/* xterm - secondary screen? */
		    break;
		default:
		    OptPrintf(OPRT_TERM, "%s: char = h. argv[%d]=%d\n",
			      __FUNCTION__, term->argc, term->argv[term->argc]);
		    break;
	    }
	    break;

	case 'l':	/* reset */
	    switch (term->argv[term->argc])
	    {
		case 1:
		    term->mode &= ~MODE_CUR_APL;
		    break;
		case 1000:
		case 1001:
		    term->mode &= ~MODE_EDIT_BUTTON;
		    break;
		case 3:	/* 80 column */
		    TermSetSize(term, 80, term->row);
		    break;
		case 47:	/* xterm - secondary screen? */
		    break;
		default:
		    OptPrintf(OPRT_TERM, "%s: char = l. argv[%d]=%d\n",
			      __FUNCTION__, term->argc, term->argv[term->argc]);
		    break;
	    }
	    break;

	default:
	    OptPrintf(OPRT_TERM, "%s: char = %d,%c\n", __FUNCTION__, c, c);
	    break;
    }
    term->state = TERM_GROUND;
}

/* TermStateOsc() {{{1 */
static void
TermStateOsc(TermType *term, guchar c)
{
    GtkWidget *top;
    static gboolean firstTime = TRUE;
    static char *s;
    static char title[80];

    if (firstTime && isdigit(c))
	term->argv[0] = 10 * term->argv[0] + c - '0';
    else if (firstTime && !isdigit(c))
    {
	firstTime = FALSE;
	s = title;
    }
    else if (isprint(c) || (c & 0x80))
	*s++ = c;
    else
    {
	*s = '\0';
	if (term->argv[0] == 0 || term->argv[0] == 2)
	{
	    top = gtk_widget_get_toplevel(GTK_WIDGET(term));
	    gtk_window_set_title(GTK_WINDOW(top), title);
	}
	firstTime = TRUE;
	term->state = TERM_GROUND;
    }
}

/* TermReceiveByte() {{{1 */
static void
TermReceiveByte(TermType *term, guchar c)
{
    switch (term->state)
    {
	case TERM_GROUND:	/* default */
	    TermStateGround(term, c);
	    break;
	case TERM_ESC:	/* ESC */
	    TermStateEsc(term, c);
	    break;
	case TERM_CSI:	/* ESC [ */
	    TermStateCsi(term, c);
	    break;
	case TERM_IYAGI:	/* ESC [ = */
	    TermStateIyagi(term, c);
	    break;
	case TERM_IGNORE:	/* ESC [ ) */
	    term->state = TERM_GROUND;
	    break;
	case TERM_SCS0:	/* ESC [ ( */
	    TermStateScs0(term, c);
	    break;
	case TERM_DEC:	/* ESC [ ? */
	    TermStateDec(term, c);
	    break;
	case TERM_OSC:	/* ESC [ ] */
	    TermStateOsc(term, c);
	    break;
	default:
	    g_warning(_("%s: BUG! invalid term state = %d"), __FUNCTION__,
		      term->state);
	    term->state = TERM_GROUND;
	    break;
    }
}

/* TermReceiveString() {{{1 */
static int
TermReceiveString(TermType *term, const guchar *buf, int len)
{
    register int i, j;
    char *p;

    if (len < 0)
	len = strlen((char*) buf);
    if (len == 0)
	return FALSE;

    if (term->visibleLineBegin != term->realVisibleLineBegin)
    {
	if (TermCanBlock)
	{
	    TermTmpBuffer(term, buf, len);
	    return FALSE;
	}
	term->adjustment->value = term->adjustment->upper - term->row;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "value_changed");
    }
    TermHideCursor(term);

    p = g_malloc(len);
    while (len > 0)
    {
	if (term->state == TERM_GROUND)
	{
	    for (i = 0; i < (term->col - term->currX) && len > 0; i++, len--)
		if (buf[i] < 0x20)
		    break;

	    if (i)
	    {
		int realLineOffset = term->col * TERM_LINENUM(term->currY);

		if (term->charset == '0')
		{
		    for (j = 0; j < i; j++)
			p[j] = TermTransGraphicMode(buf[j]);
		}
		else if (term->charset == 'C')
		{
		    for (j = 0; j < i; j++)
		    {
			if (buf[j] >= 0x21 && buf[j] <= 0x7e)
			    p[j] = buf[j] | 0x80;
			else
			    p[j] = buf[j];
		    }
		}
		else
		    memcpy(p, buf, i);

		memcpy(term->buf + realLineOffset + term->currX, p, i);
		memset(term->color + realLineOffset + term->currX,
		       COLOR(term->currFG, term->currBG), i);
		memset(term->attr + realLineOffset + term->currX,
		       term->currAttr, i);
		TermExposeLine(term, term->currY, term->currX,
			       term->currX + i - 1);

		if (TermStripControlChar)
		    CaptureInputFilter(p, i);

		term->currX += i;
		buf += i;
	    }
	}
	if (len > 0)
	{
	    TermReceiveByte(term, *buf);
	    ++buf;
	    --len;
	}
    }
    g_free(p);

    if (!term->drawFlag)
    {
	term->drawFlag = TRUE;
	TermQueueDraw(term);
    }
    g_signal_emit(G_OBJECT(term), termSignals[TERM_RECEIVED], 0);
    TermShowCursor(term, TRUE);
    return FALSE;
}

/* TermFlush() {{{1 */
static void
TermFlush(TermType *term)
{
    if (term->tmpLen)
    {
	TermReceiveString(term, term->tmpStr, term->tmpLen);
	term->tmpLen = 0;
    }
}

/* TermTimer() {{{1 */
static gint
TermTimer(gpointer data)
{
    TermType *term;

    term = TERM(data);
    term->timer = 0;
    TermCompleteDraw(term);
    if (term->needSlideUpdate)
    {
	term->needSlideUpdate = FALSE;
	g_signal_emit_by_name(G_OBJECT(term->adjustment), "changed");
    }
    return FALSE;
}

/* TermQueueDraw() {{{1 */
static void
TermQueueDraw(TermType *term)
{
    if (!term->timer)
	term->timer = gtk_timeout_add(TERM_DRAW_TIMEOUT, TermTimer, term);
}

/* TermAdjustmentChanged() {{{1 */
static int
TermAdjustmentChanged(GtkAdjustment *adj, TermType *term)
{
    int n;

    UNUSED(adj);

    if (term->adjustment->upper > 0)
    {
	n = ((int) term->adjustment->value + TermSavedLines
	     + term->realVisibleLineBegin - (int) term->adjustment->upper
	     + term->row) % TermSavedLines;
	if (term->visibleLineBegin != n)
	{
	    term->visibleLineBegin = n;
	    if (term->realVisibleLineBegin == term->visibleLineBegin
		&& term->tmpLen)
		TermFlush(term);
	    TermQueueDraw(term);
	}
    }
    return TRUE;
}

/* TermSetScrollAdjustments() {{{1 */
static void
TermSetScrollAdjustments(TermType *term, GtkAdjustment *hadj,
			 GtkAdjustment *vadj)
{
    UNUSED(hadj);

    if (!vadj)
	vadj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 0, 1, term->row,
						 term->row));
    if (term->adjustment && term->adjustment != vadj)
    {
	g_signal_handlers_disconnect_matched(G_OBJECT(term->adjustment),
					     G_SIGNAL_MATCH_DATA,
					     0, 0, NULL, NULL, term);
	g_object_unref(G_OBJECT(term->adjustment));
    }
    if (term->adjustment != vadj)
    {
	term->adjustment = vadj;
	g_object_ref(G_OBJECT(vadj));
	gtk_object_sink(GTK_OBJECT(vadj));
	g_signal_connect(G_OBJECT(vadj), "value_changed",
			 G_CALLBACK(TermAdjustmentChanged), term);
	vadj->upper = term->row;
	vadj->page_size = term->row;
	vadj->value = vadj->upper - term->row;
	vadj->step_increment = 1;
	vadj->page_increment = term->row - 2;
    }
}

/* termIconvStr {{{1 */
static gchar *
termIconvStr(const gchar *str)
{
    gchar *buf;

    if (gu_utf8_validate_conv(str, -1, &buf, NULL) == TRUE)
	return buf;

    return (gchar *) str;
}

/* TermCommitCB() {{{1 */
static void
TermCommitCB(GtkIMContext *context, const gchar *str, TermType *term)
{
    char *buf;
    int tolen;

    UNUSED(context);

    buf = termIconvStr(str);
    tolen = strlen(buf);

    if (ximPos > 0)
    {
	int realLineOffset = term->col * TERM_LINENUM(termICSpotY);
	memcpy(term->buf + realLineOffset + termICSpotX, oldximbuf, ximPos);
	TermExposeLine(term, termICSpotY, termICSpotX,
		       termICSpotX + ximPos - 1);
	ximPos = 0;
    }

    /* remote로 문자를 보낸 후 그 문자가 다시 echo되어 터미널에 출력된 후 커서
     * 위치가 바뀐 다음에 사용자가 한글을 입력하면 좋지만 실제로는 그렇게 되지
     * 않는다. 따라서, 여기에서 일단 다음 XIM 입력위치로 바꾸어 놓는다. 만일
     * echo가 먼저 되는 경우에는 TermShowCursor()에 의해 다시 갱신된다.
     */
    termICSpotX += tolen;
    if (termICSpotX >= term->col)
    {
	/* XXX: 이렇게 해도 실제로는 입력된 초성이 반절만 보인다. */
	termICSpotX = 0;
	termICSpotY++;
	/* XXX: scroll이 필요하면 scroll? */
    }

    term->remoteWrite((guchar*) buf, tolen);
    if (buf != str)
	g_free(buf);
}

/* TermPreeditChangedCB() {{{1 */
static void
TermPreeditChangedCB(GtkIMContext *context, TermType *term)
{
    gchar *preedit_string, *buf;
    gint cursor_pos;
    int realLineOffset = term->col * TERM_LINENUM(termICSpotY);

    gtk_im_context_get_preedit_string(context, &preedit_string, NULL,
				       &cursor_pos);

    cursor_pos = ximPos;
    if (ximPos > 0)
	memcpy(term->buf + realLineOffset + termICSpotX, oldximbuf, ximPos);

    buf = termIconvStr(preedit_string);
    ximPos = strlen(buf);
    memcpy(oldximbuf, term->buf + realLineOffset + termICSpotX, ximPos);
    memcpy(term->buf + realLineOffset + termICSpotX, buf, ximPos);
    if (buf != preedit_string)
	g_free(buf);

    ximPos = MAX(ximPos, cursor_pos);
    TermExposeLine(term, termICSpotY, termICSpotX, termICSpotX + ximPos - 1);
}

/* TermClassInit() {{{1 */
static void
TermClassInit(TermClassType *klass)
{
    GtkObjectClass *oc;
    GtkWidgetClass *wc;

    oc = GTK_OBJECT_CLASS(klass);
    wc = GTK_WIDGET_CLASS(klass);
    ParentClass = gtk_type_class(GTK_TYPE_WIDGET);

    termSignals[TERM_SIZE_CHANGED]
	= g_signal_new("size_changed", G_TYPE_FROM_CLASS(oc),
		       G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(TermClassType, sizeChanged),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
    termSignals[TERM_LINE_CHANGED]
	= g_signal_new("line_changed", G_TYPE_FROM_CLASS(oc),
		       G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(TermClassType, lineChanged),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
    termSignals[TERM_RECEIVED]
	= g_signal_new("received", G_TYPE_FROM_CLASS(oc),
		       G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(TermClassType, received),
		       NULL, NULL,
		       g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    wc->expose_event = TermExpose;
    wc->key_press_event = TermKeyPress;
    wc->size_allocate = TermSizeAllocate;
    wc->size_request = TermSizeRequest;
    wc->focus_in_event = TermFocusIn;
    wc->focus_out_event = TermFocusOut;
    wc->selection_received = TermSelectionReceived;
    wc->realize = TermRealize;
    wc->unrealize = TermUnrealize;
    wc->set_scroll_adjustments_signal
	= g_signal_new("set_scroll_adjustments", G_TYPE_FROM_CLASS(oc),
		       G_SIGNAL_RUN_LAST,
		       G_STRUCT_OFFSET(TermClassType,
				       setScrollAdjustments),
		       NULL, NULL,
		       marshal_VOID__OBJECT_OBJECT,
		       G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT,
		       GTK_TYPE_ADJUSTMENT);

    oc->destroy = TermDestroy;

    {
	GObjectClass *goc = G_OBJECT_CLASS(klass);
	goc->finalize = (void (*)(GObject *))TermFinalize;
    }

    klass->sizeChanged = NULL;
    klass->received = NULL;
    klass->lineChanged = NULL;
    klass->setScrollAdjustments = TermSetScrollAdjustments;
}

/* TermInit() {{{1 */
static void
TermInit(TermType *term)
{
    GTK_WIDGET_SET_FLAGS(term, GTK_CAN_FOCUS);
    GTK_WIDGET_UNSET_FLAGS(term, GTK_NO_WINDOW);

    term->drawFlag = TRUE;
    term->state = TERM_GROUND;
    term->realVisibleLineBegin = 0;
    term->visibleLineBegin = 0;
    term->currX = term->currY = 0;
    term->currAttr = 0;
    term->charset = 'B';
    term->mode = 0;
    term->tmpAlloc = 256;
    term->tmpStr = g_new(guchar, term->tmpAlloc);
    term->tmpLen = 0;
    term->timer = 0;

    term->needSlideUpdate = FALSE;

    term->ic = gtk_im_multicontext_new();
    term->icReset = FALSE;
    g_signal_connect(G_OBJECT(term->ic), "commit",
		     G_CALLBACK(TermCommitCB), term);
    g_signal_connect(G_OBJECT(term->ic), "preedit_changed",
		     G_CALLBACK(TermPreeditChangedCB), term);
}

/* TermGetType() {{{1 */
static GType
TermGetType(void)
{
    static GType termType = 0;

    if (!termType)
    {
	static const GTypeInfo termInfo = {
	    sizeof(TermClassType),
	    NULL, /* base_init */
	    NULL, /* base_finalize */
	    (GClassInitFunc) TermClassInit,
	    NULL, /* class_finalize */
	    NULL, /* class_data */
	    sizeof(TermType),
	    1, /* n_preallocs */
	    (GInstanceInitFunc) TermInit,
	    NULL /* value_table */
	};

	termType = g_type_register_static(GTK_TYPE_WIDGET, "Term",
					  &termInfo, 0);
    }
    return termType;
}

/* TermNew() {{{1 */
TermType *
TermNew(void)
{
    TermType *term;

    term = g_object_new(TermGetType(), NULL);

    term->gudraw = gu_draw_new(GTK_WIDGET(term), TermFontName);

    term->fontWidth = term->gudraw->fontWidth;
    term->fontHeight = term->gudraw->fontHeight;
    term->fontAscent = term->gudraw->fontAscent;

    /* check the overflow/underflow */
    if (TermCol <= 0)
	TermCol = DEFAULT_COLUMN;
    if (TermRow <= 0)
	TermRow = DEFAULT_ROW;
    if ((int) TermSavedLines < term->row)
	TermSavedLines = term->row;
    if ((guint) TermSavedLines > (guint) MAX_SAVED_LINES)
	TermSavedLines = MAX_SAVED_LINES;
    if (MouseButtonSwap)
    {
	MouseLeftButtonNum = 3;
	MouseRightButtonNum = 1;
    }
    else
    {
	MouseLeftButtonNum = 1;
	MouseRightButtonNum = 3;
    }

    term->col = TermCol;
    term->row = TermRow;
    term->top = 1;
    term->bot = term->row;

    term->adjustment = NULL;
    term->buf = g_new(guchar, term->col * TermSavedLines);
    term->color = g_new(guchar, term->col * TermSavedLines);
    term->attr = g_new(guchar, term->col * TermSavedLines);

    termSelectBX = 0;
    termSelectEX = 0;
    termSelectBY = 0;
    termSelectEY = 0;
    termSelClick.x = -1;

    term->localWrite = TermReceiveString;
    /* term->remoteWrite은 Modem이냐 telnet이냐에 의해 나중에 필요에 따라
     * 재초기화됨.
     */
    term->remoteWrite = DummyFromTerm;

    TermInitKeyMap();

    g_signal_connect(G_OBJECT(term), "received",
		     G_CALLBACK(TermReceived), NULL);
    g_signal_connect(G_OBJECT(term), "size_changed",
		     G_CALLBACK(TermSizeChanged), NULL);
    g_signal_connect(G_OBJECT(term), "selection_get",
		     G_CALLBACK(TermSelectionGet), NULL);
    g_signal_connect(G_OBJECT(term), "button_press_event",
		     G_CALLBACK(TermButtonPress), NULL);
    g_signal_connect(G_OBJECT(term), "button_release_event",
		     G_CALLBACK(TermButtonRelease), NULL);
    g_signal_connect(G_OBJECT(term), "motion_notify_event",
		     G_CALLBACK(TermMotionNotify), NULL);
    g_signal_connect(G_OBJECT(term), "selection_clear_event",
		     G_CALLBACK(TermSelectionClear), NULL);

    DisconnectFuncList = g_slist_append(DisconnectFuncList, TermMakeSane);

    /* script에서 message 명령등에 의해 문자가 출력될 수 있기 때문에
     * 언제든 기본 charset으로 conversion이 가능한 상태로 만들어 놓는다.
     */
    TermConvOpen(term, RemoteCharset);

    return term;
}

/* TermConvOpen() {{{1 */
void
TermConvOpen(TermType *term, const char *remote_charset)
{
    gboolean is_local_utf8, is_remote_utf8;

    TermConvClose();

    if (remote_charset == NULL)
	remote_charset = RemoteCharset;

    is_local_utf8 = g_get_charset(NULL);
    is_remote_utf8 = (strcasecmp(remote_charset, "UTF-8") == 0);

    term->isLocalUtf8 = is_local_utf8;
    term->isRemoteUTF8 = is_remote_utf8;
}

/* TermConvClose() {{{1 */
void
TermConvClose(void)
{
}

/* termSelectClear() {{{1 */
static void
_termSelectClear(TermType *term, int y, int x1, int x2)
{
    register int i;
    guchar *attr = term->attr + TERM_LINENUM(y) * term->col + x1;

    for (i = x1; i <= x2; i++, attr++)
	*attr &= ~ATTR_SELECTION;
    TermExposeLine(term, y, x1, x2);
}

static void
termSelectClear(TermType *term, int bx, int by, int ex, int ey)
{
    int i;
    if (by == ey)
	_termSelectClear(term, by, bx, ex);
    else
    {
	_termSelectClear(term, by, bx, term->col - 1);
	for (i = by + 1; i < ey; i++)
	    _termSelectClear(term, i, 0, term->col - 1);
	_termSelectClear(term, ey, 0, ex);
    }
}

/* TermUnselect() {{{1 */
static void
TermUnselect(TermType *term)
{
    if (termSelectExist)
	termSelectClear(term, termSelectBX, termSelectBY,
			termSelectEX, termSelectEY);

    if (gdk_selection_owner_get(GDK_SELECTION_PRIMARY)
	== GTK_WIDGET(term)->window)
	gtk_selection_owner_set(NULL, GDK_SELECTION_PRIMARY,
				GDK_CURRENT_TIME);

    termSelectSize = 0;
    termSelectExist = FALSE;
    termSelClick.x = -1;
    termSelectOldWBX = 99999;
    termSelectOldWEX = -1;
    termSelectWBX = -1;
    termSelectWEX = -1;

}

/* TermReceived() {{{1 */
static void
TermReceived(TermType *term)
{
    if (CURR_ATTR(term) & ATTR_SELECTION)
	TermUnselect(term);
}

/* TermSelectionGet() {{{1 */
static void
TermSelectionGet(TermType *term, GtkSelectionData *selData)
{
    UNUSED(term);

    if (termSelectSize)
    {
	GdkAtom e;
	gint f, l;
	guchar *t = NULL;
	gdk_string_to_compound_text(termSelectBuf, &e, &f, &t, &l);
	if (t == NULL)
	{
	    e = GDK_SELECTION_TYPE_STRING;
	    f = 8;
	    t = (guchar*) termSelectBuf;
	    l = termSelectSize;
	}
	gtk_selection_data_set(selData, e, f, t, l);
	if (t != (guchar*) termSelectBuf)
	    gdk_free_compound_text(t);
    }
}

/* TermSelectionClear() {{{1 */
static int
TermSelectionClear(TermType *term, GdkEventSelection *ev)
{
    UNUSED(ev);
    TermUnselect(term);
    return TRUE;
}

/* termSelectGetLast() {{{1 */
static int
termSelectGetLast(TermType *term, int y, int x1, int x2)
{
    int i;
    guchar *p;

    p = term->buf + TERM_LINENUM(y) * term->col + x2;
    for (i = x2; i < term->col; i++, p++)
	if (*p != 0)
	    break;
    if (i == term->col)
    {
	p = term->buf + TERM_LINENUM(y) * term->col + x2;
	for (i = x2; i >= x1; i--, p--)
	    if (*p != 0)
		return i;
	return -1;
    }
    return x2;
}

/* termSelectMark() {{{1 */
static void
_termSelectMark(TermType *term, int y, int x1, int x2)
{
    register int i;
    register guchar *attr;

    x2 = termSelectGetLast(term, y, x1, x2);
    if (x2 < 0)
	return;

    attr = term->attr + TERM_LINENUM(y) * term->col + x1;
    for (i = x1; i <= x2; i++, attr++)
	*attr |= ATTR_SELECTION;
    TermExposeLine(term, y, x1, i);
}

static void
termSelectMark(TermType *term, int bx, int by, int ex, int ey)
{
    int i;
    if (by == ey)
	_termSelectMark(term, by, bx, ex);
    else
    {
	_termSelectMark(term, by, bx, term->col - 1);
	for (i = by + 1; i < ey; i++)
	    _termSelectMark(term, i, 0, term->col - 1);
	_termSelectMark(term, ey, 0, ex);
    }
}

/* TermSelectMotion() {{{1 */
static void
TermSelectMotion(TermType *term, int x, int y)
{
    register int bx, by, ex, ey, wbx = x, wex = x;
    gboolean clr = FALSE;

    if (x >= term->col)
	x = term->col - 1;
    if (y >= term->row)
	y = term->row - 1;

    if (termSelectMode == TERM_SELECT_WORD)
    {
	/* 현재 마우스 위치의 단어의 시작과 끝 찾음 {{{ */
	if (y == termMousePosY
	    && (termSelectOldWBX <= x && x <= termSelectOldWEX))
	    return;

	wbx = termGetWordStart(term, x, y);
	wex = termGetWordEnd(term, x, y);

	termSelectOldWBX = wbx;
	termSelectOldWEX = wex;
	/* }}} */
    }
    else if (termSelectMode == TERM_SELECT_LINE)
    {
	if (y == termMousePosY)
	    return;
    }
    else
    {
	if (y == termMousePosY
	    && (termSelectOldWBX <= x && x <= termSelectOldWEX))
	    return;
    }

    if (y < termSelectBY
	|| (y == termSelectBY && x < termSelectBX))
    {
	/* 왼쪽/위로 선택 진행 {{{ */
	if (termSelDirection == SEL_DIR_RIGHT_DOWN)
	{
	    if (termSelectMode == TERM_SELECT_CHAR)
	    {
		termSelectClear(term, termSelectBX + 1, termSelectBY,
				termSelectEX, termSelectEY);
		termSelectEX = termSelectBX;
		termSelectEY = termSelectBY;
	    }
	    else if (termSelectMode == TERM_SELECT_WORD)
	    {
		termSelectClear(term, termSelectWEX + 1, termSelectFY,
				termSelectEX, termSelectEY);
		termSelectEX = termSelectWEX;
		termSelectEY = termSelectFY;
	    }
	}

	if (termSelectMode == TERM_SELECT_LINE)
	    x = 0;
	else if (termSelectMode == TERM_SELECT_WORD)
	    x = wbx;
	else
	{
	    if (IsSecondMB((char*) term->buf + term->col * TERM_LINENUM(y), x))
	    {
		termSelectOldWEX = x;
		--x;
		termSelectOldWBX = x;
	    }
	    else
		termSelectOldWBX = termSelectOldWEX = x;
	}
	bx = x;
	by = y;
	if (termSelectBX == 0)
	{
	    ex = term->col - 1;
	    ey = termSelectBY - 1;
	}
	else
	{
	    ex = termSelectBX - 1;
	    ey = termSelectBY;
	}
	termSelDirection = SEL_DIR_LEFT_UP;
	termSelectBX = x;
	termSelectBY = y;
	/* }}} */
    }
    else if (y > termSelectEY 
	     || (y == termSelectEY && x > termSelectEX))
    {
	/* 오른쪽/아래로 선택 진행 {{{ */
	if (termSelDirection == SEL_DIR_LEFT_UP)
	{
	    if (termSelectMode == TERM_SELECT_CHAR)
	    {
		termSelectClear(term, termSelectBX, termSelectBY,
				termSelectEX - 1, termSelectEY);
		termSelectBX = termSelectEX;
		termSelectBY = termSelectEY;
	    }
	    else if (termSelectMode == TERM_SELECT_WORD)
	    {
		termSelectClear(term, termSelectBX, termSelectBY,
				termSelectWBX - 1, termSelectFY);
		termSelectBX = termSelectWBX;
		termSelectBY = termSelectFY;
	    }
	}

	if (termSelectMode == TERM_SELECT_LINE)
	    x = term->col - 1;
	else if (termSelectMode == TERM_SELECT_WORD)
	{
	    x = wex;
	    if (termSelectEX != termSelectWBX
		&& termSelectEX != termSelectWEX)
		termSelectEX++;
	}
	else
	{
	    if (IsFirstMB((char*) term->buf + term->col * TERM_LINENUM(y), x))
	    {
		termSelectOldWBX = x;
		++x;
		termSelectOldWEX = x;
	    }
	    else
		termSelectOldWBX = termSelectOldWEX = x;
	}

	if (termSelectEX == term->col - 1)
	{
	    bx = 0;
	    by = termSelectEY;
	}
	else
	{
	    bx = termSelectEX;
	    by = termSelectEY;
	}
	ex = x;
	ey = y;
	termSelDirection = SEL_DIR_RIGHT_DOWN;
	termSelectEX = x;
	termSelectEY = y;
	/* }}} */
    }
    else
    {
	if (termSelDirection == SEL_DIR_RIGHT_DOWN)
	{
	    /* 오른쪽/아래로 진행하다가 방향을 바꿈 {{{ */
	    if (termSelectMode == TERM_SELECT_LINE)
		x = 0;
	    else if (termSelectMode == TERM_SELECT_WORD)
		x = wex + 1;
	    else
	    {
		if (IsFirstMB((char*) term->buf + term->col * TERM_LINENUM(y),
			      x))
		{
		    termSelectOldWBX = x;
		    termSelectOldWEX = x + 1;
		    x += 2;
		}
		else
		{
		    if (IsSecondMB(((char*) term->buf
				    + term->col * TERM_LINENUM(y)), x))
		    {
			termSelectOldWBX = x - 1;
			termSelectOldWEX = x;
		    }
		    else
			termSelectOldWBX = termSelectOldWEX = x;
		    ++x;
		}
	    }
	    bx = x;
	    by = y;
	    ex = termSelectEX;
	    ey = termSelectEY;
	    if (termSelectMode != TERM_SELECT_LINE)
		termSelectEX = x - 1;
	    termSelectEY = y;
	    /* }}} */
	}
	else
	{
	    /* 왼쪽/위로 진행하다가 방향을 바꿈 {{{ */
	    if (termSelectMode == TERM_SELECT_LINE)
		x = term->col - 1;
	    else if (termSelectMode == TERM_SELECT_WORD)
		x = wbx - 1;
	    else
	    {
		if (IsSecondMB(((char*) term->buf
				+ term->col * TERM_LINENUM(y)), x))
		{
		    termSelectOldWBX = x - 1, termSelectOldWEX = x;
		    x -= 2;
		}
		else
		{
		    if (IsFirstMB(((char*) term->buf
				   + term->col * TERM_LINENUM(y)), x))
			termSelectOldWBX = x, termSelectOldWEX = x + 1;
		    else
			termSelectOldWBX = termSelectOldWEX = 0;
		    --x;
		}
	    }
	    bx = termSelectBX;
	    by = termSelectBY;
	    ex = x;
	    ey = y;
	    if (termSelectMode != TERM_SELECT_LINE)
		termSelectBX = x + 1;
	    termSelectBY = y;
	    /* }}} */
	}
	clr = TRUE;
    }

    /* mouse가 창 밖에서 움직이는 경우에는 line 전체를 선택.
     * 'y'가 음수인 경우에는 무시.
     */
    if (bx < 0)
	bx = 0;
    if (ex < 0)
	ex = term->col;

    if (clr)
    {
	if (termSelectMode == TERM_SELECT_LINE)
	{
	    if (by == termSelectFY)
		by++;
	    if (ey == termSelectFY)
		ey--;
	    if (by > ey)
		return;
	    if (termSelectBY > termSelectFY)
		termSelectBY = termSelectFY;
	    if (termSelectEY < termSelectFY)
		termSelectEY = termSelectFY;
	}
	else
	{
	    if (by == termSelectFY || ey == termSelectFY)
	    {
		if (by == termSelectFY
		    && termSelectWBX <= bx && bx <= termSelectWEX)
		    bx = termSelectWEX + 1;
		if (ey == termSelectFY
		    && termSelectWBX <= ex && ex <= termSelectWEX)
		    ex = termSelectWBX - 1;
		if (by == ey && bx > ex)
		    return;
		if (termSelectBX > termSelectWBX)
		    termSelectBX = termSelectWBX;
		if (termSelectEX < termSelectWEX)
		    termSelectEX = termSelectWEX;
	    }
	}
	termSelectClear(term, bx, by, ex, ey);
    }
    else
	termSelectMark(term, bx, by, ex, ey);
}

/* TermAllocSelectBuf() {{{1 */
static void
TermAllocSelectBuf(int len)
{
    static guint bufSize = 0;

    if (termSelectSize + len > bufSize)
    {
	bufSize = termSelectSize + len + 80;
	termSelectBuf = g_realloc(termSelectBuf, bufSize);
    }
}

/* TermSelectEnd() {{{1 */
static void
TermSelectEnd(TermType *term)
{
    register int i, j;
    guchar *buf, *termBuf;

    termCursorBlinkStart(term);
    TermShowCursor(term, TRUE);

    termBuf = (term->buf + term->col * TERM_LINENUM(termSelectBY)
	       + termSelectBX);
    if (termSelectBY == termSelectEY)
    {
	termSelectEX = termSelectGetLast(term, termSelectBY,
					   termSelectBX, termSelectEX);
	if (termSelectEX < 0)
	    return;

	i = termSelectEX - termSelectBX + 1;
	TermAllocSelectBuf(i);
	memcpy(termSelectBuf, termBuf, i);
	termSelectSize = i;
    }
    else
    {
	int x;
	x = termSelectGetLast(term, termSelectBY, termSelectBX,
			      term->col - 1);
	if (x >= 0)
	    i = x - termSelectBX + 1;
	else
	    i = 0;
	TermAllocSelectBuf(i + 1);
	if (i > 0)
	    memcpy(termSelectBuf, termBuf, i);
	buf = (guchar*) termSelectBuf + i;
	*buf = '\n';
	termBuf += term->col - termSelectBX;
	termSelectSize = i + 1;
	for (j = termSelectBY + 1; j < termSelectEY; j++)
	{
	    x = termSelectGetLast(term, j, 0, term->col - 1);
	    if (x >= 0)
		i = x + 1;
	    else
		i = 0;
	    TermAllocSelectBuf(i + 1);
	    buf = (guchar*) termSelectBuf + termSelectSize;
	    if (i > 0)
	    {
		memcpy(buf, termBuf, i);
		buf += i;
	    }
	    *buf = '\n';
	    termSelectSize += i + 1;
	    termBuf += term->col;
	}
	x = termSelectGetLast(term, termSelectEY, 0, termSelectEX);
	if (x >= 0)
	{
	    i = x + 1;
	    TermAllocSelectBuf(i);
	    buf = (guchar*) termSelectBuf + termSelectSize;
	    memcpy(buf, termBuf, i);
	    termSelectSize += i;
	}
    }

    if (termSelectMode == TERM_SELECT_LINE)
    {
	TermAllocSelectBuf(1);
	buf = (guchar*) termSelectBuf + termSelectSize;
	*buf = '\n';
	termSelectSize++;
    }

    TermAllocSelectBuf(1);
    buf = (guchar*) termSelectBuf + termSelectSize;
    *buf = 0;
    termSelectSize++;
}

/* termSelectStartChar() {{{1 */
static void
termSelectStartChar(TermType *term, int x, int y)
{
    termCursorBlinkStop(term);
    TermHideCursor(term);

    x /= term->fontWidth;
    y /= term->fontHeight;

    termSelectBY = termSelectEY = y;
    termSelectFY = y;
    termSelDirection = SEL_DIR_RIGHT_DOWN;

    termSelectBX = x;
    termSelectWBX = x;
    termSelectWEX = x;
    if (IsFirstMB((char*) term->buf + term->col * TERM_LINENUM(y), x))
    {
	termSelectWBX = x;
	termSelectWEX = x + 1;
    }
    else if (IsSecondMB((char*) term->buf + term->col * TERM_LINENUM(y), x))
    {
	termSelectWBX = x - 1;
	termSelectWEX = x;
	termSelectBX = x - 1;
    }
    termSelectEX = termSelectBX;
    termSelectMark(term, termSelectBX, termSelectBY, termSelectEX,
		   termSelectEY);
}

/* termGetWordStart() {{{1 */
static int
termGetWordStart(TermType *term, int x, int y)
{
    char *buf;
    char *sep = TermWordSeperator;
    if (sep == NULL)
	sep = "";

    buf = (char *) (term->buf + term->col * TERM_LINENUM(y) + x);

    if (*buf == 0)
    {
	for (; x > 0; x--, buf--)
	{
	    if (*buf != 0)
		break;
	}
    }

    if (!isspace(*buf))
    {
	if (strchr(sep, *buf) == NULL)
	{
	    for (; x > 0; x--, buf--)
	    {
		if (isspace(*buf) || strchr(sep, *buf) != NULL)
		{
		    x++;
		    break;
		}
	    }
	}
    }
    else
    {
	for (; x > 0; x--, buf--)
	{
	    if (!isspace(*buf))
	    {
		x++;
		break;
	    }
	}
    }

    return x;
}

/* termGetWordEnd() {{{1 */
static int
termGetWordEnd(TermType *term, int x, int y)
{
    char *buf;
    char *sep = TermWordSeperator;
    if (sep == NULL)
	sep = "";

    buf = (char *) (term->buf + term->col * TERM_LINENUM(y) + x);
    if (*buf == 0)
    {
	for (; x > 0; x--, buf--)
	    if (*buf != 0)
		break;
    }
    else if (!isspace(*buf))
    {
	if (strchr(sep, *buf) == NULL)
	{
	    for (; x < term->col; x++, buf++)
	    {
		if (isspace(*buf) || *buf == 0
		    || strchr(sep, *buf) != NULL)
		{
		    x--;
		    break;
		}
	    }
	}
    }
    else
    {
	for (; x < term->col; x++, buf++)
	{
	    if (!isspace(*buf) || *buf == 0)
	    {
		x--;
		break;
	    }
	}
    }

    return x;
}

/* termSelectStartWord() {{{1 */
static void
termSelectStartWord(TermType *term, GdkEventButton *ev)
{
    register int x, y;

    termCursorBlinkStop(term);
    TermHideCursor(term);

    x = ev->x / term->fontWidth;
    y = ev->y / term->fontHeight;

    termSelectBX = termGetWordStart(term, x, y);
    termSelectEX = termGetWordEnd(term, x, y);
    termSelectBY = termSelectEY = y;

    termSelectWBX = termSelectBX;
    termSelectWEX = termSelectEX;
    termSelectFY = y;

    termSelDirection = SEL_DIR_RIGHT_DOWN;
    termSelectMark(term, termSelectBX, termSelectBY, termSelectEX,
		   termSelectEY);
}

/* termSelectStartLine() {{{1 */
static void
termSelectStartLine(TermType *term, GdkEventButton *ev)
{
    termCursorBlinkStop(term);
    TermHideCursor(term);

    termSelectBX = 0;
    termSelectEX = term->col - 1;
    termSelectBY = termSelectEY = ev->y / term->fontHeight;
    termSelectFY = termSelectBY;

    termSelDirection = SEL_DIR_RIGHT_DOWN;
    termSelectMark(term, termSelectBX, termSelectBY, termSelectEX,
		   termSelectEY);
}
/* }}} */

#ifdef _USE_POPUP_MENU_
/* termPopupExample() {{{1 */
static void
termPopupExample(GtkWidget *w, gpointer data)
{
    UNUSED(w);
    UNUSED(data);
    g_print("Example popup menu called\n");
}

/* termMenuPopup() {{{1 */
static void
termMenuPopup(void)
{
    static GtkItemFactory *PopupMenu = NULL;
    static GtkItemFactoryEntry entry[] = {
	{"/Example", NULL, termPopupExample, 0, "<Item>", NULL },
    };

    gtk_grab_remove(GTK_WIDGET(Term));

    if (!PopupMenu)
    {
	/* create popup menu */
	PopupMenu = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);
	gtk_item_factory_create_items(PopupMenu, G_N_ELEMENTS(entry), entry, NULL);
    }
    gtk_menu_popup(GTK_MENU(PopupMenu->widget), NULL, NULL, NULL, NULL, 0, 0);
}
/* }}} */
#endif /* _USE_POPUP_MENU_ */

/* TermButtonPress() {{{1 */
static int
TermButtonPress(TermType *term, GdkEventButton *ev)
{
    GtkWidget *w = GTK_WIDGET(term);

    if (!GTK_WIDGET_HAS_FOCUS(w))
	gtk_widget_grab_focus(w);

    if (DoSelect && ev->button != 2)
    {
	TermUnselect(term);
	DoSelect = FALSE;
    }

    DoISelect = FALSE;

    if (ev->button == MouseLeftButtonNum)
    {
	TermUnselect(term);
	DoSelect = TRUE;
	gtk_grab_add(w);
	if (ev->type == GDK_2BUTTON_PRESS)
	{
	    termSelectMode = TERM_SELECT_WORD;
	    termSelectExist = TRUE;
	    termSelectStartWord(term, ev);
	    termSelClick.x = -1;
	}
	else if (ev->type == GDK_3BUTTON_PRESS)
	{
	    termSelectMode = TERM_SELECT_LINE;
	    termSelectExist = TRUE;
	    termSelectStartLine(term, ev);
	    termSelClick.x = -1;
	}
	else
	{
	    termSelectMode = TERM_SELECT_CHAR;
	    termSelClick = *ev;
	}
	termMousePosX = ev->x / term->fontWidth;
	termMousePosY = ev->y / term->fontHeight;
    }
    else if (ev->button == 2 && ev->type == GDK_BUTTON_PRESS)
    {
	gtk_selection_convert(w, GDK_SELECTION_PRIMARY, GDK_TARGET_STRING,
			      ev->time);
    }
    else if (ev->button == MouseRightButtonNum)
    {
	if (ev->type != GDK_2BUTTON_PRESS && ev->type != GDK_3BUTTON_PRESS)
	{
	    if (ev->state & GDK_CONTROL_MASK)
	    {
#ifdef _USE_POPUP_MENU_
		termMenuPopup();
#endif
	    }
	    else
	    {
		gtk_grab_add(w);
		if (term->mode & MODE_EDIT_BUTTON)
		{
		    char buf[10];
		    guchar x = ev->x / term->fontWidth + ' ' + 1;
		    guchar y = ev->y / term->fontHeight + ' ' + 1;

		    g_snprintf(buf, sizeof(buf), "\033[M%c%c%c", ' ', x, y);
		    term->remoteWrite((guchar*) buf, -1);
		}
		else
		{
		    DoISelect = TRUE;
		    I_SelectBegin(term, ev->x, ev->y);
		}
	    }
	}
    }
    return TRUE;
}

/* TermButtonRelease() {{{1 */
static int
TermButtonRelease(TermType *term, GdkEventButton *ev)
{
    GtkWidget *w = GTK_WIDGET(term);

    gtk_grab_remove(w);
    if (ev->button == MouseLeftButtonNum)
    {
	if (termSelectExist)
	{
	    TermSelectEnd(term);
	    gtk_selection_owner_set(w, GDK_SELECTION_PRIMARY,
				    ev->time);
	}
	else
	{
	    if (LeftMouseClickStr)
	    {
		guint32 currTime;
		static guint32 lastSendTime = 0UL;

		currTime = gdk_time_get();
		if (currTime > lastSendTime + 500)
		{
		    term->remoteWrite((guchar*) LeftMouseClickStr, -1);
		    StatusShowMessage(" ");	/* clear status line */
		}
		lastSendTime = currTime;
	    }
	}
    }
    else if (ev->button == MouseRightButtonNum)
    {
	if (ev->state & GDK_CONTROL_MASK)
	{
	}
	else
	{
	    if (term->mode & MODE_EDIT_BUTTON)
	    {
		char buf[10];
		guchar x = ev->x / term->fontWidth + ' ' + 1;
		guchar y = ev->y / term->fontHeight + ' ' + 1;

		g_snprintf(buf, sizeof(buf), "\033[M%c%c%c", ' ' + 3, x, y);
		term->remoteWrite((guchar*) buf, -1);
	    }
	    I_SelectEnd(term, ev->x, ev->y);
	}
    }
    return TRUE;
}

/* TermMotionNotify() {{{1 */
static int
TermMotionNotify(TermType *term, GdkEventButton *ev)
{
    if (DoISelect)
	I_SelectMotion(term, ev->x, ev->y);
    else if (DoSelect)
    {
	int x, y;
	x = ev->x / term->fontWidth;
	y = ev->y / term->fontHeight;
	if (x != termMousePosX || y != termMousePosY)
	{
	    termSelectExist = TRUE;
	    if (termSelClick.x >= 0)
	    {
		termSelectStartChar(term, termSelClick.x,
				    termSelClick.y);
		termSelClick.x = -1;
	    }
	    TermSelectMotion(term, x, y);
	    termMousePosX = x;
	    termMousePosY = y;
	}
    }

    return TRUE;
}

/* TermSizeChanged() {{{1 */
static void
TermSizeChanged(TermType *term)
{
    OptPrintf(OPRT_TERM, "%s: col=%d, row=%d\n", __FUNCTION__, term->col,
	      term->row);

    if (ReadFD > 0 && !UseModem)
	PTY_SetWinSize(ReadFD, term->col, term->row);

#if defined(HAVE_PUTENV) || defined(HAVE_SETENV)
    {
	char buf[128];

#ifdef HAVE_PUTENV
	g_snprintf(buf, sizeof(buf), "COLUMNS=%d", term->col);
	putenv(buf);
	g_snprintf(buf, sizeof(buf), "LINES=%d", term->row);
	putenv(buf);
#else /* HAVE_SETENV */
	g_snprintf(buf, sizeof(buf), "%d", term->col);
	setenv("COLUMNS", buf, 1);
	g_snprintf(buf, sizeof(buf), "%d", term->row);
	setenv("LINES", buf, 1);
#endif /* HAVE_SETENV */
    }
#endif /* HAVE_PUTENV || HAVE_SETENV */
}

/* TermLineChanged() {{{1 */
void
TermLineChanged(TermType *term)
{
    term = NULL;
}

/* TermInputHandler() {{{1 */
gboolean
TermInputHandler(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    register int size, newSize;
    GSList *slist;
    long lf;
    static guint32 lastTime = 0;
    guint32 thisTime = 0;	/* to make gcc happy */
    static char tmpbuf[528], oldchar;
    char *buf, *inbuf;
    int source, bufsize;

    UNUSED(cond);
    UNUSED(data);

    if (TermReadHack)
	thisTime = gdk_time_get();

    source = g_io_channel_unix_get_fd(channel);

    if (oldchar)
    {
	tmpbuf[0] = oldchar;
	buf = tmpbuf + 1;
	bufsize = sizeof(tmpbuf) - 2;
    }
    else
    {
	buf = tmpbuf;
	bufsize = sizeof(tmpbuf) - 1;
    }

    inbuf = tmpbuf;
    size = read(source, buf, bufsize);
    if (size <= 0)
    {
	/* remove host로부터 자동으로 끊어진 경우 */
	if (TermInputChannel)
	{
	    GSource *src;

	    g_io_channel_unref(TermInputChannel);
	    TermInputChannel = 0;

	    src = g_main_context_find_source_by_id(NULL, TermInputID);
	    if (src)
		g_source_destroy(src);
	}
	close(ReadFD);
	ReadFD = -1;
    }
    else
    {
	if (TermReadHack && (size < bufsize))
	{
	    if (thisTime - lastTime < 250)
	    {
		for (;;)
		{
		    g_usleep(TermReadHack);
		    ioctl(source, FIONREAD, &lf);
		    if (!lf)
			break;
		    newSize = read(source, buf + size, bufsize - size);
		    if (newSize <= 0)
			break;
		    size += newSize;
		    if (size >= bufsize)
			break;
		}
		lastTime = gdk_time_get();
	    }
	    else
		lastTime = thisTime;
	}

	/* terminal에 무언가 들어왔으므로 auto-response 기능을 켠다. */
	AR_CheckFlag = TRUE;

	if (Term->isRemoteUTF8)
	{
	    gsize bw;

	    if (gu_utf8_validate_conv(tmpbuf, size, &inbuf, &bw) == FALSE)
		goto no_conv;

	    size = bw;
	}
	else
	{
no_conv:
	    inbuf = tmpbuf;
	    if (oldchar)
	    {
		size++;
		oldchar = 0;
	    }

	    if (IsFirstMB(tmpbuf, size - 1))
	    {
		oldchar = tmpbuf[size - 1];
		size--;
	    }
	    tmpbuf[size] = '\0'; /* safety */
	}

	for (slist = InputFilterList; slist;)
	{
	    int (*func) (char *, int);

	    func = slist->data;

	    /* InputFilterList가 func call에 의해 바뀔 수 있으므로 여기에서 미리
	     * 다음 list를 가져온다. 그렇지 않을 경우 list->data가 invalid한
	     * 경우가 발생된다.
	     */
	    slist = slist->next;

	    size = (*func)(inbuf, size);
	    if (!size)
		goto out;
	}

	if (DoRemoteEcho)
	{
	    register int i;
	    guchar c = '\n';

	    for (i = 0; i < size; i++)
	    {
		if (inbuf[i] != '\r')
		    Term->remoteWrite((guchar*) inbuf + i, 1);
		else
		{
		    Term->remoteWrite((guchar*) inbuf + i, 1);
		    Term->remoteWrite(&c, 1);
		}
	    }
	}

	Term->localWrite(Term, (guchar*) inbuf, size);

	ControlLabelLineStsToggle('r');
    }

out:
    if (inbuf != tmpbuf)
	g_free(inbuf);

    return TRUE;
}

/* TermEmulModeSet() {{{1 */
void
TermEmulModeSet(void)
{
#ifdef HAVE_PUTENV
    if (EmulateMode == EMULATE_VT100)
	putenv("TERM=vt100");
    else
	putenv("TERM=ansi");
#else
# ifdef HAVE_SETENV
    if (EmulateMode == EMULATE_VT100)
	setenv("TERM", "vt100", 1);
    else
	setenv("TERM", "ansi", 1);
# else
#  warning "Cannot set TERM environment."
# endif
#endif

    ControlEmulateChange();
}

/* DummyFromTerm() {{{1 */
/* for telnet connect */
int
DummyFromTerm(const guchar *buf, int len)
{
    UNUSED(buf);
    UNUSED(len);
    return 0;
}

/* TermExit() {{{1 */
void
TermExit(void)
{
    GSList *l;
    TermType *term = Term;

    if (term)
    {
	g_free(term->buf);
	g_free(term->color);
	g_free(term->attr);
	g_free(term->tmpStr);

	g_object_unref(term->gc);
	g_object_unref(term->whiteGC);
	g_object_unref(term->blackGC);
	g_object_unref(term->bgGC);
	g_object_unref(term->cursorGC);
	g_object_unref(term->iSelGC);
	g_object_unref(term->ic);

	gu_draw_free(term->gudraw);

	if (ColorMap)
	{
	    gdk_colormap_free_colors(ColorMap, (GdkColor *)term->colorTable,
				     TERM_COLOR_END);
	    g_free(term->colorTable);
	}
	Term = NULL;
    }

    TermConvClose();

    if (TermKeyMapList)
    {
	for (l = TermKeyMapList; l; l = l->next)
	    g_free(l->data);
	g_slist_free(TermKeyMapList);
	TermKeyMapList = NULL;
    }
}

