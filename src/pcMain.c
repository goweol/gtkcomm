/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * GTK+에서 윈도그용 프로콤과 비슷한 환경 구현.
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
#include <signal.h> /* SIGSEGV */

#include "pcMain.h"
#include "pcTerm.h" /* TermType */
#include "pcChat.h"

/* Global variables/functions {{{1 */

const char *MyName = NULL; /* 실행파일 이름 */

GtkWidget *MainWin = NULL;
GtkWidget *MainBox = NULL;
GtkWidget *TermBox = NULL;
GtkWidget *CtrlBox = NULL;
GtkWidget *StatBox = NULL;
GtkItemFactory *MenuFactory = NULL;
GtkWidget *ScriptBox = NULL;

guint PC_State = PC_IDLE_STATE;
gboolean UseModem = TRUE;
int ReadFD = -1;
guint32 LastOutTime = 0;
gboolean DisconnectAfterTransfer = FALSE;

gboolean ControlBarShow;
char *ToolBarType;
gboolean ToolBarShow;
gboolean UseHandleBox;
gboolean UseXtermCursor;

GSList *InputFilterList = NULL;
GSList *ConnectFuncList = NULL;
GSList *DisconnectFuncList = NULL;

#ifdef _DEBUG_
gboolean OPrintFlag[MAX_OPRT_NUM];
#endif

/* Local variables {{{1 */

static GtkWidget *TB_OpenButton = NULL, *MenuOpenButton = NULL;
static GtkWidget *TB_CloseButton = NULL, *MenuCloseButton = NULL;
static GtkWidget *TB_UploadButton = NULL, *MenuUploadButton = NULL;
static GtkWidget *TB_DownloadButton = NULL, *MenuDownloadButton = NULL;
static GtkWidget *MenuStopTRxButton = NULL;
static GtkWidget *ToolBox = NULL;
static GtkWidget *HandleBox = NULL;
static GtkWidget *MenuOpenWin = NULL;

static char *MyFullName = NULL;
static char *clipboard_text;

/* caughtSignal() {{{1 */
static RETSIGTYPE
caughtSignal(int sig)
{
    fprintf(stderr, "%s: caught signal %d\n", MyName, sig);

#ifdef _DEBUG_GDB_
    if (MyFullName)
    {
	char tmpFileName[] = "/tmp/.pcgdb.XXXXXX";
	char cmd[256];
	FILE *fp;

	mkstemp(tmpFileName);
	if ((fp = fopen(tmpFileName, "w")) != NULL)
	{
	    fprintf(fp,
		    "file %s\n"
		    "attach %d\n"
		    "set height 1000\n"
		    "bt full\n"
		    , MyFullName, getpid());
	    fclose(fp);
	    g_snprintf(cmd, sizeof(cmd), "xterm -sb -sl 1024 -e gdb -x %s", tmpFileName);
	    system(cmd);
	    unlink(tmpFileName);
	}
    }
#endif

    exit(1);
    _exit(1);
}

/* PC_Hangup() {{{1 */
void
PC_Hangup(void)
{
    if (UseModem)
	ModemHangup();
    else
	TelnetDone();
}

/* DoExitClean() {{{1 */
static void
DoExitClean(void)
{
    SoundPlay(SoundExitFile);
    PC_Hangup();	/* hangup the modem or telnet if it was opened */
    TRxStop();	/* file 전송중이면 종료 */
    CaptureFinish();	/* close capture file if it was opened. */
    ChatExit();
    I_SelectExit();
    AutoResponseExit();
    ControlMenuExit();
    ScriptExit();
    ModemExit();
    TelnetExit();
    ConfigExit();
    TermExit();
    ChildSignalHandlerExit();
    if (InputFilterList)
	g_slist_free(InputFilterList);
    if (ConnectFuncList)
	g_slist_free(ConnectFuncList);
    if (DisconnectFuncList)
	g_slist_free(DisconnectFuncList);
    ScriptRunAnimDestroy();
    g_free((gpointer)MyName);
    PC_IconvClose();
}

/* DoExit() {{{1 */
void
DoExit(int code)
{
    char *fname = ExpandPath(PC_ACCEL_RC_FILE);
    gtk_accel_map_save(fname);
    g_free(fname);

    DoExitClean();
    gtk_main_quit();
    _exit(code);
}

/* aboutDialog() {{{1 */
static void
aboutDialog(void)
{
    GtkWidget *win, *label;
    static const char s[] =
	N_("This is a communication program which is free\n"
	   "with source code. Features include direct connection\n"
	   "via serial device, telnet connection, modem connection\n"
	   "script language support, capture to file, and more.\n"
	   "\n"
	   "It is based on GAU made by Chi-Deok Hwang. Many\n"
	   "thanks to him\n"
	   "\n"
	   "To learn more, check the README and FAQ"
	   );

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), PACKAGE " " VERSION);
    gtk_container_set_border_width(GTK_CONTAINER(win), 5);
    label = gtk_label_new(_(s));
    gtk_container_add(GTK_CONTAINER(win), label);
    gtk_widget_show_all(win);
}

