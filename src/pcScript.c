/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Script
 *
 * Copyright (C) 2000-2005, Nam SungHyun and various contributors
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
#include <ctype.h>	/* tolower(), isdigit() */
#include <sys/stat.h>
#include <glob.h>	/* glob() */
#include <fcntl.h>	/* X_OK */
#include "pcMain.h"
#include "pcTerm.h"

/* definitions & declarations {{{1 */

/* 혼잣말 스크립트가 실행중인가? */
gboolean ScriptRunning = FALSE;

char *ScriptPath;
char *ScriptAnimFile;

/* script enum {{{1 */
enum {
    SCR_NONE,		/* nothing */
    SCR_ASSIGN, 	/* assign은 특별하게 처리 */
    SCR_AUTORES,	/* auto response file 포함 */
    SCR_BEEP,		/* 삑 소리 냄 */
    SCR_BOOKMARK,	/* bookmark menu에 추가 */
    SCR_BUTTON,		/* control-bar에 button 추가 */
    SCR_CAPTURE,	/* capture on or off */
    SCR_CLEAR,		/* 화면 clear */
    SCR_CTLMENU,	/* control-bar에 menu 추가 */
    SCR_DISCONNECT,	/* 연결 끊기 */
    SCR_DOWNLOAD,	/* file download */
    SCR_ELSE,		/* else */
    SCR_ELSEIF,		/* elseif/elif */
    SCR_EMULATE,	/* terminal emulation mode change */
    SCR_END,		/* script end */
    SCR_ENDIF,		/* if 조건문 end */
    SCR_ENDWIN,		/* script end window */
    SCR_GOTO,		/* goto label */
    SCR_GOTO_TAG,	/* goto tag */
    SCR_IF,		/* if 조건문 */
    SCR_INCLUDE,	/* include another script file */
    SCR_KFLUSH,		/* key buffer flush */
    SCR_LABEL,		/* control-bar에 label 추가 */
    SCR_MESSAGE,	/* 화면에 메시지 출력 */
    SCR_RUN,		/* external command execute */
    SCR_PAUSE,		/* pause seconds */
    SCR_RFLUSH,		/* rx buffer flush */
    SCR_STSMSG,		/* status bar에 메시지 출력 */
    SCR_TODEC,		/* hex-decimal to decimal */
    SCR_TOHEX,		/* decimal to hexa-decimal */
    SCR_TRANSMIT,	/* transmit string */
    SCR_UPLOAD,		/* file upload */
    SCR_WAITFOR,	/* wait string */

    /* set */
    SCR_SET,		/* general parameter setting */
    SCR_SET_BAUDRATE,	/* modem baudrate change */
    SCR_SET_CONFIG,	/* config variable change */
    SCR_SET_DEBUG,	/* internal debugging option */
    SCR_SET_PROTOCOL,	/* transfer protocol change */
    SCR_SET_TITLE,	/* 창 제목 설정 */

    /* get */
    SCR_GET,
    SCR_GET_BAUDRATE,	/* baudrate get */
    SCR_GET_FILEINFO,	/* file info get */

    SCR_WIN,		/* 창 */
    SCR_WIN_WINDOW,	/* 창 생성 */
    SCR_WIN_HBOX,	/* 수평 상자 */
    SCR_WIN_VBOX,	/* 수직 상자 */
    SCR_WIN_FRAME,	/* 프레임 */
    SCR_WIN_LABEL,	/* 라벨 */
    SCR_WIN_ENTRY,	/* 입력 창 */
    SCR_WIN_BUTTON,	/* 단추 */
    SCR_WIN_HSEP,	/* 수평선 */
    SCR_WIN_END,	/* 창 생성 끝 (script 계속) */
    SCR_WIN_WAIT,	/* 창 생성 끝 (창이 닫힐 때까지 script 멈춤) */

    MAX_SCRIPT_TYPE
};

/* ScriptTableType {{{1 */
typedef struct _ScriptTableType ScriptTableType;

struct _ScriptTableType {
    int type;
    char *token;
    int argc;
    const ScriptTableType *next;
};

/* table의 argc들보다 1 이상 커야한다. */
#define MAX_SCRIPT_PARAMS  6

/* NOTE: argc field는 token이 가져야만 될 최소 파라미터 갯수이다.
 * 이것은 script parser로 하여금 해당 token에 대해 사용자가 script를 제대로
 * 작성하였는 지를 검사가 가능하도록 해준다.
 * 예를 들어, 'win begin'은 parameter가 없어도 되고, <title> parameter가
 * 있어도 되는 경우가 이에 해당되는 데, 이때는 argc field는 0을 써주어야 한다.
 * script parser는 MAX_SCRIPT_PARAMS까지는 전부 ScriptType의 argc 및 argv[]에
 * 저장해 주므로, 나중에 <title>이 있는 지를 검사할 수 있다.
 */

/* ScriptSetTable {{{1 */
static const ScriptTableType ScriptSetTable[] = {
    {SCR_SET_BAUDRATE, "baudrate", 1, NULL},	/* set baudrate 19200 */
    {SCR_SET_CONFIG, "config", 2, NULL},	/* set config var value */
    {SCR_SET_DEBUG, "debug", 1, NULL},	/* set debug option# */
    {SCR_SET_PROTOCOL, "protocol", 1, NULL},	/* set protocol [ascii/raw-ascii/kermit/xmodem/ymodem/zmodem] */
    {SCR_SET_TITLE, "title", 1, NULL},	/* set title "string" */
    {-1, NULL, -1, NULL}	/* end of table */
};

/* ScriptGetTable {{{1 */
static const ScriptTableType ScriptGetTable[] = {
    {SCR_GET_BAUDRATE, "baudrate", 1, NULL},	/* get baudrate $var */
    {SCR_GET_FILEINFO, "file", 3, NULL }, /* get file filename size $var */
    {-1, NULL, -1, NULL}	/* end of table */
};

/* ScriptWinTable {{{1 */
static const ScriptTableType ScriptWinTable[] = {
    {SCR_WIN_WINDOW, "begin", 0, NULL},	/* win begin <title> */
    {SCR_WIN_HBOX, "hbox", 3, NULL},	/* win hbox w name 3 */
    {SCR_WIN_VBOX, "vbox", 3, NULL},	/* win vbox w name 5 */
    {SCR_WIN_FRAME, "frame", 3, NULL }, /* win frame w name title */
    {SCR_WIN_LABEL, "label", 2, NULL }, /* win label w name */
    {SCR_WIN_ENTRY, "entry", 2, NULL},	/* win entry w $STRTAG <act button-title> */
    {SCR_WIN_BUTTON, "button", 4, NULL},	/* win button w title [string/script/sfile] "string-script-file" */
    {SCR_WIN_HSEP, "hsep", 1, NULL},	/* win hsep w */
    {SCR_WIN_END, "end", 0, NULL},	/* win end */
    {SCR_WIN_WAIT, "wait", 0, NULL},	/* win wait */
    {-1, NULL, -1, NULL}	/* end of table */
};

/* ScriptMainTable {{{1 */
static const ScriptTableType ScriptMainTable[] = {
    /* assign S? "string"   (? = number).
     * assign은 label과 마찬가지로 특별하게 처리한다.
     */
    {SCR_ASSIGN, "assign", 2, NULL },
    {SCR_AUTORES, "autores", 4, NULL},	/* autores add "find" [string|script] "string" */
    {SCR_BEEP, "beep", 0, NULL},	/* beep <intensity> */
    {SCR_BOOKMARK, "bookmark", 4, NULL},	/* bookmark [add/gadd] "name" [string|script] "string" */
    {SCR_BUTTON, "button", 4, NULL},	/* button [add/gadd] "name" [string|script] "string" */
    {SCR_CAPTURE, "capture", 1, NULL},	/* capture [on/off] <filename> <includebuf> */
    {SCR_CLEAR, "clear", 0, NULL},	/* clear */
    {SCR_CTLMENU, "ctlmenu", 5, NULL},	/* ctlmenu [add/gadd] title name [string/script] "string-action" */
    {SCR_DISCONNECT, "disconnect", 0, NULL},	/* disconnect */
    {SCR_DOWNLOAD, "receivefile", 1, NULL},	/* receivefile xmodem filename */
    {SCR_ELSE, "else", 0, NULL},	/* else */
    {SCR_ELSEIF, "elseif", 3, NULL},	/* elseif $var == "val" */
    {SCR_ELSEIF, "elif", 3, NULL},	/* elif $var == "val" */
    {SCR_EMULATE, "emulate", 1, NULL},	/* emulate vt100/ansi/.. */
    {SCR_END, "end", 0, NULL},	/* end */
    {SCR_ENDIF, "fi", 0, NULL},	/* fi */
    {SCR_ENDIF, "endif", 0, NULL},	/* endif */
    {SCR_ENDWIN, "endwin", 0, NULL},	/* end window */
    {SCR_GOTO, "goto", 1, NULL},	/* goto label */
    /* SCR_GOTO_TAG을 특별한 token 없다. xxxx:과 같이 :으로 끝나면 tag로 간주 */
    {SCR_IF, "if", 3, NULL},	/* if $? == 0 */
    {SCR_INCLUDE, "include", 1, NULL},	/* include "script-file" */
    {SCR_KFLUSH, "kflush", 0, NULL},	/* kflush */
    {SCR_LABEL, "label", 2, NULL}, /* label [add/gadd/rmv] [baudrate/protocol/emulate/linestatus] */
    {SCR_MESSAGE, "message", 1, NULL},	/* message "string" */
    {SCR_RUN, "run", 1, NULL},	/* run 'filename' */
    {SCR_SET, "set", 1, ScriptSetTable},	/* set ??? */
    {SCR_GET, "get", 1, ScriptGetTable}, 	/* get ??? */
    {SCR_PAUSE, "pause", 1, NULL},	/* pause secs */
    {SCR_RFLUSH, "rflush", 0, NULL},	/* rflush */
    {SCR_STSMSG, "stsmsg", 1, NULL},	/* stsmsg "string" */
    {SCR_TODEC, "todec", 2, NULL}, /* todec $hexnum decnum */
    {SCR_TOHEX, "tohex", 2, NULL}, /* tohex $decnum hexnum */
    {SCR_TRANSMIT, "transmit", 1, NULL},	/* transmit "string" */
    {SCR_UPLOAD, "sendfile", 2, NULL},	/* sendfile zmodem filename */
    {SCR_WAITFOR, "waitfor", 1, NULL},	/* waitfor "string" <timeout> <"str2"> */
    {SCR_WIN, "win", 1, ScriptWinTable}, /* win ??? */
    {-1, NULL, -1, NULL}	/* end of table */
};

