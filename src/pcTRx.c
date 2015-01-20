/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * File Transfer
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
#include <sys/stat.h>	/* stat() */
#include <signal.h>	/* kill() */
#include <sys/wait.h>	/* waitpid() */
#include "pcMain.h"
#include "pcTerm.h"

int TRxRunning = TRX_IDLE;
gfloat TRxPercent = 0.0;
guint32 TRxProtocol = TRX_PROT_XMODEM;
gboolean UseZtelnet;
gboolean AutoRunZmodem;

char *DownloadPath;
char *UploadPath;

char *RX_Command;
char *SX_Command;
char *RB_Command;
char *SB_Command;
char *RZ_Command;
char *SZ_Command;
char *RX_InfoStr;
char *TX_InfoStr;

static int TRxPipeFD[2];
static GIOChannel *TRxInputChannel = NULL;
static guint TRxInputID;
static pid_t TRxPid = -1;
static long TxTotalCnt = 0;

static GtkWidget *TRxFileQueryWin;
static GtkWidget *FilenameEntry;
static SignalType *TRxHandler = NULL;
static gchar *oldTRxPath = NULL;

/* pcMain.h의 protocol enum과 순서가 같아야 한다. */
const char *TRxProtocolNames[] = {
    "ASCII", "RASCII", "KERMIT", "XMODEM", "YMODEM", "ZMODEM",
};

/* TRxSendCancelString() {{{1 */
static void
TRxSendCancelString(void)
{
    static char cancelZmodemStr[] =
	"\030\030\030\030\030\030\030\030\030\030"
	"\008\008\008\008\008\008\008\008\008\008";

    /* xmodem cancel string: ^Z 여러번 */
    static char cancelXmodemStr[] = "\030\030\030\030";

    /* NOTE: ascii cancel의 경우에는 표준이 없을 것 같고, 본인은 항상
     * ^C를 사용하므로 이렇게 하였다. 정말로 누군가가 이걸 바꾸고 싶다면
     * email 쎄려주시라. 금새 option처리 해드리겠슴다.
     */
    static char cancelAsciiStr[] = "\003\003";

    g_assert(Term != NULL);

    if (TRxProtocol == TRX_PROT_ZMODEM)
	Term->remoteWrite((guchar*) cancelZmodemStr, -1);
    else if (TRxProtocol == TRX_PROT_XMODEM)
	Term->remoteWrite((guchar*) cancelXmodemStr, -1);
    else if (TRxProtocol == TRX_PROT_ASCII || TRxProtocol == TRX_PROT_RAW_ASCII)
	Term->remoteWrite((guchar*) cancelAsciiStr, -1);
}

/* TRxStop() {{{1 */
void
TRxStop(void)
{
    if (TRxRunning)
    {
	TRxRunning = TRX_IDLE;

	if (TRxHandler)
	{
	    RemoveChildSignalHandler(TRxHandler);
	    TRxHandler = NULL;
	}

	/* NOTE: cancel을 먼저 보내고, TRxPid를 kill하면 화면이 깨질 일이
	 * 별로 없지만 downloading받던 file은 지워진다. rz program이
	 * cancel을 인식하여 자동으로 지우는 것 같다.
	 *   반대로, kill을 먼저하고 cancel을 보내면 downloading중이던
	 * 파일은 남게 되지만 화면이 깨질 가능성이 많다.
	 */
	TRxSendCancelString();

	if (TRxInputChannel)
	{
	    GSource *src;

	    PC_Sleep(1000);
	    g_io_channel_unref(TRxInputChannel);
	    TRxInputChannel = NULL;

	    src = g_main_context_find_source_by_id(NULL, TRxInputID);
	    if (src)
		g_source_destroy(src);

	    if (TRxPid > 0)
	    {
		kill(TRxPid, SIGINT);
		waitpid(TRxPid, NULL, 0);
	    }
	    TermInputChannel = g_io_channel_unix_new(ReadFD);
	    TermInputID = g_io_add_watch(TermInputChannel, G_IO_IN,
					 TermInputHandler, NULL);
	}
	TRxPid = -1;

	StatusShowMessage(_("Transfer canceled."));
	ProgressBarControl(FALSE);	/* remove progress-bar */
	PC_ButtonControl(TB_CONNECT_STATE);
    }
}