/* SetWindowTitle() {{{1 */
void
SetWindowTitle(const char *title)
{
    char *p, *prev, *p1;
    char *buf;
    char tmpbuf[80];

    if (title)
    {
	strncpy(tmpbuf, title, sizeof(tmpbuf) - 1);
	tmpbuf[sizeof(tmpbuf) - 1] = '\0';
	buf = g_strconcat("", NULL);
	prev = tmpbuf;
	for (p = tmpbuf; (p = strchr(p, '%')) != NULL; p += 2)
	{
	    *p = '\0';
	    p1 = g_strconcat(buf, prev, NULL);
	    g_free(buf); buf = p1;
	    p1 = NULL;
	    switch (p[1])
	    {
		case 'p': p1 = g_strconcat(buf, PACKAGE, NULL);
			  break;
		case 'v': p1 = g_strconcat(buf, VERSION, NULL);
			  break;
		case 'n': if (ModemInfo)
			      p1 = g_strconcat(buf, ModemInfo->name, NULL);
			  else if (TelnetInfo)
			      p1 = g_strconcat(buf, TelnetInfo->name, NULL);
			  break;
		case 'h': if (ModemInfo)
			      p1 = g_strconcat(buf, ModemInfo->number, NULL);
			  else if (TelnetInfo)
			      p1 = g_strconcat(buf, TelnetInfo->host, NULL);
			  break;
		default: break;
	    }
	    if (p1 != NULL)
	    {
		g_free(buf);
		buf = p1;
	    }
	    prev = p + 2;
	}
	p1 = g_strconcat(buf, prev, NULL);
	g_free(buf); buf = p1;
    }
    else
	buf = g_strconcat(PACKAGE " - " VERSION, NULL);

    p1 = PC_IconvStr(buf, -1);
    gtk_window_set_title(GTK_WINDOW(MainWin), p1);
    g_free(buf);
    PC_IconvStrFree(buf, p1);
}

/* MenuOpenModem() {{{1 */
static void
MenuOpenModem(GtkWidget *w, GtkWidget *entry)
{
    const char *s;

    s = gtk_entry_get_text(GTK_ENTRY(entry));
    if (*s)
	ModemConnectByName(s);

    gtk_widget_destroy(MenuOpenWin);
    w = NULL;
}

/* MenuOpenTelnet() {{{1 */
static void
MenuOpenTelnet(GtkWidget *w, GtkWidget *entry)
{
    const char *s;

    s = gtk_entry_get_text(GTK_ENTRY(entry));
    if (*s)
	TelnetConnectByHostname(s);

    gtk_widget_destroy(MenuOpenWin);

    UNUSED(w);
}

/* MenuOpen() {{{1 */
static void
MenuOpen(void)
{
    GtkWidget *vbox, *hbox, *label, *entry, *button, *sep;

    MenuOpenWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(MenuOpenWin), 5);
    gtk_window_set_title(GTK_WINDOW(MenuOpenWin), "Connect");
    gtk_window_set_position(GTK_WINDOW(MenuOpenWin), GTK_WIN_POS_MOUSE);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(MenuOpenWin), vbox);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    label = gtk_label_new("Host or Dial");
    gtk_container_add(GTK_CONTAINER(hbox), label);

    entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(hbox), entry);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    button = gtk_button_new_with_mnemonic(_("_Modem"));
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(MenuOpenModem), entry);

    button = gtk_button_new_with_mnemonic(_("_Telnet"));
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(MenuOpenTelnet), entry);

    button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect_object(G_OBJECT(button), "clicked",
			    G_CALLBACK(gtk_widget_destroy),
			    G_OBJECT(MenuOpenWin), G_CONNECT_SWAPPED);

    gtk_widget_show_all(MenuOpenWin);
    gtk_widget_grab_focus(entry);
}

/* MenuClose() {{{1 */
static void
MenuClose(void)
{
    if (PC_State == PC_RUN_STATE)
    {
	TRxStop();	/* file 전송중이면 종료 */
	PC_Hangup();
	PC_State = PC_IDLE_STATE;
    }
}

/* MenuQuit() {{{1 */
static void
MenuQuit(void)
{
    DoExit(0);
}

