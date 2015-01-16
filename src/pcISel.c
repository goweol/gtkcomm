/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * 꾀돌이 마우스
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
#include <ctype.h>	/* isdigit() */

#include "pcMain.h"
#include "pcTerm.h"

/* Global variables/functions {{{1 */

gboolean UseISel;

/* 꾀돌이 마우스에서 URL 선택시 실행할 명령 */
char *ISel_HTTP_Command;
char *ISel_FTP_Command;
char *ISel_TELNET_Command;

/* I_SelPatternType {{{1 */
typedef struct {
    char *left;	/* mouse click한 위치로부터 왼쪽으로 찾을 문자열 */
    char *right;	/* mouse click한 위치로부터 오른쪽으로 찾을 문자열 */
    char *guard;	/* 왼쪽/오른쪽 관계없이 이걸 만나면 그 전까지만 search */
    char *field;	/* character 단위의 구분자. 여러개 사용가능하도록
			 * string 사용 */

#define I_SEL_LEFT_INC	   0x01
#define I_SEL_RIGHT_INC	   0x02
#define I_SEL_FORCE_USE	   0x04
    guchar include;
    void (*action) (char *str);	/* action. should not be NULL */
    void (*cancel) (void);	/* selection cancel. can be NULL */
    /* selection check. can be NULL */
    gboolean (*check) (char *buf, int left, int right);
} I_SelPatternType;

/* I_SelFuncTableType {{{1 */
typedef struct {
    gboolean (*search) (char *buf, int bufLen, int x);
    void (*action) (char *str);
    void (*cancel) (void);
    /* selection check. can be NULL */
    gboolean (*check) (char *buf, int left, int right);
} I_SelFuncTableType;

/* Local function prototypes {{{1 */

/* action */
static void I_SelSendString(char *str);
static void I_SelURL(char *str);

/* check */
static gboolean I_SelCheckNum(char *buf, int left, int right);
static gboolean I_SelCheckNonNum(char *buf, int left, int right);
static gboolean I_SelCheckAlphaNum(char *buf, int left, int right);
static gboolean I_SelCheckURL(char *buf, int left, int right);

/* search */
static gboolean I_SelBadukSearch(char *buf, int bufLen, int x);

/* Local variables {{{1 */

static I_SelFuncTableType PreSearchTable[] = {
    {I_SelBadukSearch, I_SelSendString, NULL, NULL},
    {NULL, NULL, NULL, NULL}
};
static I_SelFuncTableType PostSearchTable[] = {
    /* nothing yet */
    {NULL, NULL, NULL, NULL}
};

/* match문자열이 발견된 경우 문자열의 시작과 끝 위치 */
static gint VisualStart, VisualEnd;

/* mouse click이 발생되었을 때의 row, column 위치. match 문자열이
 * 발견되지 않은 상태에서 button release가 일어난 경우 mouse의 position이
 * 바뀌었는 지를 검사할 때 사용한다.
 */
static gint VisualRow, VisualCol;

static gboolean I_SelFocus;
static gboolean I_SelFound;
static char I_SelActStrBuf[256];	/* action 함수로 넘길 string */
static int LeftPos, RightPos;

/* isel pattern이 찾아진 경우 table의 action 및 cancel 함수 pointer 값을
 * 갖는다.
 */
static void (*CurrAction)(char *str);
static void (*CurrCancel)(void);

#define MAX_ISEL_PARAMS	8
static GSList *I_SelList;

