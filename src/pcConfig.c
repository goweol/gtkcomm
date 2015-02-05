/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * config.
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
#include <sys/stat.h>	/* stat() */

#include "pcMain.h"
#include "pcTerm.h"
#include "pcChat.h"

/* Definitions {{{1 */

#define OPT_STR	     0x80000000UL /* string: ConvertCtrlChar()에 의해 변환 */
#define OPT_COLOR    0x40000000UL /* color: GdkColor() */
#define OPT_PATH     0x20000000UL /* path: use ExpandPath() */
#define OPT_BOOL     0x00000001UL /* boolean number */
#define OPT_8BIT     0x00000002UL /* 8bit number */
#define OPT_16BIT    0x00000004UL /* 16bit number */
#define OPT_32BIT    0x00000008UL /* 32bit number */

/* OptTable {{{1 */
struct _OptTable {
    char *name;
    guchar **ptr;
    guint32 opt;
    gpointer init;
};

static const struct _OptTable OptTable[] = {
    {"AutoRunZmodem", (guchar **) &AutoRunZmodem, OPT_BOOL, "TRUE"},
    {"BackspaceDeleteChar", (guchar **) &BackspaceDeleteChar, OPT_BOOL, "FALSE"},
    {"BeepPercent", (guchar **) &BeepPercent, OPT_32BIT, "100"},
    {"CaptureFile", (guchar **) &CaptureFile, OPT_PATH, DEFAULT_CAPTURE_FILE},
    {"CapturePath", (guchar **) &CapturePath, OPT_PATH, DEFAULT_CAPTURE_PATH},
    {"ChatPrefix", (guchar **) &ChatPrefix, OPT_STR, ""},
    {"ChatSuffix", (guchar **) &ChatSuffix, OPT_STR, ""},
    {"LogFile", (guchar **) &LogFile, OPT_PATH, ""},
    {"ControlBarShow", (guchar **) &ControlBarShow, OPT_BOOL, "TRUE"},
    {"DoLocalEcho", (guchar **) &DoLocalEcho, OPT_BOOL, "FALSE"},
    {"DoLock", (guchar **) &DoLock, OPT_BOOL, "FALSE"},
    {"DoRemoteEcho", (guchar **) &DoRemoteEcho, OPT_BOOL, "FALSE"},
    {"DownloadPath", (guchar **) &DownloadPath, OPT_PATH, DEFAULT_DOWNLOAD_PATH},
    {"EnterConvert", (guchar **) &EnterConvert, OPT_32BIT, "1"},
    {"ISel_FTP_Command", (guchar **) &ISel_FTP_Command, OPT_PATH,
	DEFAULT_ISEL_FTP_COMMAND},
    {"ISel_HTTP_Command", (guchar **) &ISel_HTTP_Command, OPT_PATH,
	DEFAULT_ISEL_HTTP_COMMAND},
    {"ISel_TELNET_Command", (guchar **) &ISel_TELNET_Command, OPT_PATH,
	DEFAULT_ISEL_TELNET_COMMAND},
    {"IdleGapBetweenChar", (guchar **) &IdleGapBetweenChar, OPT_32BIT, "0"},
    {"IdleGapBetweenLine", (guchar **) &IdleGapBetweenLine, OPT_32BIT, "0"},
    {"IdleGuardInterval", (guchar **) &IdleGuardInterval, OPT_32BIT, "180"},
    {"IdleGuardString", (guchar **) &IdleGuardString, OPT_STR,
	DEFAULT_IDLE_GUARD_STRING},
    {"LeftMouseClickStr", (guchar **) &LeftMouseClickStr, OPT_STR, ""},
    {"MaxCaptureSize", (guchar **) &MaxCaptureSize, OPT_32BIT,
	DEFAULT_MAX_CAPTURE_SIZE_STR},
    {"ModemMaxHistory", (guchar **) &ModemMaxHistory, OPT_32BIT, "0"},
    {"ModemHistoryBufSize", (guchar **) &ModemHistoryBufSize, OPT_32BIT, "80"},
    {"MouseButtonSwap", (guchar **) &MouseButtonSwap, OPT_BOOL, "FALSE"},
    {"RB_Command", (guchar **) &RB_Command, OPT_PATH, DEFAULT_RB_COMMAND},
    {"RX_Command", (guchar **) &RX_Command, OPT_PATH, DEFAULT_RX_COMMAND},
    {"RX_InfoStr", (guchar **) &RX_InfoStr, OPT_STR, ""},
    {"RZ_Command", (guchar **) &RZ_Command, OPT_PATH, DEFAULT_RZ_COMMAND},
    {"RemoteCharset", (guchar **) &RemoteCharset, OPT_STR,
	DEFAULT_REMOTE_CHARSET},
    {"RunIconLargeSize", (guchar **) &RunIconLargeSize, OPT_32BIT, "32" },
    {"RunIconSmallSize", (guchar **) &RunIconSmallSize, OPT_32BIT, "16" },
    {"SB_Command", (guchar **) &SB_Command, OPT_PATH, DEFAULT_SB_COMMAND},
    {"SX_Command", (guchar **) &SX_Command, OPT_PATH, DEFAULT_SX_COMMAND},
    {"SZ_Command", (guchar **) &SZ_Command, OPT_PATH, DEFAULT_SZ_COMMAND},
    {"ScriptAnimFile", (guchar **) &ScriptAnimFile, OPT_PATH,
	DEFAULT_SCRIPT_ANIM_FILE},
    {"ScriptPath", (guchar **) &ScriptPath, OPT_PATH, DEFAULT_SCRIPT_PATH},
    {"SoundBeepFile", (guchar **) &SoundBeepFile, OPT_PATH, ""},
    {"SoundClickFile", (guchar **) &SoundClickFile, OPT_PATH, ""},
    {"SoundConnectFile", (guchar **) &SoundConnectFile, OPT_PATH, ""},
    {"SoundDisconnectFile", (guchar **) &SoundDisconnectFile, OPT_PATH, ""},
    {"SoundExitFile", (guchar **) &SoundExitFile, OPT_PATH, ""},
    {"SoundPath", (guchar **) &SoundPath, OPT_PATH, DEFAULT_SOUND_PATH},
    {"SoundPlayCommand", (guchar **) &SoundPlayCommand, OPT_PATH,
	DEFAULT_SOUND_PLAY_COMMAND},
    {"SoundStartFile", (guchar **) &SoundStartFile, OPT_PATH, ""},
    {"SoundTRxEndFile", (guchar **) &SoundTRxEndFile, OPT_PATH, ""},
    {"TX_InfoStr", (guchar **) &TX_InfoStr, OPT_STR, ""},
    {"TelnetCommand", (guchar **) &TelnetCommand, OPT_PATH,
	DEFAULT_TELNET_COMMAND},
    {"TermAutoLineFeed", (guchar **) &TermAutoLineFeed, OPT_BOOL, "TRUE"},
    {"TermCanBlock", (guchar **) &TermCanBlock, OPT_BOOL, "TRUE"},
    {"TermCol", (guchar **) &TermCol, OPT_32BIT, DEFAULT_COLUMN_STR},
    {"TermColor0", (guchar **) &TermColor0, OPT_COLOR,   "0 0 0"},
    {"TermColor1", (guchar **) &TermColor1, OPT_COLOR,   "205 0 0"},
    {"TermColor2", (guchar **) &TermColor2, OPT_COLOR,   "0 205 0"},
    {"TermColor3", (guchar **) &TermColor3, OPT_COLOR,   "205 205 0"},
    {"TermColor4", (guchar **) &TermColor4, OPT_COLOR,   "0 0 205"},
    {"TermColor5", (guchar **) &TermColor5, OPT_COLOR,   "205 0 205"},
    {"TermColor6", (guchar **) &TermColor6, OPT_COLOR,   "0 205 205"},
    {"TermColor7", (guchar **) &TermColor7, OPT_COLOR,   "229 229 229"},
    {"TermColor8", (guchar **) &TermColor8, OPT_COLOR,   "77 77 77"},
    {"TermColor9", (guchar **) &TermColor9, OPT_COLOR,   "255 0 0"},
    {"TermColor10", (guchar **) &TermColor10, OPT_COLOR, "0 255 0"},
    {"TermColor11", (guchar **) &TermColor11, OPT_COLOR, "255 255 0"},
    {"TermColor12", (guchar **) &TermColor12, OPT_COLOR, "0 0 255"},
    {"TermColor13", (guchar **) &TermColor13, OPT_COLOR, "255 0 255"},
    {"TermColor14", (guchar **) &TermColor14, OPT_COLOR, "0 255 255"},
    {"TermColor15", (guchar **) &TermColor15, OPT_COLOR, "255 255 255"},
    {"TermCursorBG", (guchar **) &TermCursorBG, OPT_COLOR,"255 0 0"},
    {"TermCursorFG", (guchar **) &TermCursorFG, OPT_COLOR,"255 255 255"},
    {"TermCursorBlink", (guchar **) &TermCursorBlink, OPT_BOOL, "TRUE"},
    {"TermCursorOnTime", (guchar **) &TermCursorOnTime, OPT_32BIT, "500"},
    {"TermCursorOffTime", (guchar **) &TermCursorOffTime, OPT_32BIT, "300"},
    {"TermFontName", (guchar **) &TermFontName, OPT_STR, DEFAULT_FONT},
    {"TermForce16Color", (guchar **) &TermForce16Color, OPT_BOOL, "FALSE"},
    {"TermISelFG", (guchar **) &TermISelFG, OPT_COLOR, "230 127 178"},
    {"TermNormBG", (guchar **) &TermNormBG, OPT_32BIT, "7"},
    {"TermNormFG", (guchar **) &TermNormFG, OPT_32BIT, "0"},
    {"TermReadHack", (guchar **) &TermReadHack, OPT_32BIT, "50"},
    {"TermRow", (guchar **) &TermRow, OPT_32BIT, DEFAULT_ROW_STR},
    {"TermSavedLines", (guchar **) &TermSavedLines, OPT_32BIT,
	DEFAULT_SAVED_LINES_STR},
    {"TermSilent", (guchar **) &TermSilent, OPT_BOOL, "FALSE"},
    {"TermUseBold", (guchar **) &TermUseBold, OPT_BOOL, "TRUE"},
    {"TermUseUnder", (guchar **) &TermUseUnder, OPT_BOOL, "TRUE"},
    {"TermWordSeperator", (guchar **) &TermWordSeperator, OPT_STR,
	DEFAULT_WORD_SEPERATOR},
    {"ToolBarShow", (guchar **) &ToolBarShow, OPT_BOOL, "TRUE"},
    {"ToolBarType", (guchar **) &ToolBarType, OPT_STR,
	DEFAULT_TOOLBAR_TYPE},
    {"UploadPath", (guchar **) &UploadPath, OPT_PATH, DEFAULT_UPLOAD_PATH},
    {"UseBeep", (guchar **) &UseBeep, OPT_BOOL, "FALSE"},
    {"UseHandleBox", (guchar **) &UseHandleBox, OPT_BOOL, "FALSE"},
    {"UseISel", (guchar **) &UseISel, OPT_BOOL, "TRUE"},
    {"UseSound", (guchar **) &UseSound, OPT_BOOL, "FALSE"},
    {"UseXtermCursor", (guchar **) &UseXtermCursor, OPT_BOOL, "TRUE"},
    {"UseZtelnet", (guchar **) &UseZtelnet, OPT_BOOL, "FALSE"},
};

