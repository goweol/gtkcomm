/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Utilities
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
#include <ctype.h>	/* isspace() */
#include <sys/stat.h>	/* stat() */
#include <fcntl.h>	/* R_OK */

#include "pcMain.h"
#include "pcTerm.h"

/* Global variables/functions {{{1 */

gboolean UseSound;
gboolean UseBeep;
gint32 BeepPercent;
char *SoundPath;
char *SoundPlayCommand;
char *SoundStartFile;
char *SoundExitFile;
char *SoundConnectFile;
char *SoundDisconnectFile;
char *SoundTRxEndFile;
char *SoundClickFile;
char *SoundBeepFile;

/* Local variables {{{1 */

static GtkWidget *GeneralInputWin;
static GtkWidget *GeneralInputEntry;

/* pc_system() {{{1 */
int
pc_system(const char *fmt, ...)
{
    int r;
    va_list args;
    gchar *buf;

    va_start(args, fmt);
    buf = g_strdup_vprintf(fmt, args);
    va_end(args);

    if ((r = system(buf)) != 0)
	g_warning("external cmd '%s' run error (%d)\n", buf, r);

    g_free(buf);
    return r;
}

/* ExpandPath() {{{1 */
char *
ExpandPath(const char *str)
{
    char *p = NULL;

    if (str)
    {
	if (str[0] == '~' && str[1] == '/')
	    p = g_strdup_printf("%s%s", g_get_home_dir(), str + 1);
	else
	    p = g_strdup(str);
    }
    return p;
}

/* GetRealFilename() {{{1 */
char *
GetRealFilename(const char *file, const char *path)
{
    char *fname, *s;
    struct stat st;

    if (file && *file)
    {
	fname = ExpandPath(file);
	if (!stat(fname, &st) && S_ISREG(st.st_mode) && !access(fname, R_OK))
	{
	    return fname;
	}
	else if (*fname != '/' && path && *path)
	{
	    s = g_strconcat(path, "/", fname, NULL);
	    g_free(fname);
	    if (!stat(s, &st) && S_ISREG(st.st_mode) && !access(s, R_OK))
		return (char *) s;
	    g_free(s);
	}
	else
	    g_free(fname);
    }
    return NULL;
}

/* ToOctalNum() {{{1 */
static guint
ToOctalNum(const guchar *buf)
{
    guint u;

    if (buf[0] && buf[1])
    {
	u = (*buf - '0') * 8 * 8;
	if (buf[1] >= '0' && buf[1] <= '7')
	{
	    u += (buf[1] - '0') * 8;
	    if (buf[2] >= '0' && buf[2] <= '7')
	    {
		u += (buf[2] - '0');
		return u;
	    }
	}
    }
    return (guint) -1;
}

/* ToHexNum() {{{1 */
static guint
ToHexNum(const guchar *buf)
{
    guint u;
    guchar b;

    if (buf[0] && buf[1])
    {
	if (isxdigit(buf[0]) && isxdigit(buf[1]))
	{
	    b = tolower(buf[0]);
	    if (b >= 'a' && b <= 'f')
		u = (b - 'a' + 10) * 16;
	    else
		u = (b - '0') * 16;
	    b = tolower(buf[1]);
	    if (b >= 'a' && b <= 'f')
		u += (b - 'a') * 10;
	    else
		u += (b - '0');
	    return u;
	}
    }
    return (guint) -1;
}

/* ConvertCtrlChar() {{{1 */
char *
ConvertCtrlChar(const char *buf)
{
    register int i;
    guchar *dest, *str;
    guint u = 0;	/* to make gcc happy */

    /* control character들을 conversion하면 당연히 원래 문자열보다 작아지므로
     * 원래 문자열만큼 memory를 잡으면 충분하다. 이것은 약간의 memory 낭비가 될
     * 수도 있다.
     */
    i = strlen(buf) + 1;
    dest = g_new(guchar, i);
    str = dest;	/* store return pointer */
    for (i = 0; buf[i]; i++, dest++)
    {
	if (buf[i] == '\\' && buf[i + 1])
	{
	    ++i;
	    switch (buf[i])
	    {
		case 'n':
		    *dest = '\n';
		    break;
		case 'r':
		    *dest = '\r';
		    break;
		case 't':
		    *dest = '\t';
		    break;
		case 'b':
		    *dest = '\b';
		    break;
		case '0':
		case '1':
		case '2':
		case '3':
		    u = ToOctalNum((guchar*) &buf[i]);
		    /* fall through */
		case 'x':
		    if (buf[i] == 'x')
			u = ToHexNum((guchar*) &buf[i + 1]);
		    if (u < 256)
		    {
			i += 2;
			*dest = (guchar) u;
		    }
		    else
			*dest = buf[i];
		    break;
		default:
		    *dest = buf[i];
		    break;
	    }
	}
	else if (buf[i] == '^' && buf[i + 1])
	{
	    ++i;
	    *dest = buf[i] + '\b' - 'H';
	}
	else
	    *dest = buf[i];
    }
    *dest = '\0';

    return (char*) str;
}

