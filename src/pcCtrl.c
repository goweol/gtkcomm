/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Control bar
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
#include <ctype.h>
#include <time.h>	/* time(), localtime() */
#include <fcntl.h>	/* open() */

#include "pcMain.h"
#include "pcTerm.h"
#include <gdk/gdkx.h>

/* Global variables/functions {{{1 */

guint32 MaxCaptureSize; /* maximum capture file size */
char *CapturePath;
char *CaptureFile;
char *LogFile;
unsigned int log_flags;

/* Local variables {{{1 */

static GtkWidget *BaudFrame = NULL, *BaudLabel = NULL;
static GtkWidget *EmulFrame = NULL, *EmulLabel = NULL;
static GtkWidget *ProtFrame = NULL, *ProtLabel = NULL;

/* pcMain.h의 emulate enum과 순서가 같아야 한다. */
const char *EmulateNames[] = { "ANSI", "VT100" };

/* ControlBaudrateChange() {{{1 */
void
ControlBaudrateChange(guint baud)
{
    char buf[9];

    if (BaudLabel)
    {
	g_snprintf(buf, sizeof(buf), " %6d ", baud);
	gtk_label_set_text(GTK_LABEL(BaudLabel), buf);
    }
}

/* ControlProtocolChange() {{{1 */
void
ControlProtocolChange(void)
{
    char buf[9];

    if (ProtLabel)
    {
	g_snprintf(buf, sizeof(buf), " %6s ", TRxProtocolNames[TRxProtocol]);
	gtk_label_set_text(GTK_LABEL(ProtLabel), buf);
    }
}

/* ControlEmulateChange() {{{1 */
void
ControlEmulateChange(void)
{
    char buf[8];

    if (EmulLabel)
    {
	g_snprintf(buf, sizeof(buf), " %5s ", EmulateNames[EmulateMode]);
	gtk_label_set_text(GTK_LABEL(EmulLabel), buf);
    }
}

/* CreateControlLabel() {{{1 */
static GtkWidget *
CreateControlLabel(char *label, GtkWidget **labelWin)
{
    GtkWidget *frame;
    char *p;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_end(GTK_BOX(CtrlBox), frame, FALSE, FALSE, 0);

    p = PC_IconvStr(label, -1);
    *labelWin = gtk_label_new(p);
    PC_IconvStrFree(label, p);

    gtk_container_add(GTK_CONTAINER(frame), *labelWin);
    gtk_widget_show_all(frame);
    return frame;
}

/* ControlLabelBaudCtrl() {{{1 */
int
ControlLabelBaudCtrl(gboolean create)
{
    char buf[10];

    if (create)
    {
	if (!BaudFrame)
	{
	    g_snprintf(buf, sizeof(buf), " %6d ", ModemGetSpeed());
	    BaudFrame = CreateControlLabel(buf, &BaudLabel);
	    return 0;
	}
    }
    else
    {
	if (BaudFrame)
	{
	    gtk_widget_destroy(BaudFrame);
	    BaudLabel = BaudFrame = NULL;
	    return 0;
	}
    }
    return -1;
}

/* ControlLabelEmulCtrl() {{{1 */
int
ControlLabelEmulCtrl(gboolean create)
{
    char buf[10];

    if (create)
    {
	if (!EmulFrame)
	{
	    g_snprintf(buf, sizeof(buf), " %5s ", EmulateNames[EmulateMode]);
	    EmulFrame = CreateControlLabel(buf, &EmulLabel);
	    return 0;
	}
    }
    else
    {
	if (EmulFrame)
	{
	    gtk_widget_destroy(EmulFrame);
	    EmulLabel = EmulFrame = NULL;
	    return 0;
	}
    }
    return -1;
}

/* ControlLabelProtCtrl() {{{1 */
int
ControlLabelProtCtrl(gboolean create)
{
    char buf[10];

    if (create)
    {
	if (!ProtFrame)
	{
	    g_snprintf(buf, sizeof(buf), " %5s ",
		       TRxProtocolNames[TRxProtocol]);
	    ProtFrame = CreateControlLabel(buf, &ProtLabel);
	    return 0;
	}
    }
    else
    {
	if (ProtFrame)
	{
	    gtk_widget_destroy(ProtFrame);
	    ProtLabel = ProtFrame = NULL;
	    return 0;
	}
    }
    return -1;
}
/* }}} */