/* ScriptType {{{1 */
/* ScriptBookmarkType/ButtonRemoveType등의 type */
#define SB_SCRIPT	   0x01	/* script 문자열? 이 bit가 0이면 string */
#define SB_SCRIPTFILE	   0x02	/* script file? */
#define SB_BRANCH	   0x02	/* bookmark의 경우 branch인지 아닌지.. */
#define SB_GLOBAL	   0x04	/* disconnect시 remove or not */

typedef struct {
    int type;
    int argc;
    gpointer *argv;
    gpointer *savedArgv;
} ScriptType;

/* AssignType {{{1 */
typedef struct {
    char *var;
    char *val;
} AssignType;

/* Local variables {{{1 */

/* script list는 ScriptBox를 구현하기 위해 gtk_combo_set_popdown_strings()
 * 함수를 이용하는 데, GList이어야 한다.
 */
static GList *CurrScriptItem = NULL;	/* 현재 실행중인 script item */
static GList *ScriptItemList = NULL;	/* 전체 script item list */

/* Script 실행이 잠시 중단된 경우 CurrScriptItem 값을 갖는다. */
static GList *ScriptWinSavedList = NULL;

static char *CurrScriptFile = NULL;
static GSList *AssignList = NULL;
static gint ScriptResult = 0;
static gboolean WaitForPending = FALSE;
static guint WaitForTimeoutTag = 0;
static char *WaitForString = NULL;
static char *WaitForString2 = NULL;
static guint IdleTag = 0;
static guint PauseTag = 0;
static gboolean ScriptCaptureActive = FALSE;

static char *ScriptChildStr = NULL;

static GSList *ScriptLabelList = NULL;
static gboolean ScriptLabelRemoved = TRUE;

/* ScriptWinObjType {{{1 */
typedef struct {
    char *str;
    char *env;
    int type;
} ScriptWinObjType;

static GSList *ScriptWinObjList = NULL;

/* 'win' 명령에 의해 생성된 창 */
static GtkWidget *ScriptWin = NULL;

/* ScriptBookmarkType {{{1 */
typedef struct {
    char *id;
    int type;
    char *str;
} ScriptBookmarkType;

static GSList *BookmarkList = NULL;
static gboolean BookmarkRemoved = TRUE;

/* ScriptButtonType {{{1 */
typedef struct {
    char *id;
    int type;
    char *str;
} ScriptButtonType;

static GSList *ButtonList = NULL;
static gboolean ButtonRemoved = TRUE;

/* script execute command */
static int ScriptRunFD[2];
static GIOChannel *ScriptRunInputChannel = NULL;
static guint ScriptRunInputID;

/* Local function prototypes {{{1 */

static GList *ScriptMakeListFromFile(char *filename);
static int ScriptRunNext(void);
static void ScriptRunAnimStart(void);
static void ScriptRunAnimStop(void);
static AssignType *GetAssign(const char *s);
static int WaitForInputFilter(char *buf, int len);
static int AddAssign(char *var, char *val);

static void ScriptRemoveLabel(void);
static void ScriptRemoveBookmark(void);
static void ScriptRemoveButton(void);
static void ScriptDestroyWinObjList(void);

/* ScriptDestroy() {{{1 */
static void
ScriptDestroy(void)
{
    register int i;

    CurrScriptItem = NULL;
    ScriptWinSavedList = NULL;

    if (ScriptItemList)
    {
	GList *list;
	ScriptType *script;

	/* destroy script list */
	for (list = ScriptItemList; list; list = list->next)
	{
	    script = list->data;
	    if (script->argc > 0)
	    {
		for (i = 0; i < script->argc; i++)
		    g_free(script->argv[i]);
		g_free(script->argv);
		if (script->savedArgv)
		{
		    for (i = 0; i < script->argc; i++)
			if (script->savedArgv[i])
			    g_free(script->savedArgv[i]);
		    g_free(script->savedArgv);
		}
	    }
	    g_free(script);
	}
	g_list_free(ScriptItemList);
	ScriptItemList = NULL;
    }

    if (CurrScriptFile)
    {
	g_free(CurrScriptFile);
	CurrScriptFile = NULL;
    }

    /* destroy assign list */
    if (AssignList)
    {
	GSList *slist;
	AssignType *assign;

	for (slist = AssignList; slist; slist = slist->next)
	{
	    assign = slist->data;
	    g_free(assign->var);
	    g_free(assign->val);
	}
	g_slist_free(AssignList);
	AssignList = NULL;
    }
}

/* ScriptRemoveAll() {{{1 */
static void
ScriptRemoveAll(void)
{
    if (WaitForPending)
    {
	WaitForPending = FALSE;
	InputFilterList = g_slist_remove(InputFilterList, WaitForInputFilter);
    }
    if (WaitForTimeoutTag > 0)
    {
	gtk_timeout_remove(WaitForTimeoutTag);
	WaitForTimeoutTag = 0;
    }
    if (IdleTag > 0)
    {
	gtk_timeout_remove(IdleTag);
	IdleTag = 0;
    }
    if (PauseTag > 0)
    {
	gtk_timeout_remove(PauseTag);
	PauseTag = 0;
    }
    if (ScriptCaptureActive)
    {
	ScriptCaptureActive = FALSE;
	CaptureFinish();
    }
    ScriptDestroy();
    ScriptRunAnimStop();
}

/* ScriptFinish() {{{1 */
static void
ScriptFinish(void)
{
    if (ScriptRunning == FALSE)
	return;
    ScriptRunning = FALSE;

    if (CurrScriptFile)
	StatusShowMessage(_("Script '%s' finished."), CurrScriptFile);
    ScriptRemoveAll();
    ScriptResult = 0;
}

/* ScriptExit() {{{1 */
void
ScriptExit(void)
{
    ScriptFinish();
    ScriptRemoveLabel();
    ScriptRemoveBookmark();
    ScriptRemoveButton();
    ScriptDestroyWinObjList();
}

/* ScriptInstallNextItem() {{{1 */
static void
ScriptInstallNextItem(void)
{
    if (TRxRunning)
	PC_Sleep(1);
    if (CurrScriptItem == ScriptWinSavedList)
	/* will install after window destroyed */
	return;
    IdleTag = gtk_timeout_add(0, (GtkFunction) ScriptRunNext, NULL);
}

/* WaitForInputFilter() {{{1 */
static int
WaitForInputFilter(char *buf, int len)
{
    register int i;
    static char *p, *p2;

    if (len < 0)
    {
	p = WaitForString;
	p2 = WaitForString2;
	return 0;
    }

    for (i = 0; i < len; i++)
    {
	if (buf[i] != *p)
	{
	    p = WaitForString;
	    if (WaitForString2 && buf[i] != *p2)
		p2 = WaitForString2;
	    else
		p2++;

	}
	else
	    p++;
	
	if (*p == '\0' || (WaitForString2 && *WaitForString2 && *p2 == '\0'))
	{
	    /* match string */
	    StatusShowMessage(" ");	/* clear 'Waitfor...' if exists */
	    InputFilterList =
		g_slist_remove(InputFilterList, WaitForInputFilter);
	    WaitForPending = FALSE;
	    if (WaitForTimeoutTag > 0)
	    {
		gtk_timeout_remove(WaitForTimeoutTag);
		WaitForTimeoutTag = 0;
	    }

	    if (*p == '\0')
		ScriptResult = 0;
	    else
		ScriptResult = 1;
	    if (CurrScriptItem && CurrScriptItem->next)
	    {
		CurrScriptItem = CurrScriptItem->next;
		ScriptInstallNextItem();
	    }
	    else
		ScriptFinish();
	}
    }
    return len;
}

/* WaitForTimer() {{{1 */
static int
WaitForTimer(gpointer data)
{
    UNUSED(data);

    WaitForPending = FALSE;
    InputFilterList = g_slist_remove(InputFilterList, WaitForInputFilter);

    ScriptResult = -1;
    WaitForTimeoutTag = 0;
    if (CurrScriptItem && CurrScriptItem->next)
    {
	CurrScriptItem = CurrScriptItem->next;
	ScriptInstallNextItem();
    }
    else
	ScriptFinish();
    StatusShowMessage(_("waitfor timeout!"));
    return FALSE;
}

/* ScriptWaitFor() {{{1 */
static void
ScriptWaitFor(ScriptType *script, float seconds)
{
    if (WaitForString)
	g_free(WaitForString);
    WaitForString = ConvertCtrlChar((char *) script->argv[0]);

    if (WaitForString2)
    {
	g_free(WaitForString2);
	WaitForString2 = NULL;
    }
    if (script->argc >= 3)
	WaitForString2 = ConvertCtrlChar((char *) script->argv[2]);

    WaitForInputFilter(NULL, -1);	/* initialize the static string ptr */

    WaitForPending = TRUE;

    InputFilterList = g_slist_append(InputFilterList, WaitForInputFilter);

    if ((long) (seconds * 10.0f) >= 1L)
    {
	StatusShowMessage(_("Waiting '%s' (%.1f secs)..."), WaitForString,
			  seconds);
	WaitForTimeoutTag = gtk_timeout_add((guint32) (seconds * 1000UL),
					    WaitForTimer, NULL);
    }
    else
    {
	StatusShowMessage(_("Waiting '%s' (endless)..."), WaitForString);
    }
}

/* ScriptGetBaudrate() {{{1 */
static gint
ScriptGetBaudrate(ScriptType *s)
{
    guint32 b = ModemGetSpeed();
    char buf[20];

    g_snprintf(buf, sizeof(buf), "%lu", (unsigned long) b);
    return AddAssign(s->argv[0], buf);
}

/* ScriptGetFileInfo() {{{1 */
static gint
ScriptGetFileInfo(ScriptType *s)
{
    gint sts = -1;
    struct stat t;
    char *fname;
    char *info = (char *) s->argv[1];
    char *vars = (char *) s->argv[2];
    char buf[32];

    if ((fname = GetRealFilename((char *) s->argv[0], UploadPath)) == NULL)
    {
	g_warning(_("Cannot access '%s'."), (char *) s->argv[0]);
	return -1;
    }

    if (stat(fname, &t))
	g_warning(_("Cannot stat '%s'."), fname);
    else
    {
	if (!g_ascii_strcasecmp(info, "size"))
	{
	    g_snprintf(buf, sizeof(buf), "%lu", (unsigned long) t.st_size);
	    sts = AddAssign(vars, buf);
	}
	else
	    g_warning(_("Unknown info name '%s'."), info);
    }

    g_free(fname);
    return sts;
}