/* clipboard_owner_change() {{{1 */
static void
clipboard_owner_change(GtkClipboard *clipboard, GdkEvent *event,
		    gpointer data)
{
    char *text;

    // Avoid 'unused arg' warnings.
    (void)event;
    (void)data;

    // Get the selected text from the clipboard; note that we could
    // get back NULL if the clipboard is empty or does not contain
    // text.
    if ((text = gtk_clipboard_wait_for_text(clipboard)) != NULL)
    {
	if (clipboard_text)
	    g_free(clipboard_text);
	clipboard_text = g_strdup(text);
    }
}

/* MenuEditCopy() {{{1 */
static void
MenuEditCopy(gpointer cb_data, guint cb_action, GtkWidget *w)
{
    if (clipboard_text)
    {
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_set_text(clipboard, clipboard_text,
			       strlen(clipboard_text));
    }
}

/* MenuEditPaste() {{{1 */
static void
MenuEditPaste(gpointer cb_data, guint cb_action, GtkWidget *w)
{
    if (gtk_widget_has_focus(GTK_WIDGET(Term)) && clipboard_text)
	TERM(Term)->remoteWrite(clipboard_text, strlen(clipboard_text));
}

/* MenuResetTTY() {{{1 */
static void
MenuResetTTY(void)
{
    if (Term)
    {
	TermReset(Term);

	/* reset tty if modem is used */
	ModemResetTTY();
    }
}

/* MenuRunScript() {{{1 */
/* ScriptBox에서 script file명을 얻어오지 않음으로서 사용자가
 * 언제든 원하는 script file을 입력할 수 있도록 한다.
 */
static void
MenuRunScript(void)
{
    if (ScriptRunning)
	ScriptCancel();
    else
    {
	const char *s = NULL;
	if (ScriptBox)
	    gtk_entry_get_text(GTK_ENTRY(ScriptBox));
	GeneralInputQuery(_("Script Run"), _("Filename:"), s,
			  (void (*)(char *)) ScriptRunFromFile, NULL);
    }
}

/* MenuRunEditor() {{{1 */
static void
MenuRunEditor(const char *file)
{
    char *xeditor;

    if (file && *file)
    {
	xeditor = getenv("XEDITOR");
	if (xeditor == NULL || *xeditor == '\0')
	    xeditor = "gvim";
	pc_system("%s %s", xeditor, file);
    }
}

/* MenuEditScript() {{{1 */
static void
MenuEditScript(void)
{
    const char *s = NULL;

    if (ScriptBox)
	s = gtk_entry_get_text(GTK_ENTRY(ScriptBox));
    if (s && *s)
    {
	s = ScriptGetFullPath(s);
	if (s && *s)
	{
	    GeneralInputQuery(_("Script Edit"), _("Filename:"), s,
			      (void (*)(char *)) MenuRunEditor, NULL);
	}
    }
}

/* TB_Script() {{{1 */
static void
TB_Script(void)
{
    const char *s;

    if (ScriptRunning)
	ScriptCancel();
    else
    {
	s = gtk_entry_get_text(GTK_ENTRY(ScriptBox));
	/* NOTE: 다음 script가 제대로 finish되지 않는 경우 - 가령 무한
	 * waitfor 상태 - combo box가 모든 key를 받아먹는다. 그래서
	 * Term으로 focus를 돌린다.
	 */
	gtk_widget_grab_focus(GTK_WIDGET(Term));
	ScriptRunFromFile(s);
    }
}

/* MenuOptTblType {{{1 */
typedef struct _MenuOptTblType MenuOptTblType;
struct _MenuOptTblType {
    char *name;
    char *type;
};

/* MenuOptTbl {{{1 */
static MenuOptTblType MenuOptTbl[] = {
    {"Controlbar/View", "<ToggleItem>"},
    {"Toolbar/View", "<ToggleItem>"},
    {"Toolbar/Type/ICONS", ""},
    {"Toolbar/Type/TEXT", ""},
    {"Toolbar/Type/BOTH", ""},
    {"NewlineConvert/NONE", ""},
    {"NewlineConvert/CR", ""},
    {"NewlineConvert/LF", ""},
    {"NewlineConvert/CRLF", ""},
    {"Auto Response/Active", "<ToggleItem>"}
};

/* MenuOptionControl() {{{1 */
static void
MenuOptionControl(int idx)
{
    GtkWidget *w;
    char *path;

    path = g_strconcat("/Tools/Options/", MenuOptTbl[idx].name, NULL);
    w = gtk_item_factory_get_widget(MenuFactory, path);
    g_free(path);

    if (w == NULL)
    {
	g_warning(_("%s: cannot get widget for '%s'"), __FUNCTION__,
		  MenuOptTbl[idx].name);
	return;
    }

    switch (idx)
    {
	case 0:	/* control-bar view */
	    /* control-bar hide가 false인 경우 control-bar가 보일 것이므로
	     * active로 만들어준다.
	     */
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					  ControlBarShow);
	    break;

	case 1:	/* tool-bar view */
	    /* tool-bar hide가 false인 경우 tool-bar가 보일 것이므로
	     * active로 만들어준다.
	     */
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), ToolBarShow);
	    break;

	case 9:	/* auto-response */
	    /* auto-response enable flag가 이 값에 의해서 toggle된다. */
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), TRUE);
	    break;

	default:
	    break;
    }
}