/****************************************************************************
		     C O N T R O L   L I N E   S T A T U S
****************************************************************************/
#include "pixmaps/yellow.xpm"
#include "pixmaps/red.xpm"
#include "pixmaps/green.xpm"

#define LED_ON_INTERVAL 100

static GtkWidget *ctrlLineSts;
static GtkWidget *ctrlRxBox, *ctrlTxBox;
static GtkWidget *ctrlRxOffWid, *ctrlTxOffWid, *ctrlTxWid, *ctrlRxWid;
static int ctrlRxTid, ctrlTxTid;
static GMutex *ctrlMutex;

/* ControlLabelLineStsCtrl() {{{1 */
int
ControlLabelLineStsCtrl(gboolean create)
{
    GtkWidget *label, *box;
    GdkPixbuf *pixYellow, *pixRed, *pixGreen;

    if (ctrlMutex == NULL)
    {
	static GMutex mtx;

	g_mutex_init(&mtx);
	ctrlMutex = &mtx;
    }

    if (ctrlLineSts)
    {
	if (create == FALSE)
	{
	    g_mutex_lock(ctrlMutex);
	    gtk_widget_destroy(ctrlLineSts);
	    ctrlLineSts = NULL;
	    ctrlRxTid = -1;
	    ctrlTxTid = -1;
	    g_mutex_unlock(ctrlMutex);
	    return 0;
	}
	return -1;
    }

    ctrlRxTid = -1;
    ctrlTxTid = -1;

    pixYellow = gdk_pixbuf_new_from_xpm_data((const char **)yellow_xpm);
    pixRed = gdk_pixbuf_new_from_xpm_data((const char **)red_xpm);
    pixGreen = gdk_pixbuf_new_from_xpm_data((const char **)green_xpm);
    if (pixYellow == NULL || pixRed == NULL || pixGreen == NULL)
    {
	g_warning(_("%s: Cannot create pixmaps\n"), __FUNCTION__);
	return -1;
    }
    ctrlRxWid = gtk_image_new_from_pixbuf(pixGreen);
    ctrlTxWid = gtk_image_new_from_pixbuf(pixRed);
    gtk_widget_show(ctrlRxWid);
    gtk_widget_show(ctrlTxWid);

    ctrlLineSts = gtk_frame_new(0);
    gtk_frame_set_shadow_type(GTK_FRAME(ctrlLineSts), GTK_SHADOW_IN);
    gtk_box_pack_end(GTK_BOX(CtrlBox), ctrlLineSts, FALSE, FALSE, 0);

    box = gtk_hbox_new(0, 0);
    gtk_container_add(GTK_CONTAINER(ctrlLineSts), box);

    ctrlRxBox = gtk_hbox_new(0, 0);
    gtk_box_pack_end(GTK_BOX(box), ctrlRxBox, FALSE, FALSE, 0);

    ctrlRxOffWid = gtk_image_new_from_pixbuf(pixYellow);
    gtk_box_pack_start(GTK_BOX(ctrlRxBox), ctrlRxOffWid, FALSE, FALSE, 5);
    label = gtk_label_new("R");
    gtk_box_pack_end(GTK_BOX(ctrlRxBox), label, FALSE, FALSE, 5);

    ctrlTxBox = gtk_hbox_new(0, 0);
    gtk_box_pack_end(GTK_BOX(box), ctrlTxBox, FALSE, FALSE, 0);

    ctrlTxOffWid = gtk_image_new_from_pixbuf(pixYellow);
    gtk_box_pack_start(GTK_BOX(ctrlTxBox), ctrlTxOffWid, FALSE, FALSE, 5);
    label = gtk_label_new("T");
    gtk_box_pack_end(GTK_BOX(ctrlTxBox), label, FALSE, FALSE, 5);

    gtk_widget_show_all(ctrlLineSts);
    return 0;
}