/* TRxChildSignalHandler() {{{1 */
static void
TRxChildSignalHandler(void)
{
    TRxRunning = TRX_IDLE;
    TRxPid = -1;
    TRxHandler = NULL;

    if (TRxInputChannel)
    {
	GSource *src;

	g_io_channel_unref(TRxInputChannel);
	TRxInputChannel = NULL;

	src = g_main_context_find_source_by_id(NULL, TRxInputID);
	if (src)
	    g_source_destroy(src);

	TermInputChannel = g_io_channel_unix_new(ReadFD);
	TermInputID = g_io_add_watch(TermInputChannel, G_IO_IN,
				     TermInputHandler, NULL);
    }

    SoundPlay(SoundTRxEndFile);
    ProgressBarControl(FALSE);	/* remove progress-bar */
    PC_ButtonControl(TB_CONNECT_STATE);

    if (DisconnectAfterTransfer)
	PC_Hangup();
}

/* TRxInputHandler() {{{1 */
static gboolean
TRxInputHandler(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    char buf[256];
    int size;
    long txCnt, rxCnt, totalCnt;
    char *p;
    int source;

    UNUSED(data);
    UNUSED(cond);

    source = g_io_channel_unix_get_fd(channel);
    if ((size = read(source, buf, sizeof(buf))) > 0)
    {
	Term->localWrite(Term, (guchar*) buf, size);
	if (TRxProtocol == TRX_PROT_ZMODEM || TRxProtocol == TRX_PROT_XMODEM
	    || TRxProtocol == TRX_PROT_YMODEM)
	{
	    if (TRxRunning == TRX_DOWNLOAD && RX_InfoStr)
	    {
		p = FindSubString(buf, size, RX_InfoStr, strlen(RX_InfoStr));
		if (p != NULL)
		{
		    if (sscanf(p + strlen(RX_InfoStr), "%ld/%ld",
			       &rxCnt, &totalCnt) == 2)
			TRxPercent = (gfloat) rxCnt / totalCnt;
		    else
		    {
			g_warning(_("Parsing error. "
				    "Cannot update progress-bar"));
			g_free(RX_InfoStr);
			RX_InfoStr = NULL;
		    }
		}
	    }
	    else if (TRxRunning == TRX_UPLOAD && TX_InfoStr)
	    {
		p = FindSubString(buf, size, TX_InfoStr, strlen(TX_InfoStr));
		if (p != NULL)
		{
		    if (sscanf(p + strlen(TX_InfoStr), "%ld", &txCnt) == 1)
			TRxPercent = (gfloat) txCnt / TxTotalCnt;
		    else
		    {
			g_warning(_("Parsing error. "
				    "Cannot update progress-bar"));
			g_free(TX_InfoStr);
			TX_InfoStr = NULL;
		    }
		}
	    }
	}

	/* file transfer하는 중이므로 송수신이 항상 일어난다고 가정 */
	ControlLabelLineStsToggle('t');
	ControlLabelLineStsToggle('r');
    }

    return TRUE;
}