/* ControlBarFix() {{{1 */
void
ControlBarFix(void)
{
    if (CtrlBox)
    {
	if (ControlBarShow != GTK_WIDGET_VISIBLE(CtrlBox))
	{
	    if (ControlBarShow)
		gtk_widget_show(CtrlBox);
	    else
		gtk_widget_hide(CtrlBox);
	    MenuOptionControl(0);
	}
	TermSizeFix();
    }
}

/* WidgetShowHide() {{{1 */
static void
WidgetShowHide(GtkWidget *w, gboolean show)
{
    if (w)
    {
	if (show)
	{
	    if (!GTK_WIDGET_VISIBLE(w))
		gtk_widget_show(w);
	}
	else
	{
	    if (GTK_WIDGET_VISIBLE(w))
		gtk_widget_hide(w);
	}
	TermSizeFix();
    }
}

/* MenuOptions() {{{1 */
/* CtrlBox나 ToolBox 등이 생성되기 전에 이 함수가 불려질 수 있는 데, 그것은
 * MenuOptionControl()에 의해서이다. menu의 active state가 바뀔 수 있기
 * 때문이다.
 */
static void
MenuOptions(const char *name)
{
    register int idx;

    for (idx = 0; idx < (int) G_N_ELEMENTS(MenuOptTbl); idx++)
	if (!strcmp(name, MenuOptTbl[idx].name))
	    break;

    switch (idx)
    {
	case 0:	/* control-bar view */
	    if (CtrlBox)
	    {
		ControlBarShow = !ControlBarShow;
		WidgetShowHide(CtrlBox, ControlBarShow);
	    }
	    break;

	case 1:	/* tool-bar view */
	    if (UseHandleBox)
	    {
		if (HandleBox)
		{
		    ToolBarShow = !ToolBarShow;
		    WidgetShowHide(HandleBox, ToolBarShow);
		}
	    }
	    else
	    {
		if (ToolBox)
		{
		    ToolBarShow = !ToolBarShow;
		    WidgetShowHide(ToolBox, ToolBarShow);
		}
	    }
	    break;

	case 2:
	    if (ToolBox)
	    {
		ScriptRunSizeChanged(FALSE);	/* small */
		gtk_toolbar_set_style(GTK_TOOLBAR(ToolBox), GTK_TOOLBAR_ICONS);
		g_free(ToolBarType);
		ToolBarType = g_strdup("ICONS");
	    }
	    break;

	case 3:
	    if (ToolBox)
	    {
		ScriptRunSizeChanged(FALSE);	/* small */
		gtk_toolbar_set_style(GTK_TOOLBAR(ToolBox), GTK_TOOLBAR_TEXT);
		g_free(ToolBarType);
		ToolBarType = g_strdup("TEXT");
	    }
	    break;

	case 4:
	    if (ToolBox)
	    {
		ScriptRunSizeChanged(TRUE);	/* large */
		gtk_toolbar_set_style(GTK_TOOLBAR(ToolBox), GTK_TOOLBAR_BOTH);
		g_free(ToolBarType);
		ToolBarType = g_strdup("BOTH");
	    }
	    break;

	    /* enter convert */
	case 5:
	    EnterConvert = ENTER_NONE;
	    break;
	case 6:
	    EnterConvert = ENTER_CR;
	    break;
	case 7:
	    EnterConvert = ENTER_LF;
	    break;
	case 8:
	    EnterConvert = ENTER_CRLF;
	    break;

	case 9:	/* auto response */
	    AutoResCheckEnabled = !AutoResCheckEnabled;
	    break;

	default:
	    g_warning(_("Unknown menu option index = %d"), idx);
	    break;
    }
}

/* CreateNewPixmap() {{{1 */
static GtkWidget *
CreateNewPixmap(const char *filename)
{
    GtkWidget *w = NULL;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    char *f;

    /* check if I can access pixmap file */
    if ((f = GetRealFilename(filename, PKGDATADIR "/pixmap")) == NULL)
	return NULL;

    pixmap = gdk_pixmap_colormap_create_from_xpm(NULL,
						 gtk_widget_get_colormap(MainWin),
						 &mask,
						 &MainWin->style->bg[GTK_STATE_NORMAL],
						 (const gchar *)f);
    g_free(f);

    if (pixmap)
    {
	w = gtk_image_new_from_pixmap(pixmap, mask);

	g_object_unref(pixmap);
	g_object_unref(mask);
    }

    return w;
}