/* FindWordSeperator() {{{1 */
char *
FindWordSeperator(const char *s)
{
    if (s)
    {
	for (; *s; s++)
	    if (!isalnum(*s) && *s != '_')
		return (char *)s;
    }
    return NULL;
}

/* FindWhiteSpace() {{{1 */
char *
FindWhiteSpace(const char *s)
{
    if (s && *s)
    {
	for (; *s; s++)
	    if (isspace(*s))
		return (char *)s;
    }
    return NULL;
}

/* SkipWhiteSpace() {{{1 */
char *
SkipWhiteSpace(const char *s)
{
    if (s && *s)
    {
	for (; *s; s++)
	    if (!isspace(*s))
		return (char *)s;
    }
    return NULL;
}

/* FindChar() {{{1 */
char *
FindChar(const char *s, char c)
{
    const char *p;

    if (s)
    {
	for (p = s; *s; s++)
	{
	    if (*s == c)
	    {
		if (s == p || s[-1] != '\\' || (s > p + 1 && s[-2] == '\\'))
		    return (char *)s;
	    }
	}
    }
    return NULL;
}

/* SkipTrailWhiteSpace() {{{1 */
/* buffer의 공백이 아닌 마지막 문자 포인터를 되돌려준다.
 * NOTE: parameter 's'는 string의 마지막이라고 가정한다 (NUL 제외).
 */
char *
SkipTrailWhiteSpace(const char *s)
{
    if (s)
    {
	for (; *s; s--)
	    if (!isspace(*s))
		return (char *)s;
    }
    return NULL;
}

/* ParseLine() {{{1 */
/* 공백문자로 나누이진 파라미터들을 dest에 분할해서 할당.
 *
 * NOTE: dest[]에는 NUL 로 구분된 buf pointer가 할당된다. 따라서, 계속적으로
 * 사용되어야 할 경우에는 아마도 duplicate를 할 필요가 있을 것이다.
 */
int
ParseLine(char *buf, int n, char *dest[])
{
    register int i;
    char *s, *last;

    if (buf == NULL || dest == NULL)
	return 0;

    last = buf + strlen(buf) + 1;
    if ((buf = SkipWhiteSpace(buf)) == NULL)
	return 0;

    for (i = 0; i < n;)
    {
	if (buf[0] == '#' || buf[0] == ';')	/* comment */
	    break;
	if (buf[0] == '"' || buf[0] == '\'')
	{
	    ++buf;
	    if ((s = FindChar(buf, buf[-1])) == NULL)
		break;
	}
	else
	    s = FindWhiteSpace(buf);

	dest[i] = buf;
	++i;
	if (!s || !*s)
	    break;
	*s = '\0';
	if ((buf = SkipWhiteSpace(s + 1)) == NULL)
	    break;
    }
    return i;
}