/* I_SelectInit() {{{1 */
void
I_SelectInit(void)
{
    FILE *fp;
    char *fname, *buf, *param[MAX_ISEL_PARAMS];
    I_SelPatternType *isp;

    I_SelList = NULL;
    fname = ExpandPath(PC_ISEL_RC_FILE);
    fp = fopen(fname, "r");
    g_free(fname);
    if (fp != NULL)
    {
	while ((buf = GetLines(fp)) != NULL)
	{
	    if (ParseLine(buf, MAX_ISEL_PARAMS, param) < MAX_ISEL_PARAMS)
	    {
		g_warning(_("%s: parsing error.\nbuf=%s"), __FUNCTION__, buf);
		break;
	    }

	    isp = g_new(I_SelPatternType, 1);
	    if (!g_ascii_strcasecmp(param[0], "NULL") || !g_ascii_strcasecmp(param[0], "0"))
		isp->left = NULL;
	    else
		isp->left = ConvertCtrlChar(param[0]);
	    if (!g_ascii_strcasecmp(param[1], "NULL") || !g_ascii_strcasecmp(param[0], "0"))
		isp->right = NULL;
	    else
		isp->right = ConvertCtrlChar(param[1]);
	    if (!g_ascii_strcasecmp(param[2], "NULL") || !g_ascii_strcasecmp(param[0], "0"))
		isp->guard = NULL;
	    else
		isp->guard = ConvertCtrlChar(param[2]);
	    if (!g_ascii_strcasecmp(param[3], "NULL") || !g_ascii_strcasecmp(param[0], "0"))
		isp->field = NULL;
	    else
		isp->field = ConvertCtrlChar(param[3]);

	    isp->include = atoi(param[4]);

	    if (!g_ascii_strcasecmp(param[5], "URL"))
		isp->action = I_SelURL;
	    else if (!g_ascii_strcasecmp(param[5], "STRING"))
		isp->action = I_SelSendString;
	    else
	    {
		g_warning(_("%s: invalid param, action = %s"), __FUNCTION__,
			  param[5]);
		break;
	    }

	    if (!g_ascii_strcasecmp(param[6], "NULL"))
		isp->cancel = NULL;
	    else
	    {
		g_warning(_("%s: invalid param, cancel = %s"), __FUNCTION__,
			  param[6]);
		break;
	    }

	    if (!g_ascii_strcasecmp(param[7], "URL"))
		isp->check = I_SelCheckURL;
	    else if (!g_ascii_strcasecmp(param[7], "NUMBER"))
		isp->check = I_SelCheckNum;
	    else if (!g_ascii_strcasecmp(param[7], "NONNUM"))
		isp->check = I_SelCheckNonNum;
	    else if (!g_ascii_strcasecmp(param[7], "ALNUM"))
		isp->check = I_SelCheckAlphaNum;
	    else if (!g_ascii_strcasecmp(param[7], "NULL"))
		isp->check = NULL;
	    else
	    {
		g_warning(_("%s: invalid param, check = %s"), __FUNCTION__,
			  param[7]);
		break;
	    }

	    I_SelList = g_slist_append(I_SelList, isp);
	}
	fclose(fp);
    }
}

/* I_SelBadukSearch() {{{1 */
/* 어설프게 보이기는 하지만 하이텔, 천리안, 나우에서 마우스로
 * 바둑을 둘 수 있습니다. : 예술인 (98. 5. 16)
 *
 *    1 ┌┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┬┐  1
 *    4 ├●┼○┼┼┼┼┼＋┼┼●○┼○┼┼┤  4
 *   19 └┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┴┘ 19
 */
static gboolean
I_SelBadukSearch(char *buf, int bufLen, int x)
{
    int bx, by;
    guchar *s;

    if (sscanf(buf, "%d", &by) != 1 || by < 1 || by > 19)
	return FALSE;

    if (IsSecondMB(buf, x))
	s = (guchar *) (buf + x - 1);
    else
	s = (guchar *) (buf + x);

    if (!((*s == 166 && *(s + 1) >= 163 && *(s + 1) <= 171) ||
	  (*s == 163 && *(s + 1) == 171)))
	return FALSE;

    bx = (x - 3) / 2;

    if (bx < 0 || bx > 18)
	return FALSE;

    VisualStart = (int) ((char *) s - buf);
    VisualEnd = VisualStart + 1;

    g_snprintf(I_SelActStrBuf, sizeof(I_SelActStrBuf), "%d,%c", by, 'a' + bx);
    return TRUE;

    bufLen = 0;
}

/* I_SelURL() {{{1 */
static void
I_SelURL(char *str)
{
    char cmd[MAX_PATH];

    if (!strncmp(str, "http://", 7))
	g_snprintf(cmd, sizeof(cmd), "%s %s", ISel_HTTP_Command, str);
    else if (!strncmp(str, "ftp://", 6))
	g_snprintf(cmd, sizeof(cmd), "%s %s", ISel_FTP_Command, str);
    else if (!strncmp(str, "telnet://", 9))
	g_snprintf(cmd, sizeof(cmd), "%s %s", ISel_HTTP_Command, str);
    else
    {
	g_warning(_("%s: invalid URL %s"), __FUNCTION__, str);
	return;
    }

    if (fork() == 0)
    {
	setsid();
	execlp("sh", "sh", "-c", cmd, NULL);

	/* should never reach */
	g_warning(_("%s: fail to run '%s'"), __FUNCTION__, cmd);
	_exit(1);
    }
}