/* CreateToolbar() {{{1 */
static GtkWidget *
CreateToolbar(GtkWidget *w, gint tbType)
{
    GtkWidget *tb, *pixmap, *cb;

    UNUSED(w);

    tb = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(tb), tbType);
    gtk_toolbar_set_orientation(GTK_TOOLBAR(tb), GTK_ORIENTATION_HORIZONTAL);

    if ((pixmap = CreateNewPixmap("tb_open.xpm")) != NULL)
	TB_OpenButton = gtk_toolbar_append_item(GTK_TOOLBAR(tb),
						_("Open"),
						_("Telnet or Modem connect"),
						NULL, pixmap,
						(GCallback) MenuOpen, tb);

    if ((pixmap = CreateNewPixmap("tb_close.xpm")) != NULL)
	TB_CloseButton = gtk_toolbar_append_item(GTK_TOOLBAR(tb),
						 _("Close"),
						 _("Telnet or Modem disconnect"),
						 NULL, pixmap,
						 (GCallback) MenuClose, tb);

    if ((pixmap = CreateNewPixmap("tb_exit.xpm")) != NULL)
	gtk_toolbar_append_item(GTK_TOOLBAR(tb),
				_("Exit"), _("Exit program"), NULL,
				pixmap, (GCallback) MenuQuit, tb);

    if ((pixmap = ScriptRunAnimInit((GCallback) TB_Script)) != NULL)
    {
	if ((cb = CreateScriptFileList(tb, TB_Script)) != NULL)
	{
	    gtk_toolbar_append_space(GTK_TOOLBAR(tb));

	    gtk_toolbar_append_widget(GTK_TOOLBAR(tb), cb, "Script file list",
				      NULL);
	    gtk_toolbar_append_space(GTK_TOOLBAR(tb));

	    gtk_toolbar_append_widget(GTK_TOOLBAR(tb), pixmap,
				      "Script file list", NULL);
	    gtk_toolbar_append_space(GTK_TOOLBAR(tb));
	}
	else
	    ScriptRunAnimDestroy();
    }

    if ((pixmap = CreateNewPixmap("tb_capture.xpm")) != NULL)
	gtk_toolbar_append_item(GTK_TOOLBAR(tb),
				_("Capture"), _("Capture the input string"),
				NULL, pixmap, (GCallback) CaptureControl,
				tb);

    if ((pixmap = CreateNewPixmap("tb_up.xpm")) != NULL)
	TB_UploadButton = gtk_toolbar_append_item(GTK_TOOLBAR(tb),
						  _("Upload"),
						  _("Upload a file"),
						  NULL, pixmap,
						  (GCallback) UploadFileQuery,
						  tb);

    if ((pixmap = CreateNewPixmap("tb_down.xpm")) != NULL)
	TB_DownloadButton = gtk_toolbar_append_item(GTK_TOOLBAR(tb),
						    _("Download"),
						    _("Download a file"), NULL,
						    pixmap,
						    (GCallback)
						    DownloadFileQuery, tb);

    return tb;
}

/* PC_ButtonControl() {{{1 */
void
PC_ButtonControl(int state)
{
    gboolean open_f, close_f, up_f, down_f, stop_f;

    open_f = close_f = up_f = down_f = stop_f = FALSE;
    switch (state)
    {
	case TB_IDLE_STATE:
	    open_f = TRUE;
	    break;
	case TB_CONNECT_STATE:
	    close_f = up_f = down_f = TRUE;
	    break;
	case TB_TRX_STATE:
	    stop_f = TRUE;
	    break;
	default:
	    FatalExit(NULL, "invalid button sel.");
	    break;	/* never reach */
    }

    if (TB_OpenButton)
	gtk_widget_set_sensitive(TB_OpenButton, open_f);
    if (TB_CloseButton)
	gtk_widget_set_sensitive(TB_CloseButton, close_f);
    if (TB_UploadButton)
	gtk_widget_set_sensitive(TB_UploadButton, up_f);
    if (TB_DownloadButton)
	gtk_widget_set_sensitive(TB_DownloadButton, down_f);

    if (MenuOpenButton)
	gtk_widget_set_sensitive(MenuOpenButton, open_f);
    if (MenuCloseButton)
	gtk_widget_set_sensitive(MenuCloseButton, close_f);
    if (MenuUploadButton)
	gtk_widget_set_sensitive(MenuUploadButton, up_f);
    if (MenuDownloadButton)
	gtk_widget_set_sensitive(MenuDownloadButton, down_f);
    if (MenuStopTRxButton)
	gtk_widget_set_sensitive(MenuStopTRxButton, stop_f);
}