/* GetLines() {{{1 */
char *
GetLines(FILE * fp)
{
    register int lineLen, totalLen;
    register gboolean contFlag;	/* line이 backslash로 끝났는가? */
    char *p;
    static char *buf = NULL;
    static int bufSize;

    if (buf == NULL)
    {
	bufSize = 128;
	buf = g_new(char, bufSize);
    }

    contFlag = FALSE;
    totalLen = 0;
    while (fgets(buf + totalLen, bufSize - totalLen - 1, fp) != NULL)
    {
	p = SkipWhiteSpace(buf + totalLen);
	if (!p || *p == '#' || *p == ';')
	{
	    if (contFlag)
	    {
		g_warning(_("%s: parsing error. empty line after '\\'"),
			  __FUNCTION__);
		totalLen = 0;	/* Don't read any-more. return NULL */
		break;
	    }
	    continue;	/* empty line */
	}

	if (p > (buf + totalLen))
	    memmove(buf + totalLen, p, strlen(p) + 1);

	contFlag = FALSE;
	lineLen = strlen(buf + totalLen);
	totalLen += lineLen;
	if (buf[totalLen - 1] != '\n' && buf[totalLen - 1] != '\r')
	{
	    /* line을 다 읽지 못했다? 그럼 버퍼 크기를 늘려서 더 읽어야쥐! */
	    bufSize += 128;
	    buf = g_realloc(buf, bufSize);
	}
	else
	{
	    p = SkipTrailWhiteSpace(buf + totalLen - 1);
	    if (!p)
	    {
		FatalExit(NULL, "%s: FIX-ME! my brain was damaged.",
			  __FUNCTION__);
		break;	/* should never reach */
	    }
	    p[1] = '\0';

	    /* adjust total length */
	    totalLen -= buf + totalLen - p;

	    if (*p == '\\' && p[-1] != '\\')
	    {
		/* backslash로 끝났으므로 다음 line에 계속 */
		contFlag = TRUE;

		/* backslash를 공백으로 바꾸어서 다음 라인과의 사이에 공백을 둠 */
		*p = ' ';

		continue;
	    }
	    else
		break;	/* 한줄를 다 읽었다. */
	}
    }
    if (totalLen > 0)
	return buf;
    else
	return NULL;
}

/* FindSubString() {{{1 */
/* NUL로 끝나지 않는 문자열에서 sub-string을 찾는다. */
char *
FindSubString(const char *buf, int len, const char *patt, int patLen)
{
    register int i, j;

    for (i = 0; i < len; i++)
    {
	for (j = 0; j < patLen; j++)
	    if (buf[i + j] != patt[j])
		break;
	if (j == patLen)
	    return (char *) (buf + i);
    }
    return NULL;
}

/* IsFirstMB() {{{1 */
/* 한글의 첫번째 바이트인가? */
gboolean
IsFirstMB(const char *buf, int x)
{
    int xx;

    if (x >= 0)
    {
	xx = x;
	if (buf[x] & 0x80)
	{
	    do
		x--;
	    while (x >= 0 && (buf[x] & 0x80));

	    if ((xx - x) % 2 == 1)
		return TRUE;
	}
    }
    return FALSE;
}

/* IsSecondMB() {{{1 */
/* 한글의 두번째 바이트인가? */
gboolean
IsSecondMB(const char *buf, int x)
{
    int xx;

    if (x > 0)
    {
	xx = x;
	if (buf[x] & 0x80)
	{
	    do
		x--;
	    while (x >= 0 && (buf[x] & 0x80));

	    if ((xx - x) % 2 == 0)
		return TRUE;
	}
    }
    return FALSE;
}

/* GeneralInputOk() {{{1 */
static void
GeneralInputOk(GtkWidget *w, void (*okFunc)(const char *))
{
    const char *s;

    s = gtk_entry_get_text(GTK_ENTRY(GeneralInputEntry));
    if (*s)
    {
	okFunc(s);
	gtk_widget_destroy(GeneralInputWin);
    }

    w = NULL;	/* to make gcc happy */
}

/* GeneralInputCancel() {{{1 */
static void
GeneralInputCancel(GtkWidget *w, void (*cancelFunc)(void))
{
    UNUSED(w);

    if (cancelFunc)
	cancelFunc();
    gtk_widget_destroy(GeneralInputWin);
}

/* GeneralInputQuery() {{{1 */
void
GeneralInputQuery(const char *title, const char *labelName,
		  const char *contents, void (*okFunc)(char *),
		  void (*cancelFunc)(void))
{
    GtkWidget *vbox, *hbox, *label, *button, *sep;

    if (!okFunc)
	return;

    GeneralInputWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(GeneralInputWin), 10);
    if (title)
	gtk_window_set_title(GTK_WINDOW(GeneralInputWin), title);
    gtk_window_set_position(GTK_WINDOW(GeneralInputWin), GTK_WIN_POS_MOUSE);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(GeneralInputWin), vbox);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    if (labelName)
    {
	label = gtk_label_new(labelName);
	gtk_container_add(GTK_CONTAINER(hbox), label);
    }

    GeneralInputEntry = gtk_entry_new();
    if (contents && *contents)
    {
	int width;

	gtk_entry_set_text(GTK_ENTRY(GeneralInputEntry), contents);
	gtk_editable_select_region(GTK_EDITABLE(GeneralInputEntry),
				   0, strlen(contents));

	if ((width = strlen(contents) + 2) > 80)
	    width = 80;
	width *= Term->fontWidth;
	gtk_widget_set_size_request(GeneralInputEntry, width, -1);
    }
    gtk_container_add(GTK_CONTAINER(hbox), GeneralInputEntry);
    g_signal_connect(G_OBJECT(GeneralInputEntry), "activate",
		     G_CALLBACK(GeneralInputOk), okFunc);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(GeneralInputOk), okFunc);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_widget_grab_default(button);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(GeneralInputCancel), cancelFunc);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

    gtk_widget_show_all(GeneralInputWin);
    gtk_widget_grab_focus(GeneralInputEntry);
}