/* RunRZ() {{{1 */
int
RunRZ(void)
{
    /* auto receive zmodem 등의 기능에 의해 바로 불려질 수 있으므로 여기에서
     * 필요한 여러 정보를을 다시 update.
     */
    TRxRunning = TRX_DOWNLOAD;
    ControlProtocolChange();
    PC_ButtonControl(TB_TRX_STATE);

    if (RX_InfoStr)
	ProgressBarControl(TRUE);	/* start progress-bar */

    if (pipe(TRxPipeFD) != 0)
    {
	g_warning("pipe() failed");
	return -1;
    }

    if ((TRxPid = fork()) < 0)
    {
	g_warning("fork() failed");
	close(TRxPipeFD[0]);
	close(TRxPipeFD[1]);
	return -1;
    }

    if (TRxPid > 0)
    {
	GSource *src;

	/* I'm parent */
	TRxHandler = AddChildSignalHandler(TRxPid, TRxChildSignalHandler);

	/* transfer 중에는 잡다한 동작을 수행하지 마라. */
	g_assert(TermInputChannel != NULL);
	g_io_channel_unref(TermInputChannel);
	TermInputChannel = NULL;

	src = g_main_context_find_source_by_id(NULL, TermInputID);
	if (src)
	    g_source_destroy(src);

	close(TRxPipeFD[1]);
	TRxInputChannel = g_io_channel_unix_new(TRxPipeFD[0]);
	TRxInputID = g_io_add_watch(TRxInputChannel, G_IO_IN, TRxInputHandler,
				    NULL);
	return 0;
    }

    /* I'm child */
    if (chdir(DownloadPath) == 0)
    {
	dup2(ReadFD, 0);
	dup2(ReadFD, 1);
	close(ReadFD);
	close(TRxPipeFD[0]);
	dup2(TRxPipeFD[1], 2);
	execlp("sh", "sh", "-c", RZ_Command, NULL);
    }

    /* should never reach */
    StatusShowMessage(_("fail to run '%s'"), RZ_Command);
    ProgressBarControl(FALSE);	/* end progress-bar */
    _exit(1);
}

/* TRxFileSelectOk() {{{1 */
static void
TRxFileSelectOk(GtkWidget *w, GtkWidget *win)
{
    char *p;
    const char *filename;

    w = g_dataset_get_data(G_OBJECT(GTK_FILE_SELECTION(win)->ok_button),
			   "sendbutton");

    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(win));
    if ((p = strrchr(filename, '/')) != NULL)
    {
	if (oldTRxPath)
	    g_free(oldTRxPath);
	oldTRxPath = g_strdup(filename);
	oldTRxPath[(int)(p - filename + 1)] = 0;
    }
    gtk_entry_set_text(GTK_ENTRY(FilenameEntry), p + 1);

    gtk_widget_destroy(win);
    gtk_grab_add(TRxFileQueryWin);
    gtk_widget_grab_focus(FilenameEntry);

    gtk_widget_grab_default(w);
}

/* TRxFileSelectCancel() {{{1 */
static void
TRxFileSelectCancel(GtkWidget *w, GtkWidget *win)
{
    w = g_dataset_get_data(G_OBJECT(GTK_FILE_SELECTION(win)->ok_button),
			   "sendbutton");

    gtk_widget_destroy(win);
    gtk_grab_add(TRxFileQueryWin);

    gtk_widget_grab_focus(FilenameEntry);

    gtk_widget_grab_default(w);
}

/* TRxFileBrowse() {{{1 */
static void
TRxFileBrowse(GtkWidget *w, gpointer data)
{
    GtkWidget *fs = gtk_file_selection_new("File Browse");

    UNUSED(w);

    gtk_widget_grab_default(GTK_WIDGET(data));

    gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(fs));
    gtk_window_set_position(GTK_WINDOW(fs), GTK_WIN_POS_MOUSE);
    g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
		     "clicked", G_CALLBACK(TRxFileSelectOk), fs);
    g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
		     "clicked", G_CALLBACK(TRxFileSelectCancel), fs);
    g_dataset_set_data(G_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
		       "sendbutton", data);
    gtk_grab_remove(TRxFileQueryWin);
    gtk_widget_show(fs);
    if (oldTRxPath)
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(fs), oldTRxPath);
}

/* TRxFileQueryCancel() {{{1 */
static void
TRxFileQueryCancel(void)
{
    TRxSendCancelString();
    gtk_widget_destroy(TRxFileQueryWin);
    TRxRunning = TRX_IDLE;
}

/* trxDelay() {{{1 */
static void
trxDelay(unsigned long l)
{
    volatile unsigned long c = l;
    while (c--)
	;
}