/* menuTranslate() {{{1 */
static char *
menuTranslate(const char *path, gpointer data)
{
    UNUSED(data);
    return _(path);
}

/* CreateMainWindow() {{{1 */
static void
CreateMainWindow(void)
{
    register int i, tbType;
    GtkWidget *hbox, *frame, *sw, *sep;
    GtkAccelGroup *accelGroup;
    static const GtkTargetEntry targetEntry[] = {
	{"STRING", 0, 0}
    };
    GtkItemFactoryEntry entry;
    /* NOTE: Terminal emulation 프로그램이므로 메뉴에서는 단축글쇠 사용을
     * 자재할 필요가 있다. 특히 Control 키와의 조합은 절대 사용하지 말것이며
     * 일반적으로 Alt key는 안전하다고 하겠다.
     *
     * Used hotkey list:
     * Alt: c d e f h l o p r s t u w x
     */
    static GtkItemFactoryEntry menuItems[] = {
	{N_("/_File"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_File/_Open"), "<alt>o", MenuOpen, 0, "<StockItem>", GTK_STOCK_OPEN},
	{N_("/_File/_Close"), NULL, MenuClose, 0, "<StockItem>", GTK_STOCK_CLOSE},
	{("/_File/---"), NULL, 0, 0, "<Separator>", NULL},
	{N_("/_File/_Exit"), "<alt>x", MenuQuit, 0, "<StockItem>", GTK_STOCK_QUIT},
	{N_("/_Edit"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Edit/_Copy"), "<control><shift>c",
	    (GtkItemFactoryCallback) MenuEditCopy, 0,
	    "<StockItem>", GTK_STOCK_COPY},
	{N_("/_Edit/_Paste"), "<control><shift>v",
	    (GtkItemFactoryCallback) MenuEditPaste, 0,
	    "<StockItem>", GTK_STOCK_PASTE},
	{N_("/_Tools"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options/Controlbar"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options/Toolbar"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options/Toolbar/Type"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options/NewlineConvert"), NULL, 0, 0, "<Branch>", NULL},
	{N_("/_Tools/_Options/Auto Response"), NULL, 0, 0, "<Branch>", NULL},
	{("/_Tools/---"), NULL, 0, 0, "<Separator>", NULL},
	{N_("/_Tools/_Download"), "<alt>d",
	 (GtkItemFactoryCallback) DownloadFileQuery, 0, NULL, NULL},
	{N_("/_Tools/_Upload"), "<alt>p",
	 (GtkItemFactoryCallback) UploadFileQuery, 0, NULL, NULL},
	{N_("/_Tools/_Stop Transfer"), "<alt>r", TRxStop, 0, NULL, NULL},
	{("/_Tools/---"), NULL, 0, 0, "<Separator>", NULL},
	{N_("/_Tools/_Capture"), "<alt>l",
	 (GtkItemFactoryCallback) CaptureControl, 0, NULL, NULL},
	{N_("/_Tools/Script _Run-Stop"), "<alt>s", MenuRunScript, 0, NULL, NULL},
	{N_("/_Tools/Script _Edit"), "<alt>e", MenuEditScript, 0, NULL, NULL},
	{N_("/_Tools/Cha_t"), "<alt>c", MenuChat, 0, NULL, NULL},
	{N_("/_Tools/Reset Ter_minal"), "<alt>u", MenuResetTTY, 0,
	    "<StockItem>", GTK_STOCK_CLEAR},
	{N_("/_Tools/Open _History Window"), "<alt>w", ModemOpenHistoryWin,
	    0, NULL, NULL},
	{N_("/_Bookmarks"), NULL, 0, 0, "<Branch>", NULL}
    };
    static GtkItemFactoryEntry helpEntry[] = {
	{N_("/_Help"), NULL, 0, 0, "<LastBranch>", NULL},
	{N_("/_Help/_About..."), NULL, aboutDialog, 0, "<StockItem>", GTK_STOCK_HELP}
    };

    GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);

    // Connect to the "owner-change" signal which means that ownership
    // of the X clipboard has changed.
    g_signal_connect(clipboard, "owner-change",
                     G_CALLBACK(clipboard_owner_change), NULL);

    MainWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(MainWin), "destroy", G_CALLBACK(MenuQuit), NULL);
    SetWindowTitle(NULL);
    gtk_widget_set_name(MainWin, PACKAGE);	/* for gtkrc */
    gtk_container_set_border_width(GTK_CONTAINER(MainWin), 5);

    MainBox = gtk_vbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(MainWin), MainBox);
    gtk_widget_show(MainBox);

    accelGroup = gtk_accel_group_new();
    MenuFactory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accelGroup);

    gtk_item_factory_set_translate_func(MenuFactory, menuTranslate, NULL, NULL);
    gtk_item_factory_create_items(MenuFactory, G_N_ELEMENTS(menuItems), menuItems,
				  NULL);

    AddBookmarkMenus(MenuFactory, (void (*)(gpointer)) ModemConnectByName,
		     (void (*)(gpointer)) TelnetConnectByHostname);

    gtk_item_factory_create_items(MenuFactory, G_N_ELEMENTS(helpEntry), helpEntry,
				  NULL);

    memset(&entry, 0, sizeof(entry));
    for (i = 0; i < (int) G_N_ELEMENTS(MenuOptTbl); i++)
    {
	entry.path = g_strconcat(N_("/_Tools/_Options/"), MenuOptTbl[i].name, NULL);
	entry.item_type = MenuOptTbl[i].type;
	entry.callback = MenuOptions;
	gtk_item_factory_create_item(MenuFactory, &entry,
				     (gpointer) MenuOptTbl[i].name, 1);

	MenuOptionControl(i);
	g_free(entry.path);
    }

    gtk_window_add_accel_group(GTK_WINDOW(MainWin), accelGroup);
    gtk_box_pack_start(GTK_BOX(MainBox), MenuFactory->widget, FALSE, FALSE, 0);
    gtk_widget_show(MenuFactory->widget);

    /* menu의 active 상태를 조작하기 위해 위젯을 얻어온다. */
    MenuOpenButton = gtk_item_factory_get_widget(MenuFactory,
						 N_("/_File/_Open"));
    MenuCloseButton = gtk_item_factory_get_widget(MenuFactory,
						  N_("/_File/_Close"));
    MenuDownloadButton = gtk_item_factory_get_widget(MenuFactory,
						     N_("/_Tools/_Download"));
    MenuStopTRxButton = gtk_item_factory_get_widget(MenuFactory,
						    N_("/_Tools/_Stop Transfer"));
    MenuUploadButton = gtk_item_factory_get_widget(MenuFactory,
						   N_("/_Tools/_Upload"));

    /* toolbar */

    if (!g_ascii_strcasecmp(ToolBarType, "ICONS"))
	tbType = GTK_TOOLBAR_ICONS;
    else if (!g_ascii_strcasecmp(ToolBarType, "TEXT"))
	tbType = GTK_TOOLBAR_TEXT;
    else
    {
	if (g_ascii_strcasecmp(ToolBarType, "BOTH"))
	{
	    g_free(ToolBarType);
	    ToolBarType = g_strdup("BOTH");
	}
	tbType = GTK_TOOLBAR_BOTH;
    }

    if (UseHandleBox)
    {
	HandleBox = gtk_handle_box_new();
	gtk_box_pack_start(GTK_BOX(MainBox), HandleBox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(HandleBox), hbox);
	gtk_container_set_border_width(GTK_CONTAINER(HandleBox), 3);

	if ((ToolBox = CreateToolbar(hbox, tbType)) != NULL)
	{
	    gtk_box_pack_start(GTK_BOX(hbox), ToolBox, FALSE, FALSE, 0);

	    gtk_widget_show_all(HandleBox);
	    if (!ToolBarShow)
		gtk_widget_hide(HandleBox);
	}
    }
    else
    {
	if ((ToolBox = CreateToolbar(MainBox, tbType)) != NULL)
	{
	    gtk_box_pack_start(GTK_BOX(MainBox), ToolBox, FALSE, FALSE, 0);
	    gtk_widget_show_all(ToolBox);
	    if (!ToolBarShow)
		gtk_widget_hide(ToolBox);
	}
    }

    /* terminal */

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(MainBox), hbox);

    frame = gtk_frame_new(NULL);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
    gtk_container_add(GTK_CONTAINER(hbox), frame);

    TermBox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), TermBox);

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
				   GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(TermBox), sw);

    Term = TermNew();
    gtk_container_add(GTK_CONTAINER(sw), GTK_WIDGET(Term));
    gtk_widget_set_size_request(GTK_WIDGET(Term), TermCol * Term->fontWidth,
				TermRow * Term->fontHeight);

    gtk_widget_show_all(hbox);

    gtk_selection_add_targets(GTK_WIDGET(Term), GDK_SELECTION_PRIMARY,
			      targetEntry,
			      sizeof(targetEntry) / sizeof(targetEntry[0]));

    gtk_widget_grab_focus(GTK_WIDGET(Term));

    CtrlBox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(MainBox), CtrlBox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(CtrlBox), 0);
    gtk_widget_show_all(CtrlBox);
    if (!ControlBarShow)
	gtk_widget_hide(CtrlBox);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(MainBox), sep, FALSE, FALSE, 0);
    gtk_widget_show_all(sep);

    StatBox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(MainBox), StatBox, FALSE, FALSE, 0);
    CreateStatusBar(StatBox);
    gtk_widget_show_all(StatBox);

    PC_ButtonControl(TB_IDLE_STATE);

    if (ScriptBox)
	gtk_widget_set_size_request(ScriptBox, Term->fontWidth * 12, -1);

    {
	char *fname = ExpandPath(PC_ACCEL_RC_FILE);
	gtk_accel_map_load(fname);
	g_free(fname);
    }

    gtk_widget_show_all(MainWin);

    if (UseXtermCursor)
    {
	GdkCursor *cursor = gdk_cursor_new(GDK_XTERM);
	gdk_window_set_cursor(Term->widget.window, cursor);
	gdk_cursor_unref(cursor);
    }
}