/* ctrlRxToggleButtonOff() {{{1 */
static int
ctrlRxToggleButtonOff(void)
{
    g_mutex_lock(ctrlMutex);

    if (ctrlLineSts)
    {
	gtk_widget_ref(ctrlRxWid);
	gtk_container_remove(GTK_CONTAINER(ctrlRxBox), ctrlRxWid);
	gtk_box_pack_start(GTK_BOX(ctrlRxBox), ctrlRxOffWid, FALSE,
			   FALSE, 5);
	ctrlRxTid = -1;
    }

    g_mutex_unlock(ctrlMutex);
    return FALSE;
}

/* ctrlTxToggleButtonOff() {{{1 */
static int
ctrlTxToggleButtonOff(void)
{
    g_mutex_lock(ctrlMutex);

    if (ctrlLineSts)
    {
	gtk_widget_ref(ctrlTxWid);
	gtk_container_remove(GTK_CONTAINER(ctrlTxBox), ctrlTxWid);
	gtk_box_pack_start(GTK_BOX(ctrlTxBox), ctrlTxOffWid, FALSE,
			   FALSE, 5);
	ctrlTxTid = -1;
    }

    g_mutex_unlock(ctrlMutex);
    return FALSE;
}

/* ControlLabelLineStsCtrlToggle() {{{1 */
void
ControlLabelLineStsToggle(int id)
{
    if (ctrlMutex == NULL)
	return;

    g_mutex_lock(ctrlMutex);

    if (ctrlLineSts)
    {
	if (id == 'r')
	{
	    if (ctrlRxTid < 0)
	    {
		gtk_widget_ref(ctrlRxOffWid);
		gtk_container_remove(GTK_CONTAINER(ctrlRxBox), ctrlRxOffWid);
		gtk_box_pack_start(GTK_BOX(ctrlRxBox), ctrlRxWid, FALSE,
				   FALSE, 5);
		ctrlRxTid = gtk_timeout_add(LED_ON_INTERVAL,
					    (GtkFunction) ctrlRxToggleButtonOff,
					    NULL);
	    }
	}
	else if (id == 't')
	{
	    if (ctrlTxTid < 0)
	    {
		gtk_widget_ref(ctrlTxOffWid);
		gtk_container_remove(GTK_CONTAINER(ctrlTxBox), ctrlTxOffWid);
		gtk_box_pack_start(GTK_BOX(ctrlTxBox), ctrlTxWid, FALSE,
				   FALSE, 5);

		ctrlTxTid = gtk_timeout_add(LED_ON_INTERVAL,
					    (GtkFunction) ctrlTxToggleButtonOff,
					    NULL);
	    }
	}
    }
    g_mutex_unlock(ctrlMutex);
}
/* }}} */

/****************************************************************************
			    C O N T R O L   M E N U
****************************************************************************/
/* CtrlMenuListType {{{1 */
typedef struct {
    gboolean global;
    char *title;
    GtkWidget *menu;
    GtkWidget *menuBar;
    int entryNum;
} CtrlMenuListType;

static GSList *CtrlMenuList = NULL;
static gboolean CtrlMenuRemoved = TRUE;

/* RunControlMenuString() {{{1 */
static void
RunControlMenuString(GtkWidget *w, char *string)
{
    Term->remoteWrite((guchar*) string, -1);
    SoundPlay(SoundClickFile);
    w = NULL;
}

/* RunControlMenuScript() {{{1 */
static void
RunControlMenuScript(GtkWidget *w, char *action)
{
    ScriptRunFromString(action);
    SoundPlay(SoundClickFile);
    w = NULL;
}

/* RunControlMenuScriptFile() {{{1 */
static void
RunControlMenuScriptFile(GtkWidget *w, char *action)
{
    ScriptRunFromFile(action);
    SoundPlay(SoundClickFile);
    w = NULL;
}

/* CtrlMenuAddEntry() {{{1 */
static void
CtrlMenuAddEntry(GtkWidget *menu, const char *name, GCallback func,
		 gpointer data)
{
    GtkWidget *menuItem;

    menuItem = gtk_menu_item_new_with_label(name);
    g_signal_connect(G_OBJECT(menuItem), "activate", func, data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
    gtk_widget_show(menuItem);
}

/* CreateCtrlMenu() {{{1 */
static void
CreateCtrlMenu(CtrlMenuListType *cml, const char *title,
	       const char *name, GCallback func, gpointer data)
{
    GtkWidget *menuBar, *menu, *menuItem, *hbox, *arrow, *label;

    menuBar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(CtrlBox), menuBar, FALSE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(menuBar), 0);
    gtk_widget_show(menuBar);

    menu = gtk_menu_new();

    CtrlMenuAddEntry(menu, name, func, data);

    menuItem = gtk_menu_item_new();
    hbox = gtk_hbox_new(FALSE, 1);
    arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
    label = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
    gtk_widget_show(arrow);
    gtk_widget_show(label);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(menuItem), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(menuItem), 0);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), menuItem);
    gtk_widget_show(menuItem);

    cml->menu = menu;
    cml->menuBar = menuBar;
}