/* sendAsciiFile() {{{1 */
static int
sendAsciiFile(FILE *fp)
{
    int ci, r = 1;
    char c;
    static unsigned long sent = 0;

    if (TRxRunning == TRX_IDLE || (ci = fgetc(fp)) == EOF)
    {
	TRxRunning = TRX_IDLE;
	fclose(fp);
	PC_ButtonControl(TB_CONNECT_STATE);
	ProgressBarControl(FALSE);
	SoundPlay(SoundTRxEndFile);
	sent = 0;
	return FALSE;
    }

    ++sent;
    TRxPercent = (gfloat) sent / TxTotalCnt;

    c = (char) ci;
    if (c == '\n' || c == '\r')
    {
	if (TRxProtocol == TRX_PROT_RAW_ASCII)
	{
	    r = write(ReadFD, &c, 1);
	}
	else
	{
	    if (EnterConvert == ENTER_CR)
		c = '\n';
	    else if (EnterConvert == ENTER_LF)
		c = '\r';
	    else if (EnterConvert == ENTER_CRLF)
	    {
		c = '\r';
		r = write(ReadFD, &c, 1);
		c = '\n';
	    }
	    r = write(ReadFD, &c, 1);
	}
	if (IdleGapBetweenLine)
	    /* usleep (or select/nanosleep) 사용시 속도가 너무 느리다. */
	    trxDelay(IdleGapBetweenLine);
    }
    else
    {
	r = write(ReadFD, &c, 1);
	if (IdleGapBetweenChar)
	    trxDelay(IdleGapBetweenChar);
    }

    if (r != 1)
	g_warning("ascii file send error\n");

    ControlLabelLineStsToggle('t');
    return TRUE;
}

/* RunSendAscii() {{{1 */
static int
RunSendAscii(const char *filename)
{
    FILE *fp;

    if ((fp = fopen(filename, "r")) != NULL)
    {
	ProgressBarControl(TRUE);
	gtk_idle_add((GtkFunction) sendAsciiFile, fp);
    }

    return 0;
}

/* RunSZ() {{{1 */
static int
RunSZ(const char *cmd, const char *filename)
{
    char buf[MAX_PATH];

    if (!UseModem && UseZtelnet)
    {
	g_assert(Term != NULL);

	/* ztelnet 사용시 ztelnet의 명령어 모드로 전환하고,,
	 * sz 명령어를 내린다.
	 */
	buf[0] = ']';
	Term->remoteWrite((guchar*) buf, 1);
	g_usleep(20000);
	g_snprintf(buf, sizeof(buf), "%s %s\r", cmd, filename);
	Term->remoteWrite((guchar*) buf, -1);
	return -1;
    }

    if (TX_InfoStr)
	ProgressBarControl(TRUE);	/* start progress-bar */

    /* give newline to terminal */
    buf[0] = '\n';
    Term->localWrite(Term, (guchar*) buf, 1);

    if (pipe(TRxPipeFD) != 0)
    {
	g_warning("pipe() failed");
	return -1;
    }

    if ((TRxPid = fork()) < 0)
    {
	g_warning("fork() failed");
	close(TRxPipeFD[0]);
	close(TRxPipeFD[1]);
	return -1;
    }

    if (TRxPid > 0)
    {
	GSource *src;

	/* I'm parent */
	TRxHandler = AddChildSignalHandler(TRxPid, TRxChildSignalHandler);

	/* transfer 중에는 잡다한 동작을 수행하지 마라. */
	g_assert(TermInputChannel != NULL);
	g_io_channel_unref(TermInputChannel);
	TermInputChannel = NULL;

	src = g_main_context_find_source_by_id(NULL, TermInputID);
	if (src)
	    g_source_destroy(src);

	close(TRxPipeFD[1]);
	TRxInputChannel = g_io_channel_unix_new(TRxPipeFD[0]);
	TRxInputID = g_io_add_watch(TRxInputChannel, G_IO_IN, TRxInputHandler,
				    NULL);
	return 0;
    }

    /* I'm child */
    dup2(ReadFD, 0);
    dup2(ReadFD, 1);
    close(ReadFD);
    dup2(TRxPipeFD[1], 2);
    close(TRxPipeFD[0]);
    g_snprintf(buf, sizeof(buf), "%s %s", cmd, filename);
    execlp("sh", "sh", "-c", buf, NULL);

    /* should never reach */
    StatusShowMessage(_("fail to run '%s'"), cmd);
    ProgressBarControl(FALSE);	/* end progress-bar */
    _exit(1);
}