/* FatalExit() {{{1 */
void
FatalExit(GtkWidget *parent, const gchar *form, ...)
{
    gchar *p, *p2;
    va_list args;
    GtkWidget *w;

    va_start(args, form);
    p = g_strdup_vprintf(form, args);
    va_end(args);

    p2 = g_strconcat("\n   ", p, "   \n", NULL);
    w = DialogShowMessage(parent, PACKAGE ": FATAL ERROR", "EXIT", p2);
    g_free(p);
    g_free(p2);
    if (w)
    {
	gtk_window_set_modal(GTK_WINDOW(w), TRUE);
	g_signal_connect(G_OBJECT(w), "destroy", G_CALLBACK(MenuQuit), NULL);

	/* 더이상 어떤 작업도 수행하지 마라! */
	gtk_main();
    }
    DoExit(0);
}

/* main() {{{1 */
int
main(int argc, char *argv[])
{
    register int i;
    char *fname, *conName = NULL;

#ifdef HAVE_GETTEXT
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif
    textdomain(PACKAGE);
#endif

    gtk_set_locale();
    gtk_init(&argc, &argv);

    PC_IconvOpen();

    MyFullName = argv[0];
    MyName = g_path_get_basename(MyFullName);
    signal(SIGSEGV, caughtSignal);
    signal(SIGKILL, caughtSignal);
    signal(SIGQUIT, caughtSignal);
    signal(SIGTERM, caughtSignal);
    signal(SIGINT, caughtSignal);
    signal(SIGABRT, caughtSignal);

    ResourceCheckInstall();

    RegisterChildSignalHandler();

#if defined(_DEBUG_)
    for (i = 0; i < MAX_OPRT_NUM; i++)
	OPrintFlag[i] = FALSE;
#endif

    DoConfig();

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    if (!strcmp(argv[i], "--version"))
	    {
		printf("%s\n", VERSION);
		exit(0);
	    }
	    else if (!strcmp(argv[i], "--modem"))
		UseModem = TRUE;
	    else if (!strcmp(argv[i], "--telnet"))
		UseModem = FALSE;
#ifdef _DEBUG_
	    else if (!strcmp(argv[i], "--debug") && (i + 1) < argc)
	    {
		unsigned opt;
		char *s, *s2;

		++i;
		for (s = argv[i]; *s; s = s2 + 1)
		{
		    if ((s2 = strchr(s, ',')) != NULL)
			*s2 = '\0';

		    if (sscanf(s, "%u", &opt) == 1 && opt < G_N_ELEMENTS(OPrintFlag))
			OPrintFlag[opt] = TRUE;

		    if (s2 == NULL)
			break;
		}
	    }
#endif
	    else if (!strcmp(argv[i], "--help"))
	    {
		printf("Usage: %s <options> <hostname|dial_num>\n"
#ifdef _DEBUG_
		       "  --debug #n,#n1,... : enable debug\n"
#endif
		       "  --help	         : this help message\n"
		       "  --modem            : use modem if dial_num given\n"
		       "  --telnet           : use telnet if hostname given\n"
		       "  --version          : display version\n", MyName);
		exit(0);
	    }
	    else
	    {
		printf("To know valid option, type '%s --help'.\n", MyName);
		exit(0);
	    }
	}
	else
	    conName = argv[i];
    }

    CreateMainWindow();

    TermEmulModeSet();
    AutoResponseInit();
    I_SelectInit();
    TRxInit();

    if ((fname = GetRealFilename(DEFAULT_STARTUP_SCRIPT, NULL)) != NULL)
    {
	ScriptRunFromFile(fname);
	g_free(fname);
    }

    SoundPlay(SoundStartFile);

    if (conName)
    {
	if (UseModem)
	    gtk_timeout_add(500, (GtkFunction) ModemConnectByName, conName);
	else
	    gtk_timeout_add(500, (GtkFunction) TelnetConnectByHostname, conName);
    }

    gtk_main();

    DoExitClean();

    return 0;
}