/* ControlMenuExit() {{{1 */
void
ControlMenuExit(void)
{
    GSList *list;
    CtrlMenuListType *cml;

    if (!CtrlMenuRemoved)
    {
	CtrlMenuRemoved = TRUE;
	DisconnectFuncList = g_slist_remove(DisconnectFuncList,
					    ControlMenuExit);
    }

    for (list = CtrlMenuList; list;)
    {
	cml = list->data;
	list = list->next;
	if (!cml->global)
	{
	    gtk_widget_destroy(cml->menuBar);
	    CtrlMenuList = g_slist_remove(CtrlMenuList, cml);
	    g_free(cml->title);
	    g_free(cml);
	}
    }
}

/* ControlBarAddMenu() {{{1 */
void
ControlBarAddMenu(gboolean globalf, const char *title, const char *name,
		  const char *type, const char *action)
{
    GSList *list;
    CtrlMenuListType *cml = NULL;
    GCallback func;
    gpointer data;
    char *n_title, *n_name;

    n_title = PC_IconvStr(title, -1);
    n_name = PC_IconvStr(name, -1);

    for (list = CtrlMenuList; list; list = list->next)
    {
	cml = list->data;
	if (!strcmp(n_title, cml->title))
	    break;
    }

    if (!g_ascii_strcasecmp(type, "string"))
	func = (GCallback) RunControlMenuString;
    else if (!g_ascii_strcasecmp(type, "script"))
	func = (GCallback) RunControlMenuScript;
    else if (!g_ascii_strcasecmp(type, "sfile"))
	func = (GCallback) RunControlMenuScriptFile;
    else
	goto out;

    data = ConvertCtrlChar(action);

    if (!list)
    {
	cml = g_new(CtrlMenuListType, 1);
	cml->title = g_strdup(n_title);
	cml->entryNum = 1;
	cml->global = globalf;
	CreateCtrlMenu(cml, n_title, n_name, func, data);
	CtrlMenuList = g_slist_append(CtrlMenuList, cml);

	if (!cml->global && CtrlMenuRemoved)
	{
	    CtrlMenuRemoved = FALSE;
	    DisconnectFuncList = g_slist_append(DisconnectFuncList,
						ControlMenuExit);
	}
    }
    else
    {
	if (cml->global != globalf)
	    g_warning(_("You should use same global flag (title=%s)\n"
			"old=%d, new=%d"), n_title, cml->global, globalf);

	cml = list->data;
	cml->entryNum++;
	CtrlMenuAddEntry(cml->menu, n_name, func, data);
    }

out:
    PC_IconvStrFree(title, n_title);
    PC_IconvStrFree(name, n_name);
}
/* }}} */

/****************************************************************************
			 C A P T U R E   C O N T R O L
****************************************************************************/
static int CaptureFD = -1;
static guint32 CurrCaptureSize = 0;
static GtkWidget *CaptureFrame = NULL;
static gchar *new_log_dir = NULL;
static gboolean log_start_of_line;

/* capture_data_write() {{{1 */
static int
capture_data_write(const char *s, int len)
{
    int r;

    if ((r = write(CaptureFD, s, len)) != len)
	g_warning("Cannot save capture data (%d)\n", len);

    return r;
}