/* TRxSendFile() {{{1 */
int
TRxSendFile(const char *filename)
{
    int sts;
    char *s;
    struct stat st;

    if ((s = GetRealFilename(filename, UploadPath)) == NULL)
    {
	g_warning(_("Cannot access '%s'."), filename);
	TRxSendCancelString();
	TRxRunning = TRX_IDLE;
	return -1;
    }

    if (stat(s, &st))
    {
	g_warning(_("Cannot stat '%s'."), filename);
	TRxSendCancelString();
	TRxRunning = TRX_IDLE;
	return -1;
    }

    /* script에서 바로 불려지는 경우를 위해 여기에서 upload로 */
    TRxRunning = TRX_UPLOAD;

    TxTotalCnt = st.st_size;

    PC_ButtonControl(TB_TRX_STATE);

    sts = -1;
    switch (TRxProtocol)
    {
	case TRX_PROT_ASCII:
	    /* TODO: ascii support (not raw-ascii) */
	    /* fall through at this time. */
	case TRX_PROT_RAW_ASCII:
	    sts = RunSendAscii(s);
	    break;

	case TRX_PROT_KERMIT:
	    /* TODO: kermit support */
	    break;

	case TRX_PROT_XMODEM:
	    sts = RunSZ(SX_Command, s);
	    break;

	case TRX_PROT_YMODEM:
	    sts = RunSZ(SB_Command, s);
	    break;

	case TRX_PROT_ZMODEM:
	    sts = RunSZ(SZ_Command, s);
	    break;

	default:
	    FatalExit(NULL, "Invalid transfer protocol");
	    break;	/* never reach */
    }
    g_free(s);

    if (sts < 0)
    {
	/* send 함수에서 running이 false가 된 경우 */
	PC_ButtonControl(TB_CONNECT_STATE);
	TRxRunning = TRX_IDLE;
	return -1;
    }

    return 0;
}

/* TRxProtocolSetFromString() {{{1 */
gint
TRxProtocolSetFromString(const char *s)
{
    if (!g_ascii_strcasecmp(s, "ascii"))
	TRxProtocol = TRX_PROT_ASCII;
    else if (!g_ascii_strcasecmp(s, "xmodem"))
	TRxProtocol = TRX_PROT_XMODEM;
    else if (!g_ascii_strcasecmp(s, "zmodem"))
	TRxProtocol = TRX_PROT_ZMODEM;
    else if (!g_ascii_strcasecmp(s, "raw-ascii"))
	TRxProtocol = TRX_PROT_RAW_ASCII;
    else if (!g_ascii_strcasecmp(s, "ymodem"))
	TRxProtocol = TRX_PROT_YMODEM;
    else if (!g_ascii_strcasecmp(s, "kermit"))
	TRxProtocol = TRX_PROT_KERMIT;
    else
    {
	g_warning(_("Unknown protocol = %s"), s);
	return -1;
    }

    ControlProtocolChange();
    return 0;
}

/* SendFile() {{{1 */
static void
SendFile(void)
{
    const char *s;

    s = gtk_entry_get_text(GTK_ENTRY(FilenameEntry));
    if (*s)
    {
	char *f = NULL;
	if (oldTRxPath && *s != '/')
	{
	    f = g_strdup_printf("%s%s", oldTRxPath, s);
	    s = f;
	}
	TRxSendFile(s);
	if (f)
	    g_free(f);
	gtk_widget_destroy(TRxFileQueryWin);
    }
}