/* I_SelCheckURL() {{{1 */
/* URL을 check한다기 보다 오른쪽에 붙어있는 쓰레기들을 잘라내는
 * 역할을 수행한다.
 */
static gboolean
I_SelCheckURL(char *buf, int left, int right)
{
    char *str;
    static const char *validChars = ".:/@~-_#";

    /* 오! 이렇게 해야만 하나? */
    str = buf + left;
    for (; str <= buf + right; str++)
    {
	if (!isalnum(*str) && strchr(validChars, *str) == NULL)
	{
	    RightPos = str - buf - 1;
	    I_SelActStrBuf[RightPos - LeftPos] = '\0';
	    break;
	}
    }
    return TRUE;
}

/* I_SelSendString() {{{1 */
static void
I_SelSendString(char *str)
{
    int len;
    guchar c = '\r';
    guint32 currTime;
    static guint32 oldTime = 0;

    currTime = gdk_time_get();
    if (currTime < oldTime + 300)
    {
	oldTime = currTime;
	return;
    }
    oldTime = currTime;
    if (str && *str)
	Term->remoteWrite((guchar*) str, -1);
    Term->remoteWrite(&c, 1);
}

/* I_SelCheckNonNum() {{{1 */
/* 숫자가 아닌지를 확인 */
static gboolean
I_SelCheckNonNum(char *buf, int left, int right)
{
    for (; left <= right; left++)
	if (isdigit(buf[left]))
	    return FALSE;

    return TRUE;
}

/* I_SelCheckAlphaNum() {{{1 */
/* 숫자 혹은 알파벳으로 이루어진 문자열인지 확인 */
static gboolean
I_SelCheckAlphaNum(char *buf, int left, int right)
{
    for (; left <= right; left++)
	if (!isalnum(buf[left]))
	    return FALSE;

    return TRUE;
}

/* I_SelCheckNum() {{{1 */
/* 숫자로만 이루어진 문자열인지 확인 */
static gboolean
I_SelCheckNum(char *buf, int left, int right)
{
    for (; left <= right; left++)
	if (!isdigit(buf[left]))
	    return FALSE;

    return TRUE;
}

/* MousePos2TermPos() {{{1 */
/* mouse pointer position을 row/column으로 변환 */
static void
MousePos2TermPos(TermType *term, gint *x, gint *y)
{
    if (*x < 0)
	*x = 0;
    else
    {
	*x /= term->fontWidth;
	if (*x >= term->col)
	    *x = term->col - 1;
    }
    if (*y < 0)
	*y = 0;
    else
    {
	*y /= term->fontHeight;
	if (*y >= term->row)
	    *y = term->row - 1;
    }
}

/* I_SelShow() {{{1 */
static void
I_SelShow(TermType *term)
{
    char *s, *buf;
    int len;
    gsize bw;

    I_SelFocus = TRUE;
    gdk_gc_set_foreground(term->iSelGC, term->colorTable[TERM_ISEL_FG]);
    gdk_draw_rectangle(GTK_WIDGET(term)->window, term->iSelGC, TRUE,
		       VisualStart * term->fontWidth,
		       VisualRow * term->fontHeight,
		       (VisualEnd - VisualStart + 1) * term->fontWidth,
		       term->fontHeight);
    gdk_gc_set_foreground(term->iSelGC, &GTK_WIDGET(term)->style->black);

    buf = (char*) term->buf + TERM_LINENUM(VisualRow) * term->col + VisualStart;
    len = VisualEnd - VisualStart + 1;

    if (gu_euckr_validate_conv(buf, len, &s, &bw) == TRUE)
    {
	gu_draw_string(term->gudraw, GTK_WIDGET(term)->window, term->iSelGC,
		       VisualStart * term->fontWidth,
		       VisualRow * term->fontHeight + term->fontAscent,
		       (guchar*) s, bw);
	g_free(s);
    }
}

/* I_SelHide() {{{1 */
static void
I_SelHide(TermType *term)
{
    I_SelFocus = FALSE;
    gtk_widget_queue_draw_area(GTK_WIDGET(term), VisualStart * term->fontWidth,
			       VisualRow * term->fontHeight,
			       (VisualEnd - VisualStart + 1) * term->fontWidth,
			       term->fontHeight);
}

