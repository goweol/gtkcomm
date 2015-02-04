/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Terminal
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
#ifndef __PCTERM_H
#define __PCTERM_H 1

#include "pcSetting.h"
#include "gtkutil.h"

#define TERM_MAX_ARGS	15
#define ESC		27

#define ATTR_BOLD	0x01
#define ATTR_REVERSE	0x02
#define ATTR_BLINK	0x04
#define ATTR_UNDERLINE	0x08
#define ATTR_SELECTION	0x10
#define ATTR_16FG	0x20
#define ATTR_16BG	0x40

#define MODE_CUR_APL	   0x01
#define MODE_EDIT_BUTTON   0x02

enum {
    TERM_COLOR0, TERM_COLOR1, TERM_COLOR2, TERM_COLOR3,
    TERM_COLOR4, TERM_COLOR5, TERM_COLOR6, TERM_COLOR7,
    TERM_COLOR8, TERM_COLOR9, TERM_COLOR10, TERM_COLOR11,
    TERM_COLOR12, TERM_COLOR13, TERM_COLOR14, TERM_COLOR15,
    TERM_CURSOR_FG, TERM_CURSOR_BG,
    TERM_ISEL_FG,
    TERM_COLOR_END
};

typedef void (*KeyMapFuncType) (GtkWidget *, gpointer);

typedef struct {
    int keyVal;
    int state;
    gpointer data;
    KeyMapFuncType func;
} KeyMapType;

typedef struct _TermType TermType;
typedef struct _TermClassType TermClassType;

typedef int (*TermLocalWriteFuncType)(TermType *, const guchar *buf, int size);
typedef int (*TermRemoteWriteFuncType)(const guchar *buf, int size);

struct _TermType {
    GtkWidget widget;
    GtkAdjustment *adjustment;
    guint32 timer;

    GdkGC *gc;
    GdkGC *whiteGC;
    GdkGC *blackGC;
    GdkGC *bgGC;
    GdkGC *cursorGC;
    GdkGC *iSelGC;

    GdkColor **colorTable;
    guchar *buf;
    guchar *attr;
    guchar *color;
    guchar state;
    guchar currFG, oldFG;
    guchar currBG, oldBG;
    guchar currAttr, oldAttr;
    gint currX, oldX;
    gint currY, oldY;
    gint top, bot;
    gint col, row;
    gint maxReached;
    gint visibleLineBegin;
    gint realVisibleLineBegin;
    gint argc;
    gint argv[TERM_MAX_ARGS];

    GUDraw *gudraw;

    gint fontHeight;
    gint fontWidth;
    gint fontAscent;

    gboolean drawFlag;
    gboolean needSlideUpdate;	/* draw가 queuing되어 있는 경우 timer에게
				 * adjustment가 바뀌었음을 알려주는 flag */
    gboolean haveBgPixmap;	/* terminal 창의 배경 그림인가? */
    TermLocalWriteFuncType localWrite; /* message를 local 창에 출력할 함수 */
    TermRemoteWriteFuncType remoteWrite; /* message를 remote로 보낼 함수 */
    gboolean isRemoteUTF8;
    gboolean isLocalUtf8;
    char charset;
    int mode;
    guchar *tmpStr;
    int tmpAlloc, tmpLen;

    gboolean icReset;
    GtkIMContext *ic;
};

struct _TermClassType {
    GtkWidgetClass parentClass;
    void (*sizeChanged) (TermType *term);
    void (*lineChanged) (TermType *term);
    void (*received) (TermType *term);
    void (*setScrollAdjustments) (TermType *term, GtkAdjustment *hadj,
				  GtkAdjustment *vadj);
};

enum { ENTER_NONE, ENTER_CR, ENTER_LF, ENTER_CRLF };
extern guint32 EnterConvert;

extern TermType *Term;
extern char *TermFontName;
extern gint32 TermRow;
extern gint32 TermCol;
extern gint32 TermSavedLines;
extern gint32 TermNormFG;
extern gint32 TermNormBG;
extern gboolean TermSilent;
extern gboolean TermAutoLineFeed;
extern guint32 TermReadHack;
extern gboolean TermCanBlock;
extern gboolean TermUseBold;
extern gboolean TermUseUnder;
extern gboolean TermForce16Color;
extern gboolean MouseButtonSwap;
extern guint MouseLeftButtonNum, MouseRightButtonNum;
extern char *TermWordSeperator;

/* terminal input handler tag */
extern GIOChannel *TermInputChannel;
extern guint TermInputID;

extern GdkColor *TermColor0;
extern GdkColor *TermColor1;
extern GdkColor *TermColor2;
extern GdkColor *TermColor3;
extern GdkColor *TermColor4;
extern GdkColor *TermColor5;
extern GdkColor *TermColor6;
extern GdkColor *TermColor7;
extern GdkColor *TermColor8;
extern GdkColor *TermColor9;
extern GdkColor *TermColor10;
extern GdkColor *TermColor11;
extern GdkColor *TermColor12;
extern GdkColor *TermColor13;
extern GdkColor *TermColor14;
extern GdkColor *TermColor15;
extern GdkColor *TermCursorFG;
extern GdkColor *TermCursorBG;
extern GdkColor *TermISelFG;

/* Cursor Blink */
extern gboolean TermCursorBlink;
extern guint32 TermCursorOnTime;
extern guint32 TermCursorOffTime;

extern char *RemoteCharset;

#define TERM(w) ((TermType *) w)

#define TERM_LINENUM(l)	      (((l) + term->visibleLineBegin) % TermSavedLines)
#define CURR_BYTE_POS(term)   (term->buf \
			       + TERM_LINENUM(term->currY) * term->col \
			       + term->currX)
#define CURR_BYTE(term)	      (*(unsigned char *)CURR_BYTE_POS(term))
#define CURR_COLOR_POS(term)  (term->color \
			       + TERM_LINENUM(term->currY) * term->col \
			       + term->currX)
#define CURR_COLOR(term)      (*(unsigned char *)CURR_COLOR_POS(term))
#define CURR_ATTR_POS(term)   (term->attr \
			       + TERM_LINENUM(term->currY) * term->col \
			       + term->currX)
#define CURR_ATTR(term)	      (*(unsigned char *)CURR_ATTR_POS(term))
#define COLOR(fg, bg)	      (((fg) << 4) | (bg))

extern TermType *TermNew(void);
extern void TermSetSize(TermType *term, int newCol, int newRow);
extern void TermSizeFix(void);
extern void TermReset(TermType *term);
extern void TermClearScreen(TermType *term);

extern void TermLineChanged(TermType *term);
extern gboolean TermInputHandler(GIOChannel *channel, GIOCondition cond,
				 gpointer data);
extern int DummyFromTerm(const guchar *buf, int len);
extern void TermEmulModeSet(void);
extern void TermExit(void);

extern void TermConvOpen(TermType *__term, const char *__charset);
extern void TermConvClose(void);

/* i select */
extern void I_SelectInit(void);
extern void I_SelectExit(void);
extern void I_SelectBegin(TermType *term, gint x, gint y);
extern void I_SelectEnd(TermType *term, gint x, gint y);
extern void I_SelectMotion(TermType *term, gint x, gint y);

#endif /* __PCTERM_H */