/* CaptureInputFilter() {{{1 */
int
CaptureInputFilter(const char *s, int len)
{
    register int i, n;
    char c;

    if (CaptureFD != -1 && !TRxRunning)
    {
	if (CurrCaptureSize >= MaxCaptureSize)
	    return;

	for (i = 0; i < len; i++)
	{
	    c = s[i];

	    if ((log_flags & LOG_FLAGS_TIMESTAMP) != 0 && log_start_of_line)
	    {
		struct timeval tv;
		struct tm *lt;
		char buf[128];

		gettimeofday(&tv, NULL);
		lt = localtime(&tv.tv_sec);
		n = strftime(buf, sizeof(buf) - 8, "[%Y-%m-%d %H:%M:%S", lt);
		if (n >= 0)
		    n += snprintf(&buf[n], 8, ".%03u] ",
				  (unsigned int) (tv.tv_usec / 1000));
		if (n > 0)
		    capture_data_write(buf, n);
	    }
	    log_start_of_line = (int) (c == '\n');

	    capture_data_write(&c, 1);
	    ++CurrCaptureSize;
	    if (CurrCaptureSize >= MaxCaptureSize)
	    {
		CaptureFinish();
		break;
	    }
	}
    }

    return len;
}

/* CaptureFinish() {{{1 */
void
CaptureFinish(void)
{
    if (CaptureFD != -1)
    {
	close(CaptureFD);
	CaptureFD = -1;
	CurrCaptureSize = 0UL;
	StatusShowMessage(_("Capture finished."));
	if (CaptureFrame)
	{
	    StatusRemoveFrame(CaptureFrame);
	    CaptureFrame = NULL;
	}
    }
}

/* CaptureFileSelectOk() {{{1 */
static void
CaptureFileSelectOk(GtkWidget *w, unsigned int flags)
{
    char *p, *filename;

    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(w));
    if ((p = strrchr(filename, '/')) != NULL)
    {
	if (new_log_dir)
	    g_free(new_log_dir);
	new_log_dir = g_strdup(filename);
	new_log_dir[(int) (p - filename)] = '\0';
    }
    CaptureStart(filename, flags);
    g_free(filename);
}

/* CaptureFileQuery() {{{1 */
static void
CaptureFileQuery(void)
{
    GtkWidget *w, *hbox;
    GtkWidget *b_append, *b_incbuf, *b_text, *b_timestamp;
    unsigned int flags;
    const char *folder = new_log_dir? new_log_dir: CapturePath;

    w = gtk_file_chooser_dialog_new(_("Log File"),
				    GTK_WINDOW(MainWin),
				    GTK_FILE_CHOOSER_ACTION_SAVE,
				    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				    NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(w), folder);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(w), "");

    hbox = gtk_hbox_new(FALSE, 5);
    b_timestamp = gtk_check_button_new_with_label(_("Timestamp"));
    gtk_box_pack_end(GTK_BOX(hbox), b_timestamp, FALSE, FALSE, 5);
    b_text = gtk_check_button_new_with_label(_("Plain text"));
    gtk_box_pack_end(GTK_BOX(hbox), b_text, FALSE, FALSE, 5);
    b_incbuf = gtk_check_button_new_with_label(_("Include Buffer"));
    gtk_box_pack_end(GTK_BOX(hbox), b_incbuf, FALSE, FALSE, 5);
    b_append = gtk_check_button_new_with_label(_("Append"));
    gtk_box_pack_end(GTK_BOX(hbox), b_append, FALSE, FALSE, 5);
    gtk_widget_show_all(hbox);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(w), hbox);

    if (gtk_dialog_run(GTK_DIALOG(w)) == GTK_RESPONSE_ACCEPT)
    {
	flags = 0;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_append)))
	    flags |= LOG_FLAGS_APPEND;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_timestamp)))
	    flags |= LOG_FLAGS_TIMESTAMP;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_text)))
	    flags |= LOG_FLAGS_PLAIN_TEXT;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(b_incbuf)))
	    flags |= LOG_FLAGS_INCLUDE_BUFFER;
	CaptureFileSelectOk(w, flags);
    }
    gtk_widget_destroy(w);
}