/* I_SelFindMatchPattern() {{{1 */
static int
I_SelFindMatchPattern(char *buf, int bufLen, gint x)
{
    register int j, k, len, fieldPos;
    guint prevShortDist;
    guint shortLeftPos = 0, shortRightPos = 0;
    char *p;
    I_SelPatternType *tbl, *currTbl;
    GSList *slist;

    LeftPos = RightPos = 0;
    currTbl = NULL;
    prevShortDist = bufLen;
    for (slist = I_SelList; slist; slist = slist->next)
    {
	tbl = slist->data;
	p = buf;

	LeftPos = 0;
	if (tbl->left && tbl->left[0])
	{
	    len = strlen(tbl->left);
	    fieldPos = -1;
	    for (j = x; j >= 0; j--)
	    {
		for (k = len - 1; k >= 0; k--)
		{
		    /* (H,DD,DN) 의 경우 field는 comma, left는 (, right는 )가
		     * 될수있다. field가 찾아진 위치를 저장하고, left가 있는지를
		     * 계속 찾는다. left가 찾아지면 fieldPos를 LeftPos로...
		     */
		    if (fieldPos < 0 && tbl->field
			&& strchr(tbl->field, buf[j + k]))
			fieldPos = j;
		    if (buf[j + k] != tbl->left[k])
			break;
		    if (j + k <= 0)
			goto next;
		}
		if (k < 0)
		{
		    if (tbl->include & I_SEL_LEFT_INC)
			LeftPos = j;
		    else
		    {
			if (fieldPos > 0)
			    j = fieldPos;
			LeftPos = j + len;
		    }
		    goto rightFind;
		}
	    }
	    goto next;
	}

rightFind:
	while (isspace(buf[LeftPos]))
	    LeftPos++;

	RightPos = bufLen - 1;
	if (tbl->right && tbl->right[0])
	{
	    len = strlen(tbl->right);
	    fieldPos = -1;
	    for (j = LeftPos + 1; j < bufLen - len - 1; j++)
	    {
		for (k = 0; k < len; k++)
		{
		    if (fieldPos < 0 && tbl->field
			&& strchr(tbl->field, buf[j + k]))
			fieldPos = j;
		    if (buf[j + k] != tbl->right[k])
			break;
		    if (j + k >= bufLen - 1)
			goto next;
		}
		if (k == len)
		{
		    if (tbl->include & I_SEL_RIGHT_INC)
			RightPos = j + len - 1;
		    else
		    {
			if (fieldPos > 0)
			    j = fieldPos;
			RightPos = j - 1;
		    }
		    goto guardFind;
		}
	    }
	    goto next;
	}

guardFind:
	if (tbl->guard && tbl->guard[0])
	{
	    len = strlen(tbl->guard);
	    if (LeftPos < x)
	    {
		for (j = LeftPos; j < x; j++)
		    if (!strncmp(tbl->guard, buf + j, len))
			LeftPos = j + len;
	    }
	    if (RightPos > LeftPos)
	    {
		for (j = LeftPos + 1; j < RightPos - len; j++)
		    if (!strncmp(tbl->guard, buf + j, len))
			RightPos = j - 1;
	    }
	}
	/* 모든 table을 search하여 가장 가까운 데서 match난 것을 찾는다. */
	if ((guint) (x - LeftPos) < prevShortDist
	    || (tbl->include & I_SEL_FORCE_USE))
	{
	    if (tbl->check)
		if (tbl->check(buf, LeftPos, RightPos) == FALSE)
		    continue;

	    len = RightPos - LeftPos + 1;
	    if ((guint) len < sizeof(I_SelActStrBuf) - 1)
	    {
		memcpy(I_SelActStrBuf, buf + LeftPos, len);
		I_SelActStrBuf[len] = '\0';

		if ((p = FindWhiteSpace(I_SelActStrBuf)) != NULL)
		{
		    if (tbl->right)
			/* right 앞에 공백이 있으면 잘못찾은 것으로 간주 */
			continue;
		    RightPos -= I_SelActStrBuf + len - p;
		    *p = '\0';
		}

		currTbl = tbl;
		shortLeftPos = LeftPos;
		shortRightPos = RightPos;
		prevShortDist = x - LeftPos;

		OptPrintf(OPRT_ISEL, "act=%s, left=%d, right=%d\n",
			  I_SelActStrBuf, shortLeftPos, shortRightPos);

		if (!prevShortDist || (tbl->include & I_SEL_FORCE_USE))
		    break;
	    }
	}
next:
	continue;
    }

    if (currTbl)
    {
	VisualStart = shortLeftPos;
	VisualEnd = shortRightPos;
	CurrAction = currTbl->action;
	CurrCancel = currTbl->cancel;

	OptPrintf(OPRT_ISEL, "act=%s, start=%d, end=%d\n",
		  I_SelActStrBuf, VisualStart, VisualEnd);

	return TRUE;
    }

    return FALSE;
}

