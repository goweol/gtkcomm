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
static gchar *oldCapPath = NULL;

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
    register int i;

    /* download/upload 등이 수행중이면 capture하지 말아라 */
    if (CaptureFD != -1 && !TRxRunning)
    {
	if (CurrCaptureSize > MaxCaptureSize - len)
	{
	    if (MaxCaptureSize > CurrCaptureSize)
		len = MaxCaptureSize - CurrCaptureSize;
	    else
	    {
		g_warning("BUG: MaxCaptureSize=%lu, CurrCaptureSize=%lu",
			  (unsigned long) MaxCaptureSize,
			  (unsigned long) CurrCaptureSize);
		len = 1;
	    }
	}

	if (TermStripLineFeed)
	{
	    for (i = 0; i < len; i++)
	    {
		if (s[i] != '\r')
		{
		    capture_data_write(s + i, 1);
		    ++CurrCaptureSize;
		}
	    }
	}
	else
	{
	    capture_data_write(s, len);
	    CurrCaptureSize += len;
	}
	if (CurrCaptureSize >= MaxCaptureSize)
	    CaptureFinish();
    }
    return len;
}

/* CaptureFinish() {{{1 */
void
CaptureFinish(void)
{
    if (CaptureFD != -1)
    {
	if (!TermStripControlChar)
	    InputFilterList =
		g_slist_remove(InputFilterList, CaptureInputFilter);
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
CaptureFileSelectOk(GtkWidget *w, GtkWidget *win)
{
    const char *p, *filename;

    UNUSED(w);

    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(win));
    if ((p = strrchr(filename, '/')) != NULL)
    {
	if (oldCapPath)
	    g_free(oldCapPath);
	oldCapPath = g_strdup(filename);
	oldCapPath[(int)(p - filename + 1)] = 0;
    }
    CaptureStart(filename, FALSE);
    gtk_widget_destroy(win);
}

/* CaptureFileQuery() {{{1 */
static void
CaptureFileQuery(void)
{
    GtkWidget *fs;
    char *file;

    fs = gtk_file_selection_new("Capture File");
    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(fs));
    file = g_strconcat(CapturePath, "/", NULL);
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), file);
    g_free(file);
    gtk_window_set_position(GTK_WINDOW(fs), GTK_WIN_POS_MOUSE);
    g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
		     "clicked", G_CALLBACK(CaptureFileSelectOk), fs);
    g_signal_connect_object(G_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			    "clicked", G_CALLBACK(gtk_widget_destroy),
			    G_OBJECT(fs), G_CONNECT_SWAPPED);
    gtk_widget_show(fs);
    if (oldCapPath)
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), oldCapPath);
}

/* CaptureStart() {{{1 */
int
CaptureStart(const char *filename, gboolean includeCurrBuf)
{
    int i;
    char buf[MAX_PATH];

    /* script의 경우 event (가령 auto-response)에 의해 capture를 ON하는 경우 이
     * 함수가 여러번 불려질 수 있다. 이미 capture하고 있는 경우에는 성공을
     * return해 준다.
     */
    if (CaptureFD != -1)
	return 0;

    CurrCaptureSize = 0UL;

    if (!filename || !filename[0])
    {
	/* auto generated with date */
	time_t ti;
	struct tm *lt;

	ti = time(NULL);
	lt = localtime(&ti);
	g_snprintf(buf, sizeof(buf), "%s/%02d%02d%02d.cap",
		   CapturePath, lt->tm_year, lt->tm_mon + 1, lt->tm_mday);
	CaptureFD = open(buf, O_WRONLY | O_CREAT | O_APPEND, 0660);
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
	CaptureFD = open(buf, O_WRONLY | O_CREAT | O_APPEND, 0660);
    }

    if (CaptureFD == -1)
    {
	StatusShowMessage(_("Cannot open capture file '%s'."), buf);
	return -1;
    }
    else
    {
	if (!TermStripControlChar)
	    InputFilterList =
		g_slist_append(InputFilterList, CaptureInputFilter);

	StatusShowMessage(_("Capture starts to '%s'."), buf);

	if (includeCurrBuf)
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
	CaptureStart(CaptureFile, FALSE);
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