/* CaptureStart() {{{1 */
int
CaptureStart(const char *filename, unsigned int flags)
{
    int i;
    char buf[MAX_PATH];
    int append = (flags & LOG_FLAGS_APPEND)? O_APPEND: 0;

    /* script의 경우 event (가령 auto-response)에 의해 capture를 ON하는 경우 이
     * 함수가 여러번 불려질 수 있다. 이미 capture하고 있는 경우에는 성공을
     * return해 준다.
     */
    if (CaptureFD != -1)
	return 0;

    log_flags = flags;
    CurrCaptureSize = 0UL;

    if (!filename || !filename[0])
    {
	/* auto generated with date */
	time_t ti;
	struct tm *lt;

	ti = time(NULL);
	lt = localtime(&ti);
	g_snprintf(buf, sizeof(buf), "%s/%02d%02d%02d.log",
		   CapturePath, lt->tm_year, lt->tm_mon + 1, lt->tm_mday);
	if (append == 0)
	    (void) unlink(buf);
	CaptureFD = open(buf, O_WRONLY | O_CREAT | append, 0660);
    }
    else if (filename[0] == '?')
    {
	CaptureFileQuery();
	return 0;
    }
    else
    {
	if (filename[0] == '/')
	    strncpy(buf, filename, sizeof(buf) - 1);
	else
	    g_snprintf(buf, sizeof(buf), "%s/%s", CapturePath, filename);
	if (append == 0)
	    (void) unlink(buf);
	CaptureFD = open(buf, O_WRONLY | O_CREAT | append, 0660);
    }

    if (CaptureFD == -1)
    {
	StatusShowMessage(_("Cannot open capture file '%s'."), buf);
	return -1;
    }
    else
    {
	StatusShowMessage(_("Capture starts to '%s'."), buf);

	if ((log_flags & LOG_FLAGS_INCLUDE_BUFFER) != 0)
	{
	    int maxLine;
	    TermType *term = Term;

	    maxLine = TERM_LINENUM(Term->currY + 1);
	    if ((guint32) maxLine * Term->col > MaxCaptureSize)
		i = maxLine - MaxCaptureSize / Term->col + 1;
	    else
		i = 0;
	    for (; i < maxLine; i++)
	    {
		int len = strlen((char*) Term->buf + i * Term->col);
		if (len)
		    capture_data_write(Term->buf + i * Term->col, len);
		capture_data_write("\n", 1);
		CurrCaptureSize += len + 1;
	    }
	}

	if (!CaptureFrame)
	    CaptureFrame = StatusAddFrameWithLabel(" LOG ");

	log_start_of_line = TRUE;
	return 0;
    }
}

/* CaptureControl() {{{1 */
void
CaptureControl(void)
{
    if (CaptureFD != -1)
	CaptureFinish();
    else
	CaptureFileQuery();
}

/* }}} */

/****************************************************************************
			 L O G G I N G   C O N T R O L
****************************************************************************/
static FILE *LogFP = NULL;

/* OpenLogFile() {{{1 */
void
OpenLogFile(const char *name, const char *info)
{
    char *fname;
    time_t tim;
    struct tm *lt;
    char buf[128];

    if (LogFile && *LogFile)
    {
	if (*LogFile == '/' || *LogFile == '~')
	    fname = ExpandPath(LogFile);
	else
	{
	    char *exppath = ExpandPath(PC_RC_BASE);
	    fname = g_strconcat(exppath, "/", LogFile, NULL);
	    g_free(exppath);
	}

	if ((LogFP = fopen(fname, "a")) == NULL)
	    StatusShowMessage(_("Cannot open log file '%s'"), fname);
	else
	{
	    tim = time(NULL);
	    lt = localtime(&tim);
	    strftime(buf, sizeof(buf), "%a %b %d %T %Z %Y", lt);
	    fprintf(LogFP, "===================================\n");
	    fprintf(LogFP, "%s: %s\nStart: %s\n", name, info, buf);
	    fflush(LogFP);
	}
	g_free(fname);
    }
}

/* CloseLogFile() {{{1 */
void
CloseLogFile(gint32 tick)
{
    if (LogFP)
    {
	if (tick < 0)
	    tick = 0;
	fprintf(LogFP, "Total conection time: %02d:%02d:%02d\n\n",
		tick / 3600, (tick % 3600) / 60, tick % 60);
	fclose(LogFP);
	LogFP = NULL;
    }
}

/* AppendLogData() {{{1 */
/* NOT USED!!!
void
AppendLogData(const gchar *format, ...)
{
   va_list args;
   gchar *buf;

   if (LogFP)
   {
      va_start(args, format);
      buf = g_strdup_vprintf(format, args);
      va_end(args);

      fprintf(LogFP, buf);
      fflush(LogFP);
      g_free(buf);
   }
}
*/