/* I_SelFindMatchFunc() {{{1 */
static gboolean
I_SelFindMatchFunc(I_SelFuncTableType * tbl, char *buf, int bufLen, int x)
{
    int len;

    g_return_val_if_fail(tbl != NULL, FALSE);

    for (; tbl->search; tbl++)
    {
	I_SelActStrBuf[0] = '\0';
	if (tbl->search(buf, bufLen, x) == TRUE)
	{
	    len = VisualEnd - VisualStart + 1;
	    if ((guint) len < sizeof(I_SelActStrBuf) - 1)
	    {
		if (I_SelActStrBuf[0] == '\0')
		{
		    memcpy(I_SelActStrBuf, buf + VisualStart, len);
		    I_SelActStrBuf[len] = '\0';
		}

		if (tbl->check)
		    if (tbl->check(buf, VisualStart, VisualEnd) == FALSE)
			continue;

		CurrAction = tbl->action;
		CurrCancel = tbl->cancel;
		return TRUE;
	    }
	    else
	    {
		g_warning("%s: BUG?. VS=%d, VE=%d", __FUNCTION__,
			  VisualStart, VisualEnd);
		break;
	    }
	}
    }
    return FALSE;
}

/* I_SelectBegin() {{{1 */
void
I_SelectBegin(TermType *term, gint x, gint y)
{
    register int ret;
    char *buf;
    int bufLen;

    if (!UseISel)
	return;

    MousePos2TermPos(term, &x, &y);
    VisualCol = x;
    VisualRow = y;

    ret = FALSE;
    I_SelActStrBuf[0] = '\0';
    CurrAction = (void (*)(char *)) NULL;
    CurrCancel = (void (*)(void)) NULL;

    buf = (char*) term->buf + TERM_LINENUM(y) * term->col;
    bufLen = term->col;

    if (x >= bufLen)
    {
	g_warning("%s: BUG! x=%d, bufLen=%d", __FUNCTION__, x, bufLen);
	return;
    }

    if (I_SelFindMatchFunc(PreSearchTable, buf, bufLen, x) == FALSE)
    {
	if (I_SelFindMatchPattern(buf, bufLen, x) == FALSE)
	{
	    if (I_SelFindMatchFunc(PostSearchTable, buf, bufLen, x) == FALSE)
	    {
		I_SelFound = FALSE;
		return;
	    }
	}
    }

    I_SelFound = TRUE;
    I_SelShow(term);
}

/* I_SelectEnd() {{{1 */
void
I_SelectEnd(TermType *term, gint x, gint y)
{
    if (!UseISel)
	return;

    if (!I_SelFound)
    {
	MousePos2TermPos(term, &x, &y);
	/* 마우스가 움직이지 않고 click한 경우에만 \r을 보냄 */
	if (VisualCol == x && VisualRow == y)
	{
	    I_SelSendString(NULL);	/* just send a \r */
	    StatusShowMessage(" ");
	}
	return;
    }

    if (I_SelFocus)
    {
	I_SelHide(term);

	if (CurrAction)
	{
	    CurrAction(I_SelActStrBuf);
	    StatusShowMessage(_("Did action for isel pattern '%s'."),
			      I_SelActStrBuf);
	}
    }
    else
    {
	if (CurrCancel)
	    CurrCancel();
    }
}

/* I_SelectMotion() {{{1 */
void
I_SelectMotion(TermType *term, gint x, gint y)
{
    if (!UseISel || !I_SelFound)
	return;

    MousePos2TermPos(term, &x, &y);

    if (I_SelFocus)
    {
	if (VisualRow != y || VisualStart > x || VisualEnd < x)
	    I_SelHide(term);
    }
    else
    {
	if (VisualRow == y && VisualStart <= x && VisualEnd >= x)
	    I_SelShow(term);
    }
}

/* I_SelectExit() {{{1 */
void
I_SelectExit(void)
{
    if (I_SelList)
    {
	GSList *l;
	I_SelPatternType *isp;

	for (l = I_SelList; l; l = l->next)
	{
	    isp = (I_SelPatternType *) l->data;
	    if (isp->left)
		g_free(isp->left);
	    if (isp->right)
		g_free(isp->right);
	    if (isp->guard)
		g_free(isp->guard);
	    if (isp->field)
		g_free(isp->field);
	    g_free(isp);
	}
	g_slist_free(I_SelList);
	I_SelList = NULL;
    }
}