/* ConfigChangeApply() {{{1 */
static void
ConfigChangeApply(char *target)
{
    if (target == (char *) &ControlBarShow)
	ControlBarFix();
    else if (target == (char *) &ToolBarType || target == (char *) &ToolBarShow)
	StatusShowMessage(_("Use menu instead!"));
    else if (Term)
    {
	if (target == (char *) &TermCol || target == (char *) &TermRow)
	{
	    if (TermCol > 0 && TermRow > 0)
		TermSetSize(Term, TermCol, TermRow);
	    else
		StatusShowMessage
		    (_("TermCol and TermRow should be larger than 0"));
	}
	else if (target == (char *) &MouseButtonSwap)
	{
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
	}
	else if (target == (char *) &ModemMaxHistory
		 || target == (char *) &ModemHistoryBufSize)
	    ModemHistoryNew();
    }
}

/* CreateColor() {{{1 */
static GdkColor *
CreateColor(float r, float g, float b)
{
    GdkColor *c;

    c = g_new0(GdkColor, 1);
    c->red = r * 65535.0;
    c->green = g * 65535.0;
    c->blue = b * 65535.0;

    return c;
}

/* ConfigVarChange() {{{1 */
gint
ConfigVarChange(char *var, char *val, gboolean apply)
{
    register int i;
    char *s;

    for (i = 0; i < (int) G_N_ELEMENTS(OptTable); i++)
    {
	if (!g_ascii_strcasecmp(var, OptTable[i].name))
	{
	    s = ConvertCtrlChar(val);
	    if (OptTable[i].opt & (OPT_BOOL | OPT_8BIT | OPT_16BIT | OPT_32BIT))
	    {
		long int v;

		if (OptTable[i].opt & OPT_BOOL)
		{
		    if (!g_ascii_strcasecmp(s, "TRUE"))
			*(gboolean *) OptTable[i].ptr = TRUE;
		    else if (!g_ascii_strcasecmp(s, "FALSE"))
			*(gboolean *) OptTable[i].ptr = FALSE;
		    else if (sscanf(s, "%ld", &v) == 1)
			*(gboolean *) OptTable[i].ptr = (gboolean) (v != 0);
		    else
			goto err_print;
		}
		else
		{
		    if (sscanf(s, "%ld", &v) == 1)
		    {
			if (OptTable[i].opt & OPT_8BIT)
			    *(gchar *) OptTable[i].ptr = (gchar) v;
			else if (OptTable[i].opt & OPT_16BIT)
			    *(gint16 *) OptTable[i].ptr = (gint16) v;
			else if (OptTable[i].opt & OPT_32BIT)
			    *(gint32 *) OptTable[i].ptr = (gint32) v;
			else
			    FatalExit(NULL, "Invalid Option Table");
		    }
		    else
			goto err_print;
		}
	    }
	    else if (OptTable[i].opt & OPT_STR)
	    {
		if (*(gchar **) OptTable[i].ptr)
		    g_free(*(gchar **) OptTable[i].ptr);
		if (*s)
		    *(gchar **) OptTable[i].ptr = ConvertCtrlChar(s);
		else
		    *(gchar **) OptTable[i].ptr = NULL;
	    }
	    else if (OptTable[i].opt & OPT_PATH)
	    {
		if (*(gchar **) OptTable[i].ptr)
		    g_free(*(gchar **) OptTable[i].ptr);
		if (*s)
		{
		    char *fname = ExpandPath(s);
		    *(gchar **) OptTable[i].ptr = g_strdup(fname);
		    g_free(fname);
		}
		else
		    *(gchar **) OptTable[i].ptr = NULL;
	    }
	    else if (OptTable[i].opt & OPT_COLOR)
	    {
		float r, g, b;

		if (*(gchar **) OptTable[i].ptr)
		    g_free(*(gchar **) OptTable[i].ptr);

		/* 형식은  "0.95 0.00 0.81" 이거나 "255 0 240". */
		sscanf(s, "%f%f%f", &r, &g, &b);
		if (strchr(s, '.') == NULL)
		{
		    r /= 255.0;
		    g /= 255.0;
		    b /= 255.0;
		}
		*(GdkColor **) OptTable[i].ptr = CreateColor(r, g, b);
	    }
	    else
		FatalExit(NULL, "Invalid OptTable = %d,%s", i,
			  OptTable[i].name);
	    g_free(s);
	    break;
	}
    }
    if (i >= (int) G_N_ELEMENTS(OptTable))
    {
err_print:
	g_warning(_("%s: Invalid option: %s, %s"), __FUNCTION__, var, val);
	return -1;
    }
    if (apply)
	ConfigChangeApply((char *) OptTable[i].ptr);
    return 0;
}