/* ScriptGoto() {{{1 */
static gint
ScriptGoto(char *label)
{
    GList *list;
    ScriptType *script;

    for (list = ScriptItemList; list; list = list->next)
    {
	script = list->data;
	if (script->type == SCR_GOTO_TAG)
	{
	    if (!g_ascii_strcasecmp(script->argv[0], label))
	    {
		CurrScriptItem = list;
		return 0;
	    }
	}
    }

    return -1;
}

/* ScriptRunBookmark() {{{1 */
static void
ScriptRunBookmark(ScriptBookmarkType * sb)
{
    if (sb->type & SB_SCRIPT)
	ScriptRunFromString(sb->str);
    else if (sb->type & SB_SCRIPTFILE)
	ScriptRunFromFile(sb->str);
    else
	Term->remoteWrite((guchar*) sb->str, -1);
}

/* ScriptRunButton() {{{1 */
static void
ScriptRunButton(GtkWidget *w, ScriptButtonType * sb)
{
    if (sb->type & SB_SCRIPT)
	ScriptRunFromString(sb->str);
    else if (sb->type & SB_SCRIPTFILE)
	ScriptRunFromFile(sb->str);
    else
	Term->remoteWrite((guchar*) sb->str, -1);

    SoundPlay(SoundClickFile);

    w = NULL;	/* to make gcc happy */
}

/* ScriptRemoveBookmark() {{{1 */
static void
ScriptRemoveBookmark(void)
{
    GSList *slist;
    ScriptBookmarkType *sb;
    GtkWidget *w;
    gboolean globalFound;

    if (BookmarkList && !BookmarkRemoved)
    {
	BookmarkRemoved = TRUE;
	DisconnectFuncList = g_slist_remove(DisconnectFuncList,
					    ScriptRemoveBookmark);

	/* 일단 entry들을 없애고, branch가 있는 경우 branch를 없앤다.
	 * 모든 entry가 branch 밑에 있는 경우에는 branch만 없애도 자동으로 sub
	 * menu들까지 없어지지만 그렇지 않은 경우를 위해서 item들을 먼저
	 * 지워주는 것임.
	 */
	for (slist = BookmarkList; slist; slist = slist->next)
	{
	    sb = slist->data;
	    if ((sb->type & (SB_BRANCH | SB_GLOBAL)) == 0)
		/* global도 아니고 branch도 아니면 */
		gtk_item_factory_delete_item(MenuFactory, sb->id);
	}
	globalFound = FALSE;
	for (slist = BookmarkList; slist;)
	{
	    sb = slist->data;

	    /* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다. */
	    slist = slist->next;

	    if (sb->type & SB_GLOBAL)
		globalFound = TRUE;
	    else
	    {
		if (sb->type & SB_BRANCH)
		{
		    /* global이 아니고 branch이면 */
		    w = gtk_item_factory_get_widget(MenuFactory, sb->id);
		    w = gtk_widget_get_ancestor(w, GTK_TYPE_MENU);
		    w = gtk_menu_get_attach_widget(GTK_MENU(w));
		    if (w)
			gtk_widget_destroy(w);
		    else
			g_warning("%s: w = nil", __FUNCTION__);
		}
		g_free(sb->id);
		g_free(sb->str);
		g_free(sb);

		BookmarkList = g_slist_remove(BookmarkList, sb);
	    }
	}
	if (!globalFound)
	{
	    g_slist_free(BookmarkList);
	    BookmarkList = NULL;
	}
    }
}

/* ScriptRemoveButton() {{{1 */
static void
ScriptRemoveButton(void)
{
    GSList *slist;
    ScriptButtonType *sb;
    gboolean globalFound;

    if (ButtonList && !ButtonRemoved)
    {
	ButtonRemoved = TRUE;
	DisconnectFuncList = g_slist_remove(DisconnectFuncList,
					    ScriptRemoveButton);

	globalFound = FALSE;
	for (slist = ButtonList; slist;)
	{
	    sb = slist->data;

	    /* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다. */
	    slist = slist->next;

	    if (sb->type & SB_GLOBAL)
		globalFound = TRUE;
	    else
	    {
		gtk_widget_destroy(GTK_WIDGET(sb->id));
		g_free(sb->str);
		g_free(sb);

		ButtonList = g_slist_remove(ButtonList, sb);
	    }
	}
	if (!globalFound)
	{
	    g_slist_free(ButtonList);
	    ButtonList = NULL;
	}
    }
}

/* ScriptAssign() {{{1 */
static gint
ScriptAssign(ScriptType *script)
{
    return AddAssign(script->argv[0], script->argv[1]);
}

/* ScriptAutoRes() {{{1 */
static gint
ScriptAutoRes(ScriptType *script)
{
    /* argv[0]: add or gadd
     * argv[1]: 기다릴 문자열
     * argv[2]: string, script
     * argv[3]: action string or script
     */
    if (!g_ascii_strcasecmp(script->argv[0], "add")
	|| !g_ascii_strcasecmp(script->argv[0], "gadd"))
    {
	AutoResponseAdd(script->argv[0], script->argv[1], script->argv[2],
			script->argv[3]);
	return 0;
    }
    else
	g_warning(_("%s: Unknown token '%s'."), __FUNCTION__,
		  (char *) script->argv[0]);
    return -1;
}

/* ScriptBookmark() {{{1 */
static gint
ScriptBookmark(ScriptType * script)
{
    char *p;
    int type;
    GtkItemFactoryEntry entry = { NULL, NULL, NULL, 0, NULL, NULL};
    ScriptBookmarkType *sb;

    /* argv[0]: add
     * argv[1]: bookmark title
     * argv[2]: string, script
     * argv[3]: action string or script
     */
    if (!g_ascii_strcasecmp(script->argv[0], "add"))
	type = 0;
    else if (!g_ascii_strcasecmp(script->argv[0], "gadd"))
	type = SB_GLOBAL;
    else
	return -1;

    p = PC_IconvStr(script->argv[1], -1);
    entry.path = g_strconcat("/Bookmarks/", p, NULL);
    PC_IconvStrFree(script->argv[1], p);

    /* script가 끝나면 script item들은 destroy되므로 복사해서
     * callback_data를 넘겨야 된다. 따라서, 여기에서 convert.
     */
    p = ConvertCtrlChar(script->argv[3]);

    sb = g_new(ScriptBookmarkType, 1);
    sb->type = type;
    sb->id = entry.path;
    if (!g_ascii_strcasecmp(script->argv[2], "script"))
	sb->type |= SB_SCRIPT;
    else if (!g_ascii_strcasecmp(script->argv[2], "sfile"))
	sb->type |= SB_SCRIPTFILE;
    sb->str = p;

    if (!g_ascii_strcasecmp(script->argv[3], "<BRANCH>"))
    {
	sb->type |= SB_BRANCH;
	entry.item_type = "<Branch>";
	entry.callback = NULL;
    }
    else
    {
	entry.item_type = "";
	entry.callback = ScriptRunBookmark;
	gtk_item_factory_create_item(MenuFactory, &entry, sb, 1);
    }

    if (BookmarkRemoved)
    {
	DisconnectFuncList = g_slist_append(DisconnectFuncList,
					    ScriptRemoveBookmark);
	BookmarkRemoved = FALSE;
    }

    BookmarkList = g_slist_prepend(BookmarkList, sb);

    return 0;
}