/* util_file_query() {{{1 */
void
util_file_query(const char *title, const char *filename,
		void (*ok_func)(const char *))
{
    GtkWidget *w;
    GtkFileChooserAction act;
    static char *prev_dir;
    const char *folder = prev_dir? prev_dir: ScriptPath;

    if (filename)
	act = GTK_FILE_CHOOSER_ACTION_SAVE;
    else
	act = GTK_FILE_CHOOSER_ACTION_OPEN;
    w = gtk_file_chooser_dialog_new(title,
				    GTK_WINDOW(MainWin),
				    act,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				    NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w), folder);
    if (filename)
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(w), filename);

    if (gtk_dialog_run(GTK_DIALOG(w)) == GTK_RESPONSE_ACCEPT)
    {
	char *p, *scriptname;

	scriptname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w));
	if ((p = strrchr(scriptname, '/')) != NULL)
	{
	    if (prev_dir)
		g_free(prev_dir);
	    prev_dir = g_strdup(scriptname);
	    prev_dir[(int) (p - scriptname)] = '\0';
	}
	ok_func(scriptname);
	g_free(scriptname);
    }
    gtk_widget_destroy(w);
}

/* SleepCB() {{{1 */
static gint
SleepCB(void)
{
    gtk_main_quit();
    return FALSE;
}

/* PC_Sleep() {{{1 */
void
PC_Sleep(guint32 msec)
{
    gtk_timeout_add(msec, (GtkFunction) SleepCB, NULL);
    gtk_main();
}

/* SoundPlay() {{{1 */
int
SoundPlay(const char *soundFile)
{
    char *cmd, *file;
    pid_t pid;

    if (UseSound && SoundPlayCommand && *SoundPlayCommand)
    {
	if ((file = GetRealFilename(soundFile, SoundPath)) != NULL)
	{
	    cmd = g_strconcat(SoundPlayCommand, " ", file, NULL);
	    g_free(file);
	    pid = fork();
	    if (pid == 0)
	    {
		int fd = open("/dev/null", O_WRONLY);
		dup2(fd, 1);	/* all stdout print goes to /dev/null */
		/* NOTE: esdplay로 enlightenment의 estart.wav를 돌릴 경우
		 *   Audio File Library: error 5: Unix read fail...
		 * 과 같은 warning이 뜨는 데, 그것이 보기 싫어서 stderr도
		 * 막았음. sound file 혹은 esdplay에 문제가 있어보임.
		 * 그냥 command line에서 실행해도 마찬가지이므로 gtkcomm하고는
		 * 관계없음.
		 */
		dup2(fd, 2);	/* all stderr print goes to /dev/null */

		execlp("sh", "sh", "-c", cmd, NULL);
		g_warning(_("%s: fail to run '%s'"), __FUNCTION__, cmd);
		_exit(1);
	    }
	    g_free(cmd);
	    return 0;
	}
    }
    if (UseBeep)
    {
	GdkDisplay *display = gdk_display_get_default();

	XBell(GDK_DISPLAY_XDISPLAY(display), (int) BeepPercent);
	return 0;
    }
    return -1;
}
/* }}} */

#if !defined(__GNUC__) || __GNUC__ < 2
/* OptPrintf() {{{1 */
void
OptPrintf(int opt, const char *form, ...)
{
#if defined(_DEBUG_)
    if ((unsigned) opt < MAX_OPRT_NUM && OPrintFlag[opt])
    {
	va_list args;
	gchar *buf;

	va_start(args, form);
	buf = g_strdup_vprintf(form, args);
	va_end(args);

	g_print(buf);
	g_free(buf);
    }
#else
    opt = 0;
    form = NULL;
#endif
}
/* }}} */
#endif /* !__GNUC__ */