/* ConfigExit() {{{1 */
void
ConfigExit(void)
{
    int i;

    for (i = 0; i < (int) G_N_ELEMENTS(OptTable); i++)
    {
	if (OptTable[i].opt & (OPT_STR | OPT_PATH | OPT_COLOR))
	    if (OptTable[i].ptr && *(gchar **)OptTable[i].ptr)
		g_free(*(gchar **) OptTable[i].ptr);
    }
}

/* DoConfig() {{{1 */
void
DoConfig(void)
{
    guint i;
    FILE *fp;
    char *fname, *buf, *param[2];

    fname = ExpandPath(PC_GTK_RC_FILE);
    gtk_rc_parse(fname);
    g_free(fname);

    /* 먼저 기본값으로 초기화 */
    for (i = 0; i < G_N_ELEMENTS(OptTable); i++)
	ConfigVarChange(OptTable[i].name, OptTable[i].init, FALSE);

    /* 사용자가 원하는 값으로 바꿈 */
    fname = ExpandPath(PC_RC_FILE);
    fp = fopen(fname, "r");
    g_free(fname);
    if (fp != NULL)
    {
	while ((buf = GetLines(fp)) != NULL)
	{
	    if (!buf[0] || buf[0] == '#' || buf[0] == '\n')
		continue;

	    if (ParseLine(buf, 2, param) != 2)
		continue;

	    ConfigVarChange(param[0], param[1], FALSE);
	}
	fclose(fp);
    }
    else
	g_warning(_("%s: Cannot open resource file '%s'.\nUse default value."),
		  __FUNCTION__, PC_RC_FILE);
}