/* ScriptButton() {{{1 */
static gint
ScriptButton(ScriptType * script)
{
    char *p;
    int type;
    GtkWidget *button;
    ScriptButtonType *sb;

    /* argv[0]: add
     * argv[1]: bookmark title
     * argv[2]: string, script
     * argv[3]: action string or script
     */
    if (!g_ascii_strcasecmp(script->argv[0], "add"))
	type = 0;
    else if (!g_ascii_strcasecmp(script->argv[0], "gadd"))
	type = SB_GLOBAL;
    else
	return -1;

    sb = g_new(ScriptButtonType, 1);

    p = PC_IconvStr(script->argv[1], -1);
    button = gtk_button_new_with_label(p);
    PC_IconvStrFree(script->argv[1], p);

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
    gtk_box_pack_start(GTK_BOX(CtrlBox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(ScriptRunButton), sb);
    gtk_widget_show(button);

    sb->id = (char *) button;

    sb->type = type;
    if (!g_ascii_strcasecmp(script->argv[2], "script"))
	sb->type |= SB_SCRIPT;
    else if (!g_ascii_strcasecmp(script->argv[2], "sfile"))
	sb->type |= SB_SCRIPTFILE;

    p = ConvertCtrlChar(script->argv[3]);
    sb->str = p;

    if (ButtonRemoved)
    {
	DisconnectFuncList = g_slist_append(DisconnectFuncList,
					    ScriptRemoveButton);
	/* should be removed after disconnected */
	ButtonRemoved = FALSE;
    }
    ButtonList = g_slist_append(ButtonList, sb);
    ControlBarFix();

    return 0;
}

/* ScriptControlMenu() {{{1 */
static gint
ScriptControlMenu(ScriptType * script)
{
    gboolean global;

    if (!g_ascii_strcasecmp(script->argv[0], "gadd"))
	global = TRUE;
    else if (!g_ascii_strcasecmp(script->argv[0], "add"))
	global = FALSE;
    else
	return -1;

    ControlBarAddMenu(global, script->argv[1], script->argv[2],
		      script->argv[3], script->argv[4]);
    ControlBarFix();

    return 0;
}

/* ScriptRemoveLabel() {{{1 */
static void
ScriptRemoveLabel(void)
{
    GSList *slist;

    if (ScriptLabelList && !ScriptLabelRemoved)
    {
	for (slist = ScriptLabelList; slist; slist = slist->next)
	{
	    if (!g_ascii_strcasecmp(slist->data, "baudrate"))
		ControlLabelBaudCtrl(FALSE);
	    else if (!g_ascii_strcasecmp(slist->data, "emulate"))
		ControlLabelEmulCtrl(FALSE);
	    else if (!g_ascii_strcasecmp(slist->data, "protocol"))
		ControlLabelProtCtrl(FALSE);
	    else if (!g_ascii_strcasecmp(slist->data, "linestatus"))
		ControlLabelLineStsCtrl(FALSE);
	    else
		g_warning("%s: BUG! invalid label-list", __FUNCTION__);
	    g_free(slist->data);
	}
	g_slist_free(ScriptLabelList);
	ScriptLabelList = NULL;
	ScriptLabelRemoved = TRUE;
    }
}

/* ScriptLabel() {{{1 */
static gint
ScriptLabel(ScriptType * script)
{
    gboolean globflag = FALSE, addflag = TRUE;
    int result;

    if (!g_ascii_strcasecmp(script->argv[0], "add"))
	globflag = FALSE;
    else if (!g_ascii_strcasecmp(script->argv[0], "gadd"))
	globflag = TRUE;
    else if (!g_ascii_strcasecmp(script->argv[0], "rmv"))
	addflag = FALSE;
    else
	return -1;

    if (!g_ascii_strcasecmp(script->argv[1], "baudrate"))
	result = ControlLabelBaudCtrl(addflag);
    else if (!g_ascii_strcasecmp(script->argv[1], "emulate"))
	result = ControlLabelEmulCtrl(addflag);
    else if (!g_ascii_strcasecmp(script->argv[1], "protocol"))
	result = ControlLabelProtCtrl(addflag);
    else if (!g_ascii_strcasecmp(script->argv[1], "linestatus"))
	result = ControlLabelLineStsCtrl(addflag);
    else
	result = -1;

    if (result == 0)
    {
	if (!globflag && addflag)
	{
	    ScriptLabelList = g_slist_append(ScriptLabelList,
					     g_strdup(script->argv[1]));
	    if (ScriptLabelRemoved)
	    {
		DisconnectFuncList = g_slist_append(DisconnectFuncList,
						    ScriptRemoveLabel);
		ScriptLabelRemoved = FALSE;
	    }
	}
	ControlBarFix();
    }
    return result;
}

/* ScriptIf() {{{1 */
static int
ScriptIf(ScriptType *script)
{
    int sts;
    GList *list, *prev;
    ScriptType *scr;
    gint depth, v;
    gboolean same;
    char **argv;

    if (script->argc != 3)
	return -1;

    argv = (char **) script->argv;

    if (!strcmp(argv[1], "=="))
	same = TRUE;
    else if (!strcmp(argv[1], "!="))
	same = FALSE;
    else
	return -1;

    sts = 0;
    if (!strcmp(argv[0], "$?"))
    {
	/* 이전 명령의 실행결과를 확인 */
	v = atoi(argv[2]);
	if ((same && ScriptResult == v) || (!same && ScriptResult != v))
	    sts = 0;
	else
	    sts = -1;
    }
    else
    {
	int c;

	c = strcmp(argv[0], argv[2]);
	if ((same && !c) || (!same && c))
	    sts = 0;
	else
	    sts = -1;
    }

    if (sts < 0)
    {
	/* 조건이 맞지 않으면 ENDIF를 찾는다. */
	depth = 0;
	prev = NULL;
	for (list = CurrScriptItem->next; list; list = list->next)
	{
	    scr = (ScriptType *) list->data;
	    if (scr->type == SCR_IF)
		++depth;
	    else if (scr->type == SCR_ENDIF || scr->type == SCR_ELSE
		     || scr->type == SCR_ELSEIF)
	    {
		if (depth <= 0)
		{
		    if (scr->type == SCR_ELSEIF)
			/* CurrScriptItem을 prev로 설정하여 elseif가
			 * 실행되도록 한다.
			 */
			CurrScriptItem = prev;
		    else
			/* CurrScriptItem을 list로 설정하여 그 다음 script가
			 * 실행되도록 한다.
			 */
			CurrScriptItem = list;
		    break;
		}
		if (scr->type == SCR_ENDIF)
		    --depth;
	    }
	    prev = list;
	}
	if (!list)
	{
	    ScriptFinish();
	    StatusShowMessage(_("CANNOT FIND MATCHING ENDIF!"));
	}
    }
    return 0;
}

/* ScriptElse() {{{1 */
static int
ScriptElse(ScriptType * script)
{
    GList *list;
    ScriptType *scr;
    gint depth;

    depth = 0;
    for (list = CurrScriptItem->next; list; list = list->next)
    {
	scr = list->data;
	if (scr->type == SCR_IF)
	    ++depth;
	else if (scr->type == SCR_ENDIF)
	{
	    if (depth <= 0)
	    {
		CurrScriptItem = list;
		break;
	    }
	    --depth;
	}
    }
    if (!list)
    {
	ScriptFinish();
	StatusShowMessage(_("CANNOT FIND MATCHING ENDIF!"));
    }
    return 0;

    script = NULL;
}

/* ScriptRunSignalHandler() {{{1 */
static void
ScriptRunSignalHandler(void)
{
    GSource *src;

    if (CurrScriptItem && CurrScriptItem->next)
    {
	CurrScriptItem = CurrScriptItem->next;
	ScriptInstallNextItem();
    }
    else
	ScriptFinish();

    g_io_channel_unref(ScriptRunInputChannel);
    close(ScriptRunFD[0]);

    src = g_main_context_find_source_by_id(NULL, ScriptRunInputID);
    if (src)
	g_source_destroy(src);

    TermInputChannel = g_io_channel_unix_new(ReadFD);
    TermInputID = g_io_add_watch(TermInputChannel, G_IO_IN, TermInputHandler,
				 NULL);

    if (ScriptChildStr)
    {
	ScriptRunFromString(ScriptChildStr);
	g_free(ScriptChildStr);
	ScriptChildStr = NULL;
    }
}

/* ScriptRunQueueString() {{{1 */
static void
ScriptRunQueueString(char *buf)
{
    char *s;

    if (ScriptChildStr == NULL)
	ScriptChildStr = g_strdup("");
    if (ScriptChildStr[strlen(ScriptChildStr) - 1] != ';')
    {
	s = ScriptChildStr;
	ScriptChildStr = g_strconcat(s, buf, ";", NULL);
	g_free(s);
    }
}

/* ScriptRunInputHandler() {{{1 */
static gboolean
ScriptRunInputHandler(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    char *p, buf[256];
    int i, size;
    int source;

    UNUSED(cond);
    UNUSED(data);

    source = g_io_channel_unix_get_fd(channel);
    if ((size = read(source, buf, sizeof(buf) - 1)) > 0)
    {
	buf[size] = '\0';
	if (!g_ascii_strncasecmp(buf, "/set script ", 12))
	{
	    for (p = buf; *p; p++)
	    {
		if (*p == '\'' || *p == '"')
		{
		    char c = *p++;
		    while (*p && *p != c)
			p++;
		}
		if (*p == '\n' || *p == '\r')
		    *p = ';';
	    }
	    p = buf + 12;
	    while ((p = strstr(p, "/set script ")) != NULL)
	    {
		for (i = 0; i < 12; i++)
		    *p++ = ' ';
	    }
	    ScriptRunQueueString(buf + 12);
	}
	else
	    Term->localWrite(Term, (guchar*) buf, size);
    }

    return TRUE;
}

/* ScriptRun() {{{1 */
static int
ScriptRun(ScriptType * script)
{
    pid_t pid;
    char *file, buf[MAX_PATH];

    /* GetRealFilename()은 read가 가능한지를 check한다. 따라서, 실행만
     * 가능한 파일은 여기에서 무시된다.
     */
    if ((file = GetRealFilename(script->argv[0], ScriptPath)) == NULL)
	return -1;
    strncpy(buf, file, sizeof(buf) - 1);
    g_free(file);

    if (access(buf, X_OK))
    {
	g_warning(_("%s: cannot execute '%s'"), __FUNCTION__, buf);
	return -1;
    }

    if (pipe(ScriptRunFD) == -1)
    {
	g_warning(_("%s: Cannot create pipe"), __FUNCTION__);
	return -1;
    }

    if ((pid = fork()) < 0)
    {
	g_warning(_("%s: fork() failed"), __FUNCTION__);
	return -1;
    }

    if (pid > 0)
    {
	GSource *src;

	/* I'm parent */
	AddChildSignalHandler(pid, ScriptRunSignalHandler);
	g_io_channel_unref(TermInputChannel);
	TermInputChannel = NULL;

	src = g_main_context_find_source_by_id(NULL, TermInputID);
	if (src)
	    g_source_destroy(src);

	close(ScriptRunFD[1]);
	ScriptRunInputChannel = g_io_channel_unix_new(ScriptRunFD[0]);
	ScriptRunInputID = g_io_add_watch(ScriptRunInputChannel, G_IO_IN,
					  ScriptRunInputHandler, NULL);
	return 0;
    }

    /* I'm child */
    dup2(ReadFD, 0);
    dup2(ReadFD, 1);
    close(ReadFD);
    close(ScriptRunFD[0]);
    dup2(ScriptRunFD[1], 2);
    execlp("sh", "sh", "-c", buf, NULL);

    /* should never reach if command execute success */
    g_warning(_("%s: fail to run '%s'"), __FUNCTION__, buf);
    _exit(1);
}

/* ScriptToDec() {{{1 */
static int
ScriptToDec(ScriptType *script)
{
    char buf[64];
    gulong d;
    char *hnum = (char *) script->argv[0];
    char *dnum = (char *) script->argv[1];

    if (tolower(hnum[1]) == 'x')
	d = (gulong) atol(hnum);
    else
    {
	snprintf(buf, sizeof(buf), "0x%s", hnum);
	d = (gulong) atol(buf);
    }
    snprintf(buf, sizeof(buf), "%lu", d);
    return AddAssign(dnum, buf);
}

/* ScriptToHex() {{{1 */
static int
ScriptToHex(ScriptType *script)
{
    char buf[64];
    gulong d;
    char *dnum = (char *) script->argv[0];
    char *hnum = (char *) script->argv[1];

    d = (gulong) atol(dnum);
    snprintf(buf, sizeof(buf), "%lx", d);
    return AddAssign(hnum, buf);
}

/* ScriptDestroyWinObjList() {{{1 */
static void
ScriptDestroyWinObjList(void)
{
    GSList *slist;
    ScriptWinObjType *obj;

    if (ScriptWinObjList)
    {
	for (slist = ScriptWinObjList; slist; slist = slist->next)
	{
	    obj = slist->data;
	    g_free(obj->str);
	    if (obj->env)
		g_free(obj->env);
	    g_free(obj);
	}
	g_slist_free(ScriptWinObjList);
	ScriptWinObjList = NULL;
    }
}

/* ScriptWinDestroy() {{{1 */
static void
ScriptWinDestroy(GtkWidget *w, GtkWidget **win)
{
    w = *win;

    /* quit button 을 이용하는 경우 밑의 gtk_widget_destroy()에 의해
     * win이 destroy signal을 받아서 이 함수가 다시 불려진다. 즉,
     * quit button을 이용하면 이 함수는 두번 불려지므로 w가 NULL인지를
     * 확인해야 한다.
     */
    if (w)
    {
	*win = NULL;
	gtk_widget_destroy(w);
    }

    ScriptDestroyWinObjList();

    if (ScriptWinSavedList)
    {
	CurrScriptItem = ScriptWinSavedList;
	ScriptWinSavedList = NULL;
	ScriptInstallNextItem();
    }
}

/* ScriptWinButtonClicked() {{{1 */
static void
ScriptWinButtonClicked(GtkWidget *w, gpointer data)
{
    char *p, *p2, *p3, *str, *scriptStr;
    const char *entryStr;
    char *quote;
    ScriptWinObjType *obj;

    obj = g_dataset_get_data(w, "script_data");
    str = g_strdup(obj->str);
    if (data)
    {
	entryStr = gtk_entry_get_text(GTK_ENTRY(data));
	if (!*entryStr)
	    return;

	if ((p = strstr(str, obj->env)) != NULL)
	{
	    /* '$' keyword가 '나 " 사이에 있지 않으면 쌍따옴표로 묵는다. */
	    if (strpbrk(entryStr, " \t"))
	    {
		quote = "\"";
		if ((p2 = strpbrk(str, "'\"")) != NULL)
		{
		    if ((p3 = strchr(p2 + 1, *p2)) != NULL)
		    {
			if (p2 < p && p < p3)
			    quote = "";
		    }
		}
	    }
	    else
		quote = "";

	    *p = '\0';
	    p2 = strpbrk(p + strlen(obj->env), " \t\n\r\\;'\"");
	    scriptStr = g_strconcat(str, quote, entryStr, quote, p2, NULL);
	    g_free(str);
	}
	else
	{
	    g_warning("%s: %s, %s", __FUNCTION__, str, obj->env);
	    scriptStr = str;
	}
	gtk_entry_set_text(GTK_ENTRY(data), "");
    }
    else
	scriptStr = str;

    if (obj->type & SB_SCRIPT)
	ScriptRunFromString(scriptStr);
    else
    {
	str = ConvertCtrlChar(scriptStr);
	Term->remoteWrite((guchar*) str, -1);
	g_free(str);
    }

    g_free(scriptStr);
}

/* ScriptWinCreateVBox() {{{1 */
static GtkWidget *
ScriptWinCreateVBox(GtkWidget *w, char *str)
{
    int spacing;
    GtkWidget *vbox;

    if (w)
    {
	if (!str || sscanf(str, "%d", &spacing) != 1)
	    spacing = 3;
	vbox = gtk_vbox_new(FALSE, spacing);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
	gtk_container_add(GTK_CONTAINER(w), vbox);
	gtk_widget_show(vbox);
	return vbox;
    }
    else
	g_warning(_("Cannot create vbox."));
    return NULL;
}

/* ScriptWinCreateHBox() {{{1 */
static GtkWidget *
ScriptWinCreateHBox(GtkWidget *w, char *str)
{
    int spacing;
    GtkWidget *hbox;

    if (w)
    {
	if (!str || sscanf(str, "%d", &spacing) != 1)
	    spacing = 3;
	hbox = gtk_hbox_new(FALSE, spacing);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	gtk_container_add(GTK_CONTAINER(w), hbox);
	gtk_widget_show(hbox);
	return hbox;
    }
    else
	g_warning(_("Cannot create hbox."));
    return NULL;
}

typedef struct {
    char *name;
    GtkWidget *w;
} ScriptParentWidgetType;

/* ScriptFindParentWidget() {{{1 */
static GtkWidget *
ScriptFindParentWidget(GSList *slist, char *name)
{
    ScriptParentWidgetType *spw;

    for (; slist; slist = slist->next)
    {
	spw = slist->data;
	if (!strcmp(name, spw->name))
	    return spw->w;
    }
    return NULL;
}

/* ScriptDestroyParentWidgetSList() {{{1 */
static void
ScriptDestroyParentWidgetSList(GSList *slist)
{
    if (slist)
    {
	for (; slist; slist = slist->next)
	    g_free(slist->data);
	g_slist_free(slist);
    }
}

/* ScriptGenSPW() {{{1 */
static gpointer
ScriptGenSPW(GtkWidget *w, char *name)
{
    ScriptParentWidgetType *spw;

    spw = g_new(ScriptParentWidgetType, 1);
    spw->w = w;

    /* dup는 필요없다. ScriptCreateWindow() 내에서만 사용되기 때문이다. */
    spw->name = name;
    return spw;
}

/* ScriptCreateWindow() {{{1 */
static int
ScriptCreateWindow(void)
{
    register int i;
    ScriptType *script;
    GtkWidget *w, *hbox, *vbox, *entry, *hsep, *button, *label, *frame;
#define MAX_ENTRY_NUM 2
    int entryNum;
    struct {
	char *str;
	char *actButton;
	GtkWidget *entry;
    } entryTbl[MAX_ENTRY_NUM];
    char *p;
    GSList *slist;

    if (ScriptWin)
    {
	if (GTK_WIDGET_VISIBLE(ScriptWin))
	    gtk_widget_destroy(ScriptWin);
	ScriptWin = NULL;
    }
    w = hbox = vbox = NULL;
    slist = NULL;
    entryNum = 0;
    for (;;)
    {
	script = CurrScriptItem->data;

	switch (script->type)
	{
	    case SCR_WIN_WINDOW:
		ScriptWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_position(GTK_WINDOW(ScriptWin),
					GTK_WIN_POS_MOUSE);
		if (script->argc >= 1)
		{
		    p = PC_IconvStr(script->argv[0], -1);
		    gtk_window_set_title(GTK_WINDOW(ScriptWin), p);
		    PC_IconvStrFree(script->argv[0], p);
		}
		else
		    gtk_window_set_title(GTK_WINDOW(ScriptWin), "Script Win");
		g_signal_connect(G_OBJECT(ScriptWin), "destroy",
				 G_CALLBACK(ScriptWinDestroy),
				 &ScriptWin);
		slist = g_slist_append(slist, ScriptGenSPW(ScriptWin, "win"));
		break;
	    case SCR_WIN_VBOX:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w)
		    goto err_return;
		vbox = ScriptWinCreateVBox(w, script->argv[2]);
		if (vbox == NULL)
		    goto err_return;
		slist = g_slist_append(slist,
				       ScriptGenSPW(vbox, script->argv[1]));
		break;
	    case SCR_WIN_HBOX:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w
		    || (hbox = ScriptWinCreateHBox(w, script->argv[2])) == NULL)
		    goto err_return;
		slist =
		    g_slist_append(slist, ScriptGenSPW(hbox, script->argv[1]));
		break;

	    case SCR_WIN_FRAME:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w)
		    goto err_return;
		p = PC_IconvStr(script->argv[2], -1);
		frame = gtk_frame_new(p);
		PC_IconvStrFree(script->argv[2], p);
		gtk_container_add(GTK_CONTAINER(w), frame);
		gtk_widget_show(frame);
		slist = g_slist_append(slist,
				       ScriptGenSPW(frame, script->argv[1]));
		break;

	    case SCR_WIN_LABEL:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w)
		    goto err_return;
		p = PC_IconvStr(script->argv[1], -1);
		label = gtk_label_new(p);
		PC_IconvStrFree(script->argv[1], p);
		gtk_container_add(GTK_CONTAINER(w), label);
		gtk_widget_show(label);
		break;

	    case SCR_WIN_ENTRY:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w)
		    goto err_return;

		if (entryNum < MAX_ENTRY_NUM
		    && ((char **) script->argv)[1][0] == '$')
		{
		    entry = gtk_entry_new();
		    gtk_box_pack_start(GTK_BOX(w), entry, TRUE, TRUE, 0);
		    gtk_widget_show(entry);

		    entryTbl[entryNum].str = script->argv[1];
		    entryTbl[entryNum].entry = entry;
		    if (script->argc >= 3
			&& !g_ascii_strcasecmp(script->argv[2], "act"))
			entryTbl[entryNum].actButton = script->argv[3];
		    else
			entryTbl[entryNum].actButton = NULL;

		    ++entryNum;
		}
		else
		{
		    g_warning(_("entry error. num=%d, argv[1][0]=%c"),
			      entryNum, ((char **) script->argv)[1][0]);
		    goto err_return;
		}
		break;

	    case SCR_WIN_BUTTON:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (!w)
		    goto err_return;

		if (!g_ascii_strcasecmp(script->argv[2], "quit"))
		{
		    button = gtk_button_new_from_stock(GTK_STOCK_QUIT);
		    gtk_box_pack_start(GTK_BOX(w), button, TRUE, TRUE, 0);
		    g_signal_connect(G_OBJECT(button), "clicked",
				     G_CALLBACK(ScriptWinDestroy),
				     &ScriptWin);
		}
		else
		{
		    gpointer data;
		    ScriptWinObjType *obj;

		    p = PC_IconvStr(script->argv[1], -1);
		    button = gtk_button_new_with_label(p);
		    PC_IconvStrFree(script->argv[1], p);
		    gtk_box_pack_start(GTK_BOX(w), button, TRUE, TRUE, 0);

		    obj = g_new(ScriptWinObjType, 1);
		    obj->str = g_strdup(script->argv[3]);
		    obj->env = NULL;

		    if (!g_ascii_strcasecmp(script->argv[2], "script"))
			obj->type = SB_SCRIPT;
		    else if (!g_ascii_strcasecmp(script->argv[2], "string"))
			obj->type = 0;	/* string */
		    else
		    {
			g_free(obj->str);
			g_free(obj);
			goto err_return;
		    }

		    data = NULL;

		    for (i = 0; i < entryNum; i++)
		    {
			if ((p = strstr(script->argv[3], entryTbl[i].str)) !=
			    NULL)
			{
			    p += strlen(entryTbl[i].str);
			    if (!*p || isspace(*p) || *p == '\\' || *p == ';')
			    {
				data = entryTbl[i].entry;
				obj->env = g_strdup(entryTbl[i].str);

				if (entryTbl[i].actButton
				    && !strcmp(script->argv[1],
					       entryTbl[i].actButton))
				{
				    /* 이 버튼을 active button으로 지정했다면 entry에
				     * 현재 object를 넘기고, activate signal에 연결
				     */
				    g_signal_connect(G_OBJECT(data), "activate",
						     G_CALLBACK(ScriptWinButtonClicked),
						     data);
				    g_dataset_set_data(data, "script_data",
						       obj);
				}
				break;
			    }
			}
		    }

		    /* list에 저장해서 나중에 종료시 free */
		    ScriptWinObjList = g_slist_append(ScriptWinObjList, obj);
		    g_dataset_set_data(button, "script_data", obj);
		    g_signal_connect(G_OBJECT(button), "clicked",
				     G_CALLBACK(ScriptWinButtonClicked),
				     data);
		}
		gtk_widget_show(button);
		break;

	    case SCR_WIN_HSEP:
		w = ScriptFindParentWidget(slist, script->argv[0]);
		if (w)
		{
		    hsep = gtk_hseparator_new();
		    gtk_box_pack_start(GTK_BOX(w), hsep, FALSE, FALSE, 3);
		    gtk_widget_show(hsep);
		}
		else
		    g_warning(_("Cannot create hseperator."));
		/* 대세에 영향을 주지 않으므로 경고만 출력 */
		break;

	    case SCR_WIN_WAIT:
	    case SCR_WIN_END:
		if (ScriptWin)
		    gtk_widget_show(ScriptWin);
		ScriptDestroyParentWidgetSList(slist);
		return 0;

	    default:
err_return:
		/* help debug */
		g_print(_("ERROR: maybe you make a invalid script arguments.\n"
			  "ARGS:"));
		for (i = 0; i < script->argc; i++)
		    g_print(" %s", (char *) script->argv[i]);
		g_print("\n");

		CurrScriptItem->next = NULL;	/* finish the script */
		break;
	}

	CurrScriptItem = CurrScriptItem->next;
	if (CurrScriptItem == NULL)
	    break;
    }

    if (ScriptWin)
    {
	gtk_widget_destroy(ScriptWin);
	ScriptWin = NULL;
    }

    ScriptDestroyParentWidgetSList(slist);
    ScriptDestroyWinObjList();
    ScriptFinish();
    return -1;
}

/* ScriptAssignConvert() {{{1 */
static void
ScriptAssignConvert(ScriptType *script)
{
    int i;
    AssignType *a = NULL;
    char *s, *e, *f;
    char *sa;

    if (script->savedArgv)
    {
	for (i = 0; i < script->argc; i++)
	{
	    if (script->savedArgv[i])
	    {
		g_free(script->argv[i]);
		script->argv[i] = g_strdup(script->savedArgv[i]);
	    }
	}
    }

    for (i = 0; i < script->argc; i++)
    {
	sa = script->argv[i];
	while (*sa && (s = strchr(sa, '$')) != NULL)
	{
	    if (s != sa && s[-1] == '\\')
	    {
		sa = s + 1;
		continue;
	    }
	    if ((e = FindWordSeperator(s + 1)) == NULL)
		f = g_strdup(s + 1);
	    else
	    {
		guint l = (guint) (e - s - 1);

		if (l)
		{
		    f = g_malloc(l + 1);
		    memcpy(f, s + 1, l);
		    f[l] = '\0';
		}
		else
		{
		    sa = s + 1;
		    continue;
		}
	    }

	    a = GetAssign(f);
	    g_free(f);

	    if (script->savedArgv == NULL)
		script->savedArgv = g_new0(gpointer, script->argc);
	    if (script->savedArgv[i] == NULL)
		script->savedArgv[i] = g_strdup(script->argv[i]);

	    sa = script->argv[i];
	    if (a == NULL)
		script->argv[i] = g_strdup("");
	    else
	    {
		*s = '\0';
		script->argv[i] = g_strdup_printf("%s%s%s", *sa? sa: "",
						  a->val,
						  e? e: "");
	    }
	    g_free(sa);
	    sa = script->argv[i];
	}
    }
}