/* TRxFileQuery() {{{1 */
static void
TRxFileQuery(gboolean downFlag)
{
    GtkWidget *vbox, *button, *sendbutton, *label, *hbox, *sep;
    char buf[80];

    TRxFileQueryWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(TRxFileQueryWin), 10);
    if (downFlag)
	g_snprintf(buf, sizeof(buf), _("File Download with %s"),
		   TRxProtocolNames[TRxProtocol]);
    else
	g_snprintf(buf, sizeof(buf), _("File Upload with %s"),
		   TRxProtocolNames[TRxProtocol]);
    gtk_window_set_title(GTK_WINDOW(TRxFileQueryWin), buf);
    gtk_window_set_position(GTK_WINDOW(TRxFileQueryWin), GTK_WIN_POS_MOUSE);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(TRxFileQueryWin), vbox);

    hbox = gtk_hbox_new(FALSE, 10);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    label = gtk_label_new(_("Filename:"));
    gtk_container_add(GTK_CONTAINER(hbox), label);

    FilenameEntry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(hbox), FilenameEntry);
    GTK_WIDGET_SET_FLAGS(FilenameEntry, GTK_CAN_FOCUS);
    g_signal_connect(G_OBJECT(FilenameEntry), "activate",
		     G_CALLBACK(SendFile), NULL);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 5);

    hbox = gtk_hbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);

    if (downFlag)
	sendbutton = gtk_button_new_with_label(_("Receive"));
    else
	sendbutton = gtk_button_new_with_label(_("Send"));
    GTK_WIDGET_SET_FLAGS(sendbutton, GTK_CAN_DEFAULT | GTK_RECEIVES_DEFAULT);
    gtk_container_add(GTK_CONTAINER(hbox), sendbutton);
    g_signal_connect(G_OBJECT(sendbutton), "clicked",
		     G_CALLBACK(SendFile), NULL);
    gtk_widget_grab_default(sendbutton);

    button = gtk_button_new_with_label(_("Browse"));
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(TRxFileBrowse), sendbutton);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_container_add(GTK_CONTAINER(hbox), button);
    g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(TRxFileQueryCancel), NULL);

    gtk_widget_show_all(TRxFileQueryWin);
    gtk_widget_grab_focus(FilenameEntry);
}

/* DownloadFileQuery() {{{1 */
int
DownloadFileQuery(void)
{
    if (TRxRunning)
    {
	StatusShowMessage(_("Downloading in progress..."));
	return 0;
    }

    TRxRunning = TRX_DOWNLOAD;
    TRxFileQuery(TRUE);
    return 0;
}

/* UploadFileQuery() {{{1 */
int
UploadFileQuery(void)
{
    if (TRxRunning)
    {
	StatusShowMessage(_("Uploading in progress..."));
	return 0;
    }

    TRxRunning = TRX_UPLOAD;
    TRxFileQuery(FALSE);
    return 0;
}

/* ZmodemInputFilter() {{{1 */
static int
ZmodemInputFilter(char *str, int len)
{
    register int i;
    static const char zmSig[] = "**\030B0";
    static const char *zmSigPtr = zmSig;

    if (AutoRunZmodem)
    {
	for (i = 0; i < len; i++)
	{
	    if (*zmSigPtr == '\0')
	    {
		zmSigPtr = zmSig;
		if (str[i] == '0')
		{
		    TRxProtocol = TRX_PROT_ZMODEM;
		    RunRZ();
		    break;
		}
		else if (str[i] == '1')
		{
		    TRxProtocol = TRX_PROT_ZMODEM;
		    gtk_timeout_add(100, (GtkFunction) UploadFileQuery, NULL);
		    break;
		}
	    }
	    else
	    {
		if (str[i] != *zmSigPtr)
		    zmSigPtr = zmSig;
		else
		    zmSigPtr++;
	    }
	}
    }
    return len;
}

/* TRxInit() {{{1 */
void
TRxInit(void)
{
    InputFilterList = g_slist_append(InputFilterList, ZmodemInputFilter);
}