/* ResourceCheckInstall() {{{1 */
void
ResourceCheckInstall(void)
{
    char *fname, *rcDir, *rc;
    struct stat st;

    fname = ExpandPath(PC_RC_BASE);
    rcDir = g_strdup(fname);
    g_free(fname);

    fname = ExpandPath(PC_RC_FILE);
    rc = GetRealFilename(fname, NULL);
    g_free(fname);
    stat(rcDir, &st);
    if (!S_ISDIR(st.st_mode) || !rc)
    {
	g_warning(_("Unable to find '%s'. I'll try to copy rc files from '%s'"),
		  rcDir, PKGDATADIR);
	/* PC_RC_BASE directory를 만들고, pkgdatadir의 모든 것을 복사해온다 */
	if (mkdir(rcDir, 0755) == 0)
	    pc_system("cp -R %s/* %s", PKGDATADIR, rcDir);
	else
	    g_warning(_("Unable to create '%s'"), rcDir);
    }
    if (rc)
	g_free(rc);
    g_free(rcDir);
}

/* getInfoID() {{{1 */

enum {
    INFO_NAME, INFO_NUMBER, INFO_SPEED, INFO_PARITY, INFO_BITS,
    INFO_USESWF, INFO_USEHWF, INFO_LOGIN_SCRIPT, INFO_LOGOUT_SCRIPT,
    INFO_DEVICE, INFO_INIT_STRING, INFO_HOSTNAME, INFO_PORT,
    INFO_REMOTE_CHARSET, INFO_DIRECT_CONN
};