/* ScriptRunNext() {{{1 */
static int
ScriptRunNext(void)
{
    ScriptType *script;
    unsigned long ul;
    gchar *p;
    gboolean doInstallNext;

    if (CurrScriptItem == NULL)
    {
	ScriptFinish();
	return FALSE;
    }

    IdleTag = 0;
    PauseTag = 0;
    doInstallNext = TRUE;

    script = CurrScriptItem->data;
    ScriptAssignConvert(script);

    switch (script->type)
    {
	case SCR_ASSIGN:
	    ScriptResult = ScriptAssign(script);
	    break;
	case SCR_AUTORES:
	    ScriptResult = ScriptAutoRes(script);
	    break;
	case SCR_BEEP:
	    if (SoundPlay(SoundBeepFile) < 0)
	    {
		int percent = (int) BeepPercent;
		GdkDisplay *display = gdk_display_get_default();

		if (script->argc > 0)
		{
		    if (sscanf(script->argv[0], "%d", &percent) != 1
			|| percent == 0)
			percent = (int) BeepPercent;
		}
		XBell(GDK_DISPLAY_XDISPLAY(display), percent);
	    }
	    ScriptResult = 0;
	    break;
	case SCR_BOOKMARK:
	    ScriptResult = ScriptBookmark(script);
	    break;
	case SCR_BUTTON:
	    ScriptResult = ScriptButton(script);
	    break;
	case SCR_CAPTURE:
	    if (!g_ascii_strcasecmp(script->argv[0], "on"))
	    {
		if (script->argc >= 2)
		{
		    gboolean includeBuf;

		    ScriptCaptureActive = TRUE;
		    if (script->argc == 3)
			includeBuf = g_ascii_strcasecmp(script->argv[2],
							"excludebuf");
		    else
			includeBuf = TRUE;
		    ScriptResult = CaptureStart(script->argv[1], includeBuf);
		    break;
		}
	    }
	    else if (!g_ascii_strcasecmp(script->argv[0], "off"))
	    {
		ScriptCaptureActive = FALSE;
		CaptureFinish();
		ScriptResult = 0;
		break;
	    }
	    ScriptResult = -1;
	    break;
	case SCR_CLEAR:
	    TermClearScreen(Term);
	    ScriptResult = 0;
	    break;
	case SCR_CTLMENU:
	    ScriptResult = ScriptControlMenu(script);
	    break;
	case SCR_DISCONNECT:
	    PC_Hangup();
	    ScriptResult = 0;
	    break;
	case SCR_DOWNLOAD:
	    if (!g_ascii_strcasecmp(script->argv[0], "zmodem"))
	    {
		TRxProtocolSetFromString(script->argv[0]);
		RunRZ();
		ScriptResult = 0;
		break;
	    }
	    /* TODO: zmodem이 아닌 경우 지원 */
	    ScriptResult = -1;
	    break;
	case SCR_ELSE:
	    ScriptResult = ScriptElse(script);
	    break;
	case SCR_EMULATE:
	    if (!g_ascii_strcasecmp(script->argv[0], "ANSI"))
		EmulateMode = EMULATE_ANSI;
	    else if (!g_ascii_strcasecmp(script->argv[0], "VT100"))
		EmulateMode = EMULATE_VT100;
	    else
	    {
		ScriptResult = -1;
		break;
	    }
	    TermEmulModeSet();
	    break;
	case SCR_END:
	    ScriptFinish();
	    break;
	case SCR_ENDIF:
	    /* do nothing */
	    ScriptResult = 0;
	    break;
	case SCR_ENDWIN:
	    if (ScriptWin)
	    {
		gtk_widget_destroy(ScriptWin);
		ScriptWin = NULL;
	    }
	    break;
	case SCR_GET_BAUDRATE:
	    ScriptResult = ScriptGetBaudrate(script);
	    break;
	case SCR_GET_FILEINFO:
	    ScriptResult = ScriptGetFileInfo(script);
	    break;
	case SCR_GOTO:
	    ScriptResult = ScriptGoto(script->argv[0]);
	    break;
	case SCR_GOTO_TAG:
	    /* do nothing */
	    ScriptResult = 0;
	    break;
	case SCR_IF:
	case SCR_ELSEIF:
	    ScriptResult = ScriptIf(script);
	    break;
	case SCR_KFLUSH:
	    /* procomm plus script 호환 */
	    ScriptResult = 0;
	    break;
	case SCR_LABEL:
	    ScriptResult = ScriptLabel(script);
	    break;
	case SCR_MESSAGE:
	    p = ConvertCtrlChar(script->argv[0]);
	    Term->localWrite(Term, (guchar*) p, -1);
	    g_free(p);
	    ScriptResult = 0;
	    break;
	case SCR_PAUSE:
	    /* pause가 마지막이면 pause가 의미가 있겠는가? */
	    if (CurrScriptItem->next)
	    {
		float f;
		if (sscanf(script->argv[0], "%f", &f) == 1 && f >= 0.1)
		    PauseTag = gtk_timeout_add((guint32) (f * 1000UL),
					       (GtkFunction) ScriptRunNext,
					       NULL);
		else
		    script->type = SCR_NONE;
	    }
	    ScriptResult = 0;
	    break;
	case SCR_RFLUSH:
	    /* procomm plus script 호환 */
	    ScriptResult = 0;
	    break;
	case SCR_RUN:
	    ScriptResult = ScriptRun(script);
	    doInstallNext = FALSE;
	    break;
	case SCR_STSMSG:
	    p = ConvertCtrlChar(script->argv[0]);
	    StatusShowMessage("%s", p);
	    g_free(p);
	    ScriptResult = 0;
	    break;
	case SCR_TODEC:
	    ScriptResult = ScriptToDec(script);
	    break;
	case SCR_TOHEX:
	    ScriptResult = ScriptToHex(script);
	    break;
	case SCR_TRANSMIT:
	    p = ConvertCtrlChar(script->argv[0]);
	    Term->remoteWrite((guchar*) p, -1);
	    g_free(p);
	    ScriptResult = 0;
	    break;
	case SCR_UPLOAD:
	    TRxProtocolSetFromString(script->argv[0]);
	    ScriptResult = 0;
	    if (*(char *) script->argv[1] == '?')
		UploadFileQuery();
	    else
	    {
		char *txfname = g_strdup(script->argv[1]);

		if (TRxSendFile(txfname) < 0)
		{
		    g_warning(_("Cannot upload '%s'."), txfname);
		    ScriptResult = -1;
		}
		g_free(txfname);
	    }
	    while (TRxRunning != TRX_IDLE)
		PC_Sleep(500);
	    break;
	case SCR_WAITFOR:
	    /* waitfor가 마지막이면 waitfor가 의미가 있겠는가? */
	    doInstallNext = FALSE;
	    if (CurrScriptItem->next)
	    {
		float waitTime = 0;
		if (script->argc >= 2)
		    if (sscanf(script->argv[1], "%f", &waitTime) != 1)
			waitTime = 0;

		ScriptWaitFor(script, waitTime);
	    }
	    break;

	case SCR_SET_BAUDRATE:
	    if (sscanf(script->argv[0], "%lu", &ul) != 1
		|| ModemSetSpeed(ul) == FALSE)
	    {
		StatusShowMessage(_("Cannot set baudrate to '%lu'."),
				  (unsigned long) ul);
		ScriptResult = -1;
	    }
	    else
	    {
		ScriptResult = 0;
		doInstallNext = FALSE;	/* window가 종료후 next item install */
		CurrScriptItem = CurrScriptItem->next;
		/* baudrate를 바꾼 후에는 2초 정도 device를 쓰거나 읽지않도록
		 * 한다. 이것은 실험 결과치임. 2초 이내에 쓰는 데이타는 그냥
		 * 사라져버린다.
		 */
		PauseTag = gtk_timeout_add(2500UL,
					   (GtkFunction) ScriptRunNext, NULL);
	    }
	    break;
	case SCR_SET_CONFIG:
	    ScriptResult = ConfigVarChange(script->argv[0], script->argv[1],
					   TRUE);
	    break;
	case SCR_SET_DEBUG:
#if defined(_DEBUG_)
	    if (sscanf(script->argv[0], "%lu", &ul) == 1
		&& ul < G_N_ELEMENTS(OPrintFlag))
	    {
		OPrintFlag[ul] = !OPrintFlag[ul];
		ScriptResult = 0;
	    }
	    else
		ScriptResult = -1;
#endif /* _DEBUG_ */
	    break;
	case SCR_SET_PROTOCOL:
	    ScriptResult = TRxProtocolSetFromString(script->argv[0]);
	    break;
	case SCR_SET_TITLE:
	    SetWindowTitle(script->argv[0]);
	    ScriptResult = 0;
	    break;

	case SCR_WIN_WINDOW:
	    ScriptResult = ScriptCreateWindow();
	    if (CurrScriptItem)
	    {
		if (((ScriptType *) CurrScriptItem->data)->type == SCR_WIN_WAIT)
		{
		    doInstallNext = FALSE;	/* window가 종료후 next item install */
		    ScriptWinSavedList = CurrScriptItem;
		}
	    }
	    break;
	case SCR_WIN_END:
	case SCR_WIN_WAIT:
	    /* nothing to do */
	    break;

	default:
	    /* 강제로 종료시킨다. */
	    CurrScriptItem = NULL;
	    g_warning(_("%s: Script finished (type=%d)."),
		      __FUNCTION__, script->type);
	    break;
    }

    if (CurrScriptItem && CurrScriptItem->next)
    {
	if (script->type == SCR_PAUSE)
	    CurrScriptItem = CurrScriptItem->next;
	else if (doInstallNext)
	{
	    CurrScriptItem = CurrScriptItem->next;
	    script = CurrScriptItem->data;
	    if (script->type == SCR_WAITFOR)
	    {
		float waitTime = 0;

		ScriptAssignConvert(script);
		if (script->argc >= 2)
		{
		    if (sscanf(script->argv[1], "%f", &waitTime) != 1)
			waitTime = 0;
		}

		ScriptWaitFor(script, waitTime);
		return FALSE;
	    }
	    ScriptInstallNextItem();
	}
    }
    else
	ScriptFinish();

    return FALSE;
}

/* ScriptCancel() {{{1 */
void
ScriptCancel(void)
{
    ScriptFinish();
}

/* GetAssign() {{{1 */
static AssignType *
GetAssign(const char *s)
{
    GSList *slist;
    AssignType *assign;

    if (s && *s)
    {
	for (slist = AssignList; slist; slist = slist->next)
	{
	    assign = slist->data;
	    if (!strcmp(assign->var, s))
		return assign;
	}
    }
    return NULL;
}

/* ScriptParsingError() {{{1 */
static void
ScriptParsingError(char *param[], int nParam)
{
    int i;

    /* print as many as possible to make debug easy */
    g_print(_("%s: Invalid line in script. nParam=%d\n\t%s"), __FUNCTION__,
	    nParam, param[0]);

    for (i = 1; i < nParam; i++)
	g_print(" %s", param[i]);
    g_print("\n");
}

/* AddAssign() {{{1 */
static int
AddAssign(char *var, char *val)
{
    AssignType *a;

    if ((a = GetAssign(var)) == NULL)
    {
	a = g_new(AssignType, 1);
	a->var = g_strdup(var);
	a->val = g_strdup(val);
	AssignList = g_slist_append(AssignList, a);
    }
    else
    {
	g_free(a->val);
	a->val = g_strdup(val);
    }
    return 0;
}

/* ScriptCreateItem() {{{1 */
static ScriptType *
ScriptCreateItem(char *param[], int nArg, int paramPos,
		 const ScriptTableType *tbl, int *result)
{
    register int i;
    ScriptType *script;
    char *p;

    *result = -1;	/* assume error */

    if (paramPos >= MAX_SCRIPT_PARAMS || nArg < 0)
    {
	ScriptParsingError(param, nArg + paramPos + 1);
	return NULL;
    }

    /* find the token from the table */
    for (; tbl->type >= 0; tbl++)
	if (!g_ascii_strcasecmp(param[paramPos], tbl->token))
	    break;

    if (tbl->type >= 0)
    {
	if (tbl->next)
	    return ScriptCreateItem(param, nArg - 1, paramPos + 1, tbl->next,
				    result);
	else
	{
	    script = g_new(ScriptType, 1);
	    script->type = tbl->type;
	    script->savedArgv = NULL;
	    if (nArg > 0 || tbl->argc > 0)
	    {
		if (nArg < tbl->argc)
		{
		    ScriptParsingError(param, nArg + paramPos + 1);
		    g_free(script);
		    return NULL;
		}
		script->argc = nArg;
		script->argv = g_new(gpointer, nArg);
		++paramPos;
		for (i = 0; i < nArg; i++)
		    script->argv[i] = g_strdup(param[paramPos + i]);
	    }
	    else
		script->argc = 0;
	    *result = 0;
	    return script;
	}
    }
    else
    {
	if (paramPos == 0)
	{
	    /* check if it is label */
	    p = param[0] + strlen(param[0]) - 1;
	    if (*p == ':')
	    {
		*p = '\0';
		script = g_new(ScriptType, 1);
		script->type = SCR_GOTO_TAG;
		script->argc = 1;
		script->argv = g_new(gpointer, 1);
		script->argv[0] = g_strdup(param[0]);
		script->savedArgv = NULL;
		*result = 0;
		return script;
	    }
	}
	ScriptParsingError(param, nArg + paramPos + 1);
    }
    return NULL;
}

/* ScriptInclude() {{{1 */
static GList *
ScriptInclude(GList *list, ScriptType * script)
{
    char *filename;
    GList *subList, *last;

    if ((filename = GetRealFilename(script->argv[0], ScriptPath)) != NULL)
    {
	subList = ScriptMakeListFromFile(filename);
	g_free(filename);

	if (subList != NULL)
	{
	    if (list == NULL)
		list = subList;
	    else
	    {
		last = g_list_last(list);
		last->next = subList;
		subList->prev = last;
	    }
	}
    }
    g_free(script->argv[0]);
    g_free(script->argv);
    g_free(script);
    return list;
}

/* ScriptInitCurrScriptItem() {{{1 */
static void
ScriptInitCurrScriptItem(GList *list)
{
    if (!ScriptItemList)
	CurrScriptItem = ScriptItemList = list;
    else
    {
	GList *last;

	CurrScriptItem->prev->next = list;
	list->prev = CurrScriptItem->prev;
	last = g_list_last(list);
	last->next = CurrScriptItem;
	CurrScriptItem->prev = last;
	CurrScriptItem = list;
    }
}

/* ScriptMakeListFromFile() {{{1 */
static GList *
ScriptMakeListFromFile(char *filename)
{
    int n, result;
    FILE *fp;
    GList *list;
    ScriptType *script;
    char *param[MAX_SCRIPT_PARAMS], *buf;

    if (filename == NULL || *filename == '\0')
    {
	StatusShowMessage(_("%s: filename is empty"), __FUNCTION__);
	return NULL;
    }

    if ((fp = fopen(filename, "r")) != NULL)
    {
	list = NULL;
	while ((buf = GetLines(fp)) != NULL)
	{
	    OptPrintf(OPRT_SCRIPT, "%s: s=%s\n", __FUNCTION__, buf);

	    if ((n = ParseLine(buf, MAX_SCRIPT_PARAMS, param)) < 1)
		continue;

	    OptPrintf(OPRT_SCRIPT, "%s: n=%d, param[0]=%s\n", __FUNCTION__,
		      n, param[0]);

	    script = ScriptCreateItem(param, n - 1, 0, ScriptMainTable,
				      &result);
	    if (script == NULL)
	    {
		if (result == 0)
		    /* just ignore this line! */
		    continue;

		g_list_free(list);
		list = NULL;
		break;
	    }

	    if (script->type == SCR_INCLUDE)
		list = ScriptInclude(list, script);
	    else
		list = g_list_append(list, script);
	}
	fclose(fp);
	return list;
    }
    else
	StatusShowMessage(_("%s: cannot open file '%s'"), __FUNCTION__,
			  filename);

    return NULL;
}

/* ScriptGetFullPath() {{{1 */
char *
ScriptGetFullPath(const char *filename)
{
    char *s;

    /* 일단 global script path로부터 filename이 있는 지를 찾고, 없으면
     * filename만 가지고 찾는다. 간결한 code를 위해 filename이 absolute
     * path인지 등은 검사하지 않는다. 이게 문제가 될까?
     */
    if (strchr(filename, '.') == NULL)
    {
	char *buf = g_malloc(MAX_PATH);
	g_snprintf(buf, MAX_PATH, "%s" DEFAULT_SCRIPT_EXT, filename);
	s = GetRealFilename(buf, ScriptPath);
	g_free(buf);
    }
    else
	s = GetRealFilename(filename, ScriptPath);

    return s;
}

/* ScriptGenListFromFile() {{{1 */
static GList *
ScriptGenListFromFile(const char *filename)
{
    GList *list = NULL;
    char *buf;

    buf = ScriptGetFullPath(filename);
    if (buf && *buf)
	list = ScriptMakeListFromFile(buf);
    else
	StatusShowMessage(_("Cannot find '%s' with default extention/path"),
			  filename);

    return list;
}

/* ScriptRunFromFile() {{{1 */
void
ScriptRunFromFile(const char *filename)
{
    GList *list;

    if ((list = ScriptGenListFromFile(filename)) != NULL)
    {
	ScriptRunning = TRUE;
	ScriptInitCurrScriptItem(list);
	CurrScriptFile = g_strdup(filename);
	ScriptResult = 0;
	ScriptRunAnimStart();
	ScriptInstallNextItem();
    }
    else
	ScriptRunning = FALSE;
}