static const struct {
    const char *token;
    int id;
} idTable[] = {
    { "name", INFO_NAME },
    { "number", INFO_NUMBER },
    { "direct_connection", INFO_DIRECT_CONN },
    { "speed", INFO_SPEED },
    { "parity", INFO_PARITY },
    { "bits", INFO_BITS },
    { "soft_flow_control", INFO_USESWF },
    { "hard_flow_control", INFO_USEHWF },
    { "login_script", INFO_LOGIN_SCRIPT },
    { "logout_script", INFO_LOGOUT_SCRIPT },
    { "device", INFO_DEVICE },
    { "init_string", INFO_INIT_STRING },
    { "hostname", INFO_HOSTNAME },
    { "port", INFO_PORT },
    { "remote_charset", INFO_REMOTE_CHARSET }
};

static int
getInfoID(const char *str)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS(idTable); i++)
	if (!g_ascii_strcasecmp(str, idTable[i].token))
	    return idTable[i].id;
    return -1;
}

/* AddBookmarkMenus() {{{1 */
void
AddBookmarkMenus(GtkItemFactory *factory, void (*modemCB)(gpointer),
		 void (*telnetCB)(gpointer))
{
    FILE *fp;
#define MAX_BOOKMARK_PARAMS   11
    char *fname, *s[MAX_BOOKMARK_PARAMS], *buf, *p;
    int num;

    fname = ExpandPath(PC_BOOKMARK_FILE);
    fp = fopen(fname, "r");
    g_free(fname);
    if (fp != NULL)
    {
	gboolean beginFind, modem;
	static const char *mBegin = "begin modem";
	static const char *tBegin = "begin telnet";
	GtkItemFactoryEntry entry;

	memset(&entry, 0, sizeof(entry));

	beginFind = TRUE;
	modem = FALSE;	/* to make gcc happy */
	while ((buf = GetLines(fp)) != NULL)
	{
	    buf = SkipWhiteSpace(buf);
	    if (buf == NULL || buf[0] == '#'
		|| buf[0] == '\n' || buf[0] == '\r')
		continue;

	    /* 먼저 begin tag를 찾는다. */
	    if (beginFind)
	    {
		if (!g_ascii_strncasecmp(buf, mBegin, strlen(mBegin)))
		{
		    beginFind = FALSE;
		    modem = TRUE;
		}
		else if (!g_ascii_strncasecmp(buf, tBegin, strlen(tBegin)))
		{
		    beginFind = FALSE;
		    modem = FALSE;
		}
		else
		    continue;
	    }

	    if (modem)
	    {
		ModemInfoType *info = g_new0(ModemInfoType, 1);

		info->speed = 115200;
		info->bits = 8;
		while ((buf = GetLines(fp)) != NULL)
		{
		    buf = SkipWhiteSpace(buf);
		    if (buf == NULL || buf[0] == '#'
			|| buf[0] == '\n' || buf[0] == '\r')
			continue;

		    if (!g_ascii_strncasecmp(buf, "end", 3))
		    {
			beginFind = TRUE;
			break;
		    }

		    if ((p = strchr(buf, '=')) == NULL)
		    {
			g_free(info);
			g_error(_("Bookmarks file format changed. "
				  "Please update your bookmarks file\n"
				  "%s\n"), buf);
			return;
		    }
		    *p = ' ';

		    if (ParseLine(buf, 2, s) == 2)
		    {
			switch (getInfoID(s[0]))
			{
			    case INFO_NAME:
				info->name = g_strdup(s[1]);
				break;

			    case INFO_NUMBER:
				info->number = g_strdup(s[1]);
				break;

			    case INFO_DIRECT_CONN:
				if (sscanf(s[1], "%d", &num) != 1
				    || (num != 0 && num != 1))
				{
				    g_warning(_("Invalid HWF = %s. "
						"Use 0 instead."), s[1]);
				    num = 0;
				}
				info->directConn = (num != 0);
				break;

			    case INFO_SPEED:
				if (sscanf(s[1], "%lu",
					   (unsigned long *) &info->speed) != 1
				    || (info->speed != 300
					&& info->speed != 600
					&& info->speed != 1200
					&& info->speed != 2400
					&& info->speed != 4800
					&& info->speed != 9600
					&& info->speed != 19200
					&& info->speed != 38400
					&& info->speed != 57600
					&& info->speed != 115200))
				{
				    g_warning(_("Unknown speed = %s. "
						"Use 115200 instead."),
					      s[1]);
				    info->speed = 115200;
				}
				break;

			    case INFO_PARITY:
				info->parity = g_strdup(s[1]);
				break;

			    case INFO_BITS:
				if (sscanf(s[1], "%u", &info->bits) != 1
				    || (info->bits != 6 && info->bits != 7
					&& info->bits != 8))
				{
				    g_warning(_("Invalid bits = %s. "
						"Use 8 instead."), s[1]);
				    info->bits = 8;
				}
				break;

			    case INFO_USESWF:
				if (sscanf(s[1], "%d", &num) != 1
				    || (num != 0 && num != 1))
				{
				    g_warning(_("Invalid SWF = %s. "
						"Use 0 instead."), s[1]);
				    num = 0;
				}
				info->useSWF = (num != 0);
				break;

			    case INFO_USEHWF:
				if (sscanf(s[1], "%d", &num) != 1
				    || (num != 0 && num != 1))
				{
				    g_warning(_("Invalid HWF = %s. "
						"Use 0 instead."), s[1]);
				    num = 0;
				}
				info->useHWF = (num != 0);
				break;

			    case INFO_LOGIN_SCRIPT:
				if (g_ascii_strcasecmp(s[1], "NONE"))
				    info->loginScript = g_strdup(s[1]);
				break;

			    case INFO_LOGOUT_SCRIPT:
				if (g_ascii_strcasecmp(s[1], "NONE"))
				    info->logoutScript = g_strdup(s[1]);
				break;

			    case INFO_DEVICE:
				info->device = g_strdup(s[1]);
				break;

			    case INFO_INIT_STRING:
				info->initString = ConvertCtrlChar(s[1]);
				break;

			    case INFO_REMOTE_CHARSET:
				info->remoteCharset = g_strdup(s[1]);
				break;

			    default:
				g_warning("Unknown token '%s'", s[0]);
				break;
			}
		    }
		}

		if (info->name == NULL
		    || (info->number == NULL && info->directConn == FALSE)
		    || info->device == NULL)
		{
		    g_warning("There's no name or number or device field");
		    g_free(info);
		    continue;
		}

		ModemInfoList = g_slist_append(ModemInfoList, info);

		{
		    static gboolean a = FALSE;
		    if (a == FALSE)
		    {
			entry.path = N_("/_Bookmarks/_Modem");
			entry.item_type = "<Branch>";
			entry.callback = NULL;
			gtk_item_factory_create_item(factory, &entry,
						     "", 1);
			a = TRUE;
		    }
		}

		/* add to bookmark menu */
		p = PC_IconvStr(info->name, -1);
		entry.path = g_strconcat("/_Bookmarks/_Modem/",
					 p, NULL);
		PC_IconvStrFree(info->name, p);
		entry.item_type = "";
		entry.callback = modemCB;
		gtk_item_factory_create_item(factory, &entry, info->name,
					     1);
		g_free(entry.path);
	    }
	    else
	    {
		TelnetInfoType *info = g_new0(TelnetInfoType, 1);

		while ((buf = GetLines(fp)) != NULL)
		{
		    buf = SkipWhiteSpace(buf);
		    if (buf == NULL || buf[0] == '#'
			|| buf[0] == '\n' || buf[0] == '\r')
			continue;

		    if (!g_ascii_strncasecmp(buf, "end", 3))
		    {
			beginFind = TRUE;
			break;
		    }

		    if ((p = strchr(buf, '=')) == NULL)
		    {
			g_free(info);
			g_error(_("Bookmarks file format changed. "
				  "Please update your bookmarks file\n"
				  "%s\n"), buf);
			return;
		    }
		    *p = ' ';

		    if (ParseLine(buf, 2, s) == 2)
		    {
			switch (getInfoID(s[0]))
			{
			    case INFO_NAME:
				info->name = g_strdup(s[1]);
				break;

			    case INFO_HOSTNAME:
				info->host = g_strdup(s[1]);
				break;

			    case INFO_PORT:
				info->port = g_strdup(s[1]);
				break;

			    case INFO_LOGIN_SCRIPT:
				if (g_ascii_strcasecmp(s[1], "NONE"))
				    info->loginScript = g_strdup(s[1]);
				break;

			    case INFO_LOGOUT_SCRIPT:
				if (g_ascii_strcasecmp(s[1], "NONE"))
				    info->logoutScript = g_strdup(s[1]);
				break;

			    case INFO_REMOTE_CHARSET:
				info->remoteCharset = g_strdup(s[1]);
				break;

			    default:
				g_warning("Unknown token '%s'", s[0]);
				break;
			}
		    }
		}

		if (info->name == NULL || info->host == NULL
		    || info->port == NULL)
		{
		    g_warning("There's no name or hostname or port field");
		    g_free(info);
		    continue;
		}

		TelnetInfoList = g_slist_append(TelnetInfoList, info);

		{
		    static gboolean a = FALSE;
		    if (a == FALSE)
		    {
			entry.path = N_("/_Bookmarks/_Telnet");
			entry.item_type = "<Branch>";
			entry.callback = NULL;
			gtk_item_factory_create_item(factory, &entry,
						     "", 1);
			a = TRUE;
		    }
		}

		/* add to bookmark menu */
		p = PC_IconvStr(info->name, -1);
		entry.path = g_strconcat("/_Bookmarks/_Telnet/",
					 p, NULL);
		PC_IconvStrFree(info->name, p);
		entry.item_type = "";
		entry.callback = telnetCB;
		gtk_item_factory_create_item(factory, &entry, info->name,
					     1);
		g_free(entry.path);
	    }
	}

	if (ModemInfoList || TelnetInfoList)
	{
	    entry.path = "/_Bookmarks/---";
	    entry.item_type = "<Separator>";
	    entry.callback = NULL;
	    gtk_item_factory_create_item(factory, &entry, NULL, 1);
	}

	fclose(fp);
    }
}