/* ScriptMakeListFromString() {{{1 */
static GList *
ScriptMakeListFromString(const char *inputStr)
{
    int n, result;
    GList *list;
    ScriptType *script;
    char *param[MAX_SCRIPT_PARAMS];
    char *s, *beginSentence, *freePtr;

    list = NULL;
    s = freePtr = g_strdup(inputStr);

    OptPrintf(OPRT_SCRIPT, "%s: s=%s\n", __FUNCTION__, s);
    while (*s)
    {
	beginSentence = s;	/* 문장 처음 저장 */

	/* 문장끝을 찾는다. */
	for (; *s; s++)
	{
	    /* double quote 안에 있을 지도 모르는 문장끝을 의미하는 ';'를
	     * skip한다.
	     */
	    if (*s == '"' || *s == '\'')
	    {
		char c = *s;
		do
		{
		    ++s;
		    if (*s == c)
			break;
		}
		while (*s);
	    }
	    if (*s == ';')
		break;
	}

	/* ';'이 아니라면 *s는 반드시 NUL 이어야 한다. */
	if (*s == ';')
	    *s++ = '\0';
	else if (*s)
	    g_warning(_("%s: parser error"), __FUNCTION__);

	if ((n = ParseLine(beginSentence, MAX_SCRIPT_PARAMS, param)) < 1)
	    continue;

	OptPrintf(OPRT_SCRIPT, "%s: n=%d, param[0]=%s, *s=%d\n", __FUNCTION__,
		  n, param[0], *s);

	script = ScriptCreateItem(param, n - 1, 0, ScriptMainTable, &result);
	if (script == NULL)
	{
	    if (result == 0)
		/* just ignore this line! */
		continue;

	    break;
	}

	if (script->type == SCR_INCLUDE)
	    list = ScriptInclude(list, script);
	else
	    list = g_list_append(list, script);
    }
    g_free(freePtr);
    return list;
}

/* ScriptRunFromString() {{{1 */
void
ScriptRunFromString(const char *str)
{
    GList *list;

    if (str)
    {
	if ((list = ScriptMakeListFromString(str)) != NULL)
	{
	    ScriptRunning = TRUE;
	    ScriptInitCurrScriptItem(list);
	    CurrScriptFile = g_strdup("string");
	    ScriptResult = 0;
	    ScriptRunAnimStart();
	    ScriptInstallNextItem();
	}
	else
	    StatusShowMessage(_("Fail to run '%s'."), str);
    }
}

/* IsSystemScript() {{{1 */
/* file이 login/logout/startup script이면 TRUE return */
static gboolean
IsSystemScript(const char *filename)
{
    GSList *slist;
    ModemInfoType *mi;
    TelnetInfoType *ti;

    for (slist = ModemInfoList; slist; slist = slist->next)
    {
	mi = slist->data;
	if (mi->loginScript && !strcmp(mi->loginScript, filename))
	    return TRUE;
	if (mi->logoutScript && !strcmp(mi->logoutScript, filename))
	    return TRUE;
    }
    for (slist = TelnetInfoList; slist; slist = slist->next)
    {
	ti = slist->data;
	if (ti->loginScript && !strcmp(ti->loginScript, filename))
	    return TRUE;
	if (ti->logoutScript && !strcmp(ti->logoutScript, filename))
	    return TRUE;
    }
    if (!strcmp(filename, DEFAULT_STARTUP_SCRIPT))
	return TRUE;

    return FALSE;
}

/* CreateScriptListFromPath() {{{1 */
static GList *
CreateScriptListFromPath(char *fullpath, GList *list)
{
    register guint i;
    glob_t gl;
    char *filename, *p;

    if (glob(fullpath, 0, NULL, &gl) == 0)
    {
	for (i = 0; i < gl.gl_pathc; i++)
	{
	    filename = g_path_get_basename(gl.gl_pathv[i]);

	    /* system script는 combo box에서는 실행할 수 없도록 한다.
	     * 정 실행해 보고 싶으면 script 실행 대화상자에서 ...
	     *
	     * NOTE: 이 기능은 약간의 문제가 있는 데, 그것은 full path가 아닌
	     * 파일명만 가지고 비교한다는 것이다.
	     */
	    if (IsSystemScript(filename))
	    {
		g_free(filename);
		continue;
	    }
	    if ((p = strrchr(filename, '.')) != NULL)
		*p = '\0';
	    list = g_list_append(list, filename);
	}
	globfree(&gl);
    }

    return list;
}

/* CreateScriptFileList() {{{1 */
/* 여기에서는 GList 를 사용하는 데, combo에서는 GList를 사용해야 되기 때문임.
 */
GtkWidget *
CreateScriptFileList(GtkWidget *w, GCallback runCB)
{
    GtkWidget *cb = NULL;
    GList *list = NULL;
    char buf[MAX_PATH];

    UNUSED(w);

    /* current directory에 있는 script file들 */
    g_snprintf(buf, sizeof(buf), "./*" DEFAULT_SCRIPT_EXT);
    list = CreateScriptListFromPath(buf, list);

    /* ScriptPath에 있는 script file들 */
    g_snprintf(buf, sizeof(buf), "%s/*" DEFAULT_SCRIPT_EXT, ScriptPath);
    list = CreateScriptListFromPath(buf, list);

    if (list)
    {
	cb = gtk_combo_new();
	gtk_combo_set_popdown_strings(GTK_COMBO(cb), list);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(cb)->entry),
			   (gchar *) list->data);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(cb)->entry), FALSE);
	ScriptBox = GTK_WIDGET(GTK_COMBO(cb)->entry);

	/* XXX: combo box에서 다른 script 선택시 runCB가 불려질 수 있게 */
	UNUSED(runCB);
    }

    return cb;
}
/* }}} */

/****************************************************************************
			       A N I M A T I O N
****************************************************************************/
/* ScriptAnimType {{{1 */
typedef struct {
    GtkWidget *w;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    char *filename;
    int currFrame;
    int frameNum;
    int size;
    guint tag;
} ScriptAnimType;

static ScriptAnimType *ScriptAnim = NULL;

guint32 RunIconLargeSize, RunIconSmallSize;

/* ScriptRunNextFrame() {{{1 */
static gint
ScriptRunNextFrame(gpointer data)
{
    ScriptAnimType *sa = data;

    sa->currFrame = (sa->currFrame + 1) % sa->frameNum;
    if (!sa->currFrame)
	sa->currFrame = 1;

    gtk_widget_queue_draw(sa->w);
    return TRUE;
}

/* ScriptRunExpose() {{{1 */
static gint
ScriptRunExpose(GtkWidget *w, GdkEventExpose *ev, gpointer data)
{
    ScriptAnimType *sa = data;

    UNUSED(w);
    UNUSED(ev);

    gdk_draw_drawable(sa->w->window, sa->w->style->black_gc, sa->pixmap,
		      sa->currFrame * sa->size, 0, 0, 0, sa->size, sa->size);
    return TRUE;
}

/* ScriptRunAnimInit() {{{1 */
GtkWidget *
ScriptRunAnimInit(GCallback runCB)
{
    int width, height;
    char *filename;
    GdkPixbuf *pb;
    ScriptAnimType *sa;

    if (!ScriptAnim && runCB)
    {
	filename = GetRealFilename(ScriptAnimFile, PKGDATADIR "/pixmap");
	if (filename == NULL)
	    return NULL;

	ScriptAnim = g_new(ScriptAnimType, 1);
	sa = ScriptAnim;

	if (!g_ascii_strcasecmp(ToolBarType, "BOTH"))
	    sa->size = RunIconLargeSize;
	else
	    sa->size = RunIconSmallSize;

	sa->filename = filename;
	sa->pixmap = NULL;
	sa->mask = NULL;

	sa->w = gtk_drawing_area_new();
	gtk_widget_ref(sa->w);
	sa->w->requisition.width = sa->size;
	sa->w->requisition.height = sa->size;
	gtk_widget_queue_resize(sa->w);
	g_signal_connect(G_OBJECT(sa->w), "expose_event",
			 G_CALLBACK(ScriptRunExpose), sa);
	g_signal_connect(G_OBJECT(sa->w), "button_press_event",
			 G_CALLBACK(runCB), sa);
	gtk_widget_set_events(GTK_WIDGET(sa->w),
			      GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK);
    }

    if (ScriptAnim && ScriptAnim->filename)
    {
	sa = ScriptAnim;
	pb = gdk_pixbuf_new_from_file(sa->filename, NULL);
	if (pb == NULL)
	    return NULL;

	width = gdk_pixbuf_get_width(pb);
	height = gdk_pixbuf_get_height(pb);
	if (height != sa->size)
	{
	    GdkPixbuf *pbt;
	    width = width * ((double)sa->size / height);
	    height = sa->size;
	    pbt = gdk_pixbuf_scale_simple(pb, width, height,
					  GDK_INTERP_NEAREST);
	    g_object_unref(pb);
	    pb = pbt;
	}

	sa->frameNum = width / height;
	sa->currFrame = 0;

	gdk_pixbuf_render_pixmap_and_mask(pb, &sa->pixmap, &sa->mask, 128);
	g_object_unref(pb);

	gtk_widget_shape_combine_mask(sa->w, sa->mask, 0, 0);
	return ScriptAnim->w;
    }

    return NULL;
}

/* ScriptRunAnimDestroy() {{{1 */
void
ScriptRunAnimDestroy(void)
{
    if (ScriptAnim)
    {
	gtk_widget_destroy(ScriptAnim->w);
	g_free(ScriptAnim->filename);
	g_free(ScriptAnim);
	ScriptAnim = NULL;
    }
}

/* ScriptRunSizeChanged() {{{1 */
void
ScriptRunSizeChanged(gboolean large)
{
    ScriptAnimType *sa = ScriptAnim;
    if (sa)
    {
	if (large)
	    sa->size = RunIconLargeSize;
	else
	    sa->size = RunIconSmallSize;
	sa->w->requisition.width = sa->size;
	sa->w->requisition.height = sa->size;
	gtk_widget_queue_resize(sa->w);
	gtk_widget_set_size_request(GTK_WIDGET(sa->w), sa->size, sa->size);
	ScriptRunAnimInit(NULL);
    }
}

/* ScriptRunAnimStart() {{{1 */
static void
ScriptRunAnimStart(void)
{
    if (ScriptAnim && ScriptAnim->tag == 0)
	ScriptAnim->tag = gtk_timeout_add(300, ScriptRunNextFrame, ScriptAnim);
}

/* ScriptRunAnimStop() {{{1 */
static void
ScriptRunAnimStop(void)
{
    if (ScriptAnim && ScriptAnim->tag != 0)
    {
	gtk_timeout_remove(ScriptAnim->tag);
	ScriptAnim->tag = 0;
	ScriptAnim->currFrame = 0;
	gtk_widget_queue_draw(ScriptAnim->w);
    }
}
