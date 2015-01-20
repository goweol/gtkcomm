/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Modem control
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

#include <termios.h>
#include <fcntl.h>	/* access() */
#include <sys/ioctl.h>	/* TIOCMGET */
#include <sys/stat.h>	/* stat() */
#include <errno.h>
#include <signal.h>	/* kill() */
#include <ctype.h>	/* isdigit() */

#include "pcMain.h"
#include "pcTerm.h"	/* Term */

/* Definitions {{{1 */

#define MODEM_CALL_PREFIX	 "atdt"
#define MODEM_CALL_SUFFIX	 "\r"
#define MODEM_DIAL_CANCEL_STR	 "\r"
#define MODEM_DIAL_WAIT_INTERVAL (30 * 1000)

/* Global variables/functions {{{1 */

/* 설정 파일에서 읽어들인 phone info 목록 */
GSList *ModemInfoList = NULL;
ModemInfoType *ModemInfo = NULL;

guint32 IdleGuardInterval;
char *IdleGuardString;
gboolean DoLocalEcho;
gboolean DoRemoteEcho;
gboolean DoLock;
guint32 IdleGapBetweenChar;
guint32 IdleGapBetweenLine;

guint32 ModemMaxHistory;
guint32 ModemHistoryBufSize;

/* Local function prototypes {{{1 */

static void modemInfoFree(void);
static void modemAddHistory(const guchar *s, int len);
static void modemHistoryDelete(void);
static void modemHistorySend(GtkTreeView *v);
static void modemHistorySendCR(GtkTreeView *v);
static int modemUnlock(void);

/* Local variables {{{1 */

static struct termios origTC;
static gboolean ModemInitialized = FALSE;
static gboolean ModemConnected = FALSE;
static gboolean ModemLocked = FALSE;
static gboolean ModemHangupInProgress = FALSE;
static gboolean ModemDirectConnect = FALSE;
static guint ModemDialTimeoutTag = 0;
static gboolean ModemDialInProgress = FALSE;
static ModemInfoType DfltModemInfo;
static char **modemHistoryBuf = NULL;
static guint modemHistoryIdx, modemHistoryPos;
static gboolean modemHistoryWrapped;

/* ModemSetSpeed() {{{1 */
gboolean
ModemSetSpeed(guint32 modemSpeed)
{
    long speed;
    struct termios tty;

    if (UseModem)
    {
	tcgetattr(ReadFD, &tty);

	switch (modemSpeed)
	{
	    case 0: speed = B0; break;
	    case 300: speed = B300; break;
	    case 600: speed = B600; break;
	    case 1200: speed = B1200; break;
	    case 2400: speed = B2400; break;
	    case 4800: speed = B4800; break;
	    case 9600: speed = B9600; break;
	    case 19200: speed = B19200; break;
	    case 38400: speed = B38400; break;
	    case 57600: speed = B57600; break;
	    case 115200: speed = B115200; break;
	    default:
		speed = -1;
		break;
	}

	if (speed >= 0)
	{
	    cfsetospeed(&tty, (speed_t) speed);
	    cfsetispeed(&tty, (speed_t) speed);

	    if (tcsetattr(ReadFD, TCSANOW, &tty) == 0)
	    {
		ControlBaudrateChange(modemSpeed);
		ModemInfo->speed = modemSpeed;
		return TRUE;
	    }
	}
    }
    if (ModemInfo)
	ModemInfo->speed = modemSpeed;
    return FALSE;
}

/* ModemGetSpeed() {{{1 */
guint32
ModemGetSpeed(void)
{
    if (ModemInfo)
	return ModemInfo->speed;
    else
	return (guint32) 115200;
}

/* ModemSetParams() {{{1 */
static gboolean
ModemSetParams(ModemInfoType *info)
{
    struct termios tty;

    if (ModemSetSpeed(info->speed) == FALSE)
	return FALSE;

    tcgetattr(ReadFD, &tty);

    switch (info->bits)
    {
	case 5:
	    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5;
	    break;
	case 6:
	    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6;
	    break;
	case 7:
	    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;
	    break;
	case 8:
	default:
	    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	    break;
    }

    tty.c_iflag = IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    tty.c_cflag &= ~(PARENB | PARODD);
    if (!g_ascii_strcasecmp(info->parity, "EVEN"))
	tty.c_cflag |= PARENB;
    else if (!g_ascii_strcasecmp(info->parity, "ODD"))
	tty.c_cflag |= PARODD;

    if (info->useSWF)
	tty.c_iflag |= IXON | IXOFF;
    else
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    if (info->useHWF)
	tty.c_cflag |= CRTSCTS;
    else
	tty.c_cflag &= ~CRTSCTS;

    return (tcsetattr(ReadFD, TCSANOW, &tty) == 0);
}

/* ModemResetTTY() {{{1 */
int
ModemResetTTY(void)
{
    struct termios tty;

    if (ModemInfo && ReadFD >= 0)
    {
	tcgetattr(ReadFD, &tty);
	tcsetattr(ReadFD, TCSAFLUSH, &tty);
	return ModemSetParams(ModemInfo);
    }
    return -1;
}

/* ModemGetConnectionStatus() {{{1 */
gboolean
ModemGetConnectionStatus(void)
{
    unsigned int v;

    if (!ModemInitialized)
	return FALSE;

    if (ModemDirectConnect)
    {
	if (ModemHangupInProgress)
	    return FALSE;
	else
	    return TRUE;
    }

    v = 0;
    if (ReadFD != -1)
	ioctl(ReadFD, TIOCMGET, &v);
    return (v & TIOCM_CAR) ? TRUE : FALSE;
}

/* ModemRestore() {{{1 */
static void
ModemRestore(void)
{
    tcsetattr(ReadFD, TCSANOW, &origTC);
}

/* ModemDtrToggle() {{{1 */
static void
ModemDtrToggle(int fd)
{
    struct termios tty, old;

    /* POSIX: set baudrate to 0 and back */
    tcgetattr(fd, &tty);
    memcpy(&old, &tty, sizeof(old));
    cfsetospeed(&tty, B0);
    cfsetispeed(&tty, B0);
    tcsetattr(fd, TCSANOW, &tty);
    sleep(1);
    tcsetattr(fd, TCSANOW, &old);
}

/* ModemConnectionCheck() {{{1 */
int
ModemConnectionCheck(gpointer dummy)
{
    guint32 currTime;
    GSList *slist;
    guint32 u32;
    TermType *term;
    static guint32 connectTime = 0;
    static gint32 tick;

    if (ModemConnected != ModemGetConnectionStatus())
    {
	ModemConnected = !ModemConnected;
	if (ModemConnected)
	{
	    tick = -1;
	    connectTime = gdk_time_get();

	    /* logging */
	    if (LogFile)
		OpenLogFile(ModemInfo->name, ModemInfo->number);

	    StatusShowMessage(_("Connected..."));
	    SoundPlay(SoundConnectFile);
	    PC_State = PC_RUN_STATE;
	    PC_ButtonControl(TB_CONNECT_STATE);

	    for (slist = ConnectFuncList; slist;)
	    {
		void (*func) (void) = slist->data;

		/* list remove가 일어나기 전에 다음 list를 얻어오는 것이
		 * 안전하다. func가 자기 자신을 삭제할 수가 있기 때문이다.
		 */
		slist = slist->next;

		(*func) ();
	    }
	}
	else
	{
	    LastOutTime += 800000000UL;
	    StatusShowMessage(_("Disconnected..."));
	    SoundPlay(SoundDisconnectFile);
	    SetWindowTitle(NULL);	/* set to default title */
	    PC_State = PC_IDLE_STATE;
	    PC_ButtonControl(TB_IDLE_STATE);

	    if (DoLock)
		modemUnlock();

	    ModemInitialized = FALSE;

	    for (slist = DisconnectFuncList; slist;)
	    {
		void (*func) (void) = slist->data;

		/* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다.
		 * func가 자기 자신을 삭제할 수가 있기 때문이다.
		 */
		slist = slist->next;

		(*func) ();
	    }

	    CloseLogFile(tick);

	    ScriptCancel(); /* 현재 수행중인 스크립트가 있는 경우 취소 */
	    if (connectTime > 0 && ModemInfo->logoutScript)
		ScriptRunFromFile(ModemInfo->logoutScript);

	    modemInfoFree();
	    connectTime = 0;
	    close(ReadFD);
	    ReadFD = -1;
	}
    }

    if (ModemConnected)
    {
	currTime = gdk_time_get();

	if (IdleGuardString && *IdleGuardString && TRxRunning == TRX_IDLE
	    && !ScriptRunning)
	    if (LastOutTime + (IdleGuardInterval * 1000) < currTime)
		Term->remoteWrite((guchar*) IdleGuardString, -1);

	if (AR_CheckFlag)
	{
	    AR_CheckFlag = FALSE;
	    term = Term;
	    AutoResponseCheckRun((char*) term->buf
				 + term->col * TERM_LINENUM(term->currY),
				 term->currX);
	}

	u32 = (currTime - connectTime) / 1000;
	if (u32 != (guint32) tick)
	{
	    tick = u32;
	    StatusShowTime(tick);
	}
    }
    return TRUE;

    dummy = 0;	/* to make gcc happy */
}

/* ModemWrite() {{{1 */
int
ModemWrite(const guchar *s, int len)
{
    register int i;
    guchar *p;
    gchar *cp = NULL;
    gint new_len;
    guchar c;

    if (s == NULL)
	return 0;
    if (len < 0)
	len = strlen((char*) s);
    if (len == 0)
	return 0;

    LastOutTime = gdk_time_get();
    if (DoLocalEcho)
    {
	for (i = 0; i < len; i++)
	{
	    if (s[i] != '\r')
		Term->localWrite(Term, s + i, 1);
	    else
	    {
		c = '\n';
		Term->localWrite(Term, s + i, 1);
		Term->localWrite(Term, &c, 1);
	    }
	}
    }

    p = (guchar*) s;
    new_len = len;
    if (Term->isRemoteUTF8)
    {
	gsize bw;

	if (gu_euckr_validate_conv((char*) s, len, &cp, &bw) == TRUE)
	{
	    p = (guchar *) cp;
	    new_len = bw;
	}
    }

    if (ModemDirectConnect)
    {
	if (modemHistoryBuf && TRxRunning == TRX_IDLE)
	    modemAddHistory(s, len);

	for (i = 0; i < new_len; i++, p++)
	{
	    if (write(ReadFD, p, 1) != 1)
	    {
		new_len = i;
		break;
	    }
	    if (IdleGapBetweenLine && (*p == '\n' || *p == '\r'))
		g_usleep(IdleGapBetweenLine);
	    if (IdleGapBetweenChar)
		g_usleep(IdleGapBetweenChar);
	}
    }
    else
	new_len = write(ReadFD, p, new_len);

    ControlLabelLineStsToggle('t');

    if (cp)
	g_free(cp);

    return new_len;
}

/* modemLock() {{{1 */
static int
modemLock(int fd)
{
#if 0
    char buf[MAX_PATH], temp[128];

    if (ModemLocked || ModemInfo == NULL)
	return 0;

    strncpy(temp, strrchr(ModemInfo->device, '/') + 1, sizeof(temp) - 1);
    temp[sizeof(temp)-1] = 0;
    g_snprintf(buf, sizeof(buf), "gtkcomm_lock -l %s", temp);
    if (system(buf) == 0)
    {
	ModemLocked = TRUE;
	return 0;
    }
#else
    if (flock(fd, LOCK_EX | LOCK_NB) == 0)
    {
	ModemLocked = TRUE;
	return 0;
    }
#endif

    ModemLocked = FALSE;
    g_warning(_("Modem lock fail."));
    return -1;
}

/* modemUnlock() {{{1 */
static int
modemUnlock(void)
{
#if 0
    char buf[MAX_PATH], temp[128];

    if (!ModemLocked || !ModemInfo)
	return 0;

    strncpy(temp, strrchr(ModemInfo->device, '/') + 1, sizeof(temp) - 1);
    temp[sizeof(temp)-1] = 0;
    g_snprintf(buf, sizeof(buf), "gtkcomm_lock -u %s", temp);
    if (system(buf) == 0)
    {
	ModemLocked = FALSE;
	return 0;
    }
#else
    if (ReadFD < 0 || flock(ReadFD, LOCK_UN | LOCK_NB) == 0)
    {
	ModemLocked = FALSE;
	return 0;
    }
#endif

    return -1;
}

/* ModemCheckDialNumber() {{{1 */
static gint
ModemCheckDialNumber(const char *number)
{
    /* check number. I allow next format:
     *    9,0333-222-1111
     * 즉, 십진수, 마이너스, 콤마만 사용가능.
     */
    for (; *number; number++)
	if (!isdigit(*number) && *number != '-' && *number != ',')
	    return -1;
    return 0;
}

/* ModemDialTimeoutCB() {{{1 */
static gint
ModemDialTimeoutCB(gpointer data)
{
    ModemDialTimeoutTag = 0;
    StatusShowMessage(_("Modem connect timeout!"));
    ModemWrite((guchar*) MODEM_DIAL_CANCEL_STR, -1);
    PC_Sleep(1000);
    ModemDialInProgress = FALSE;
    return FALSE;
    data = NULL;
}

/* ModemResponseWaitFilter() {{{1 */
static gint
ModemResponseWaitFilter(char *buf, int len)
{
    int i;
    static int log = FALSE;
    static char response[20];
    static char *p = response;

    if (buf == NULL || len <= 0)
    {
	p = response;
	log = FALSE;
	return 0;
    }

    for (i = 0; i < len; i++)
    {
	if (buf[i] == 10)
	{
	    if (log)
	    {
		*p = '\0';
		if (strstr(response, "BUSY"))
		{
		    StatusShowMessage(_("BUSY!"));
		    PC_Sleep(1000);
		    /* TODO: dial next number */
		}
		else if (strstr(response, "NO CARRIER")
			 || strstr(response, "NO DIALTONE")
			 || strstr(response, "OK") || strstr(response, "VOICE"))
		{
		    PC_Sleep(1000);
		    /* TODO: dial next number */
		}
		else
		{
		    ModemConnectionCheck(NULL);
		    /* TODO: hide if there is a dialog */
		}

		gtk_timeout_remove(ModemDialTimeoutTag);
		ModemDialTimeoutTag = 0;
		InputFilterList = g_slist_remove(InputFilterList,
						 ModemResponseWaitFilter);
		p = response;
		log = FALSE;
		ModemDialInProgress = FALSE;
		break;
	    }
	    else
		log = TRUE;
	}
	else
	{
	    if (log && p < (buf + sizeof(buf) - 1))
		*p++ = buf[i];
	}
    }
    return 0;
}

/* ModemDialNumber() {{{1 */
static void
ModemDialNumber(ModemInfoType *info)
{
    char *p;

    if (ModemInitialized)
    {
	StatusShowMessage(_("Modem already initialized. Disconnect first!"));
	return;
    }

    if (ModemCheckDialNumber(info->number) < 0)
    {
	StatusShowMessage(_("Invalid dial number '%s'"), info->number);
	return;
    }

    ModemDialInProgress = TRUE;

    ModemSetSpeed(info->speed);
    ModemWrite((guchar*) MODEM_CALL_PREFIX, -1);
    for (p = info->number; *p; p++)
    {
	if (*p == ',')
	    PC_Sleep(2000);	/* maybe-NEVER-todo: sleep만으로 충분한가? */
	if (isdigit(*p))
	    ModemWrite((guchar*) p, 1);
    }
    ModemWrite((guchar*) MODEM_CALL_SUFFIX, -1);

    StatusShowMessage(_("Try to dial '%s'"), info->number);

    if (ModemDialTimeoutTag > 0)
	gtk_timeout_remove(ModemDialTimeoutTag);
    ModemDialTimeoutTag = gtk_timeout_add(MODEM_DIAL_WAIT_INTERVAL,
					  (GtkFunction) ModemDialTimeoutCB,
					  NULL);

    ModemResponseWaitFilter(NULL, -1);	/* init */
    InputFilterList = g_slist_prepend(InputFilterList, ModemResponseWaitFilter);
}

/* modemInfoFree() {{{1 */
static void
modemInfoFree(void)
{
    if (ModemInfo == &DfltModemInfo)
    {
	if (ModemInfo->number)
	{
	    g_free(ModemInfo->number);
	    ModemInfo->number = NULL;
	}
    }
    ModemInfo = NULL;
}

/* InitModem() {{{1 */
static int
InitModem(ModemInfoType *info)
{
    int fd;

    g_return_val_if_fail(info != NULL, -1);

    ModemInfo = info;

    if (info->loginScript)
    {
	ScriptRunFromFile(info->loginScript);
	PC_Sleep(100);
    }

    if (access(info->device, R_OK | W_OK))
    {
	FatalExit(NULL, "Cannot access modem device '%s'.", info->device);
	return -1;
    }

    if ((fd = open(info->device, O_RDWR)) < 0)
    {
	FatalExit(NULL, "Cannot open modem device '%s'.", info->device);
	return -1;
    }

    if (DoLock)
    {
	if (modemLock(fd) < 0)
	{
	    close(fd);
	    StatusShowMessage(_("Cannot lock device. "
				"Modem cannot initialized."));
	    return -1;
	}
    }

    if (tcgetattr(fd, &origTC))
    {
	close(fd);
	FatalExit(NULL, "Cannot get status of the modem device '%s'.",
		  info->device);
	return -1;
    }

    ReadFD = fd;
    if (ModemSetParams(info) == FALSE)
    {
	ScriptCancel();
	ReadFD = -1;
	close(fd);
	FatalExit(NULL, "Modem device '%s' initialization error.",
		  info->device);
	return -1; /* SHOULD NEVER REACH */
    }

    if (info->initString)
	ModemWrite((guchar*) info->initString, -1);

    ModemConnected = FALSE;

    if (info->directConn)
	ModemDirectConnect = TRUE;
    else
    {
	ModemDirectConnect = FALSE;
	ModemDialNumber(info);
    }

    gtk_timeout_add(DEFAULT_CONNECT_CHK_INTERVAL, ModemConnectionCheck,
		    NULL);

    ModemHistoryNew();

    ModemInitialized = TRUE;
    return 0;
}

/* ModemExit() {{{1 */
void
ModemExit(void)
{
    GSList *l;
    ModemInfoType *mi;

    for (l = ModemInfoList; l; l = l->next)
    {
	if ((mi = (ModemInfoType *) l->data) == NULL)
	    continue;

	if (mi->name)
	    g_free(mi->name);
	if (mi->number)
	    g_free(mi->number);
	if (mi->parity)
	    g_free(mi->parity);
	if (mi->loginScript)
	    g_free(mi->loginScript);
	if (mi->logoutScript)
	    g_free(mi->logoutScript);
	if (mi->device)
	    g_free(mi->device);
	if (mi->initString)
	    g_free(mi->initString);
	g_free(mi);
    }
    if (ModemInfoList)
    {
	g_slist_free(ModemInfoList);
	ModemInfoList = NULL;
    }
}

/* ModemConnectByName() {{{1 */
int
ModemConnectByName(const char *name)
{
    GSList *slist;
    ModemInfoType *info = NULL;

    if (ModemInitialized)
    {
	StatusShowMessage(_("Currently connected. Close first..."));
	return FALSE;
    }

    g_return_val_if_fail(ModemInfo == NULL, -1);
    g_return_val_if_fail(Term != NULL, -1);
    g_return_val_if_fail(name != NULL, -1);
    g_return_val_if_fail(*name != 0, -1);

    Term->remoteWrite = ModemWrite;

    UseModem = TRUE;
    for (slist = ModemInfoList; slist; slist = slist->next)
    {
	info = slist->data;
	if (!g_ascii_strcasecmp(info->name, name) || !g_ascii_strcasecmp(info->number, name))
	    break;
    }
    if (!slist)	/* try with default setting */
    {
	if (ModemCheckDialNumber(name) < 0)
	{
	    StatusShowMessage(_("'%s' is not a valid dial number"), name);
	    return FALSE;
	}

	info = &DfltModemInfo;

	if (info->number)
	    g_free(info->number);

	memset(info, 0, sizeof(ModemInfoType));

	info->name = "UNKNOWN";
	info->number = g_strdup(name);
	info->speed = DEFAULT_MODEM_SPEED;
	info->parity = DEFAULT_MODEM_PARITY;
	info->bits = DEFAULT_MODEM_BITS;
	info->useSWF = FALSE;
	info->useHWF = TRUE;
	info->device = DEFAULT_MODEM_DEVICE;
	info->initString = DEFAULT_MODEM_INIT;
    }

    TermConvOpen(Term, info->remoteCharset);
    if (InitModem(info) < 0)
    {
	modemInfoFree();
	return FALSE;
    }

    TermInputChannel = g_io_channel_unix_new(ReadFD);
    TermInputID = g_io_add_watch(TermInputChannel, G_IO_IN, TermInputHandler,
				 NULL);
    return FALSE;
}

/* ModemHangup() {{{1 */
void
ModemHangup(void)
{
    if (ModemInitialized)
    {
	if (ModemDialInProgress)
	{
	    InputFilterList = g_slist_remove(InputFilterList,
					     ModemResponseWaitFilter);
	    ModemWrite((guchar*) MODEM_DIAL_CANCEL_STR, -1);
	    gtk_timeout_remove(ModemDialTimeoutTag);
	    ModemDialTimeoutTag = 0;
	    ModemDialInProgress = FALSE;
	}
	if (ModemGetConnectionStatus())
	{
	    ModemDtrToggle(ReadFD);
	    ModemHangupInProgress = TRUE;	/* hack for direct connection */
	    ModemConnectionCheck(NULL);
	    ModemHangupInProgress = FALSE;
	}
	ModemRestore();
	modemHistoryDelete();
	ModemInitialized = FALSE;
    }

    if (TermInputChannel)
    {
	GSource *src;

	g_io_channel_unref(TermInputChannel);
	TermInputChannel = NULL;

	src = g_main_context_find_source_by_id(NULL, TermInputID);
	if (src)
	    g_source_destroy(src);
    }

    if (ReadFD >= 0)
    {
	close(ReadFD);
	ReadFD = -1;
    }

    Term->remoteWrite = DummyFromTerm;

    modemInfoFree();
}
/* }}} */

/* MODEM USER COMMAND HISTORY */

static GtkWidget *modemHistWin = NULL;
static GtkWidget *modemHistView;
static GtkTreeStore *modemHistModel;

/* ModemOpenHistoryWin() {{{1 */
void
ModemOpenHistoryWin(void)
{
    GtkWidget *vbox, *swin, *sep, *hbox, *button;
    GtkTreeStore *model;
    GtkTreeIter iter;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    uint i, last, win_width = 400;

    if (modemHistWin == NULL)
    {
	modemHistWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(modemHistWin), "destroy",
			   G_CALLBACK(gtk_widget_destroyed),
			   &modemHistWin);
	gtk_window_set_title(GTK_WINDOW(modemHistWin), "Command History");
	gtk_container_set_border_width(GTK_CONTAINER(modemHistWin), 0);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(modemHistWin), vbox);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(swin), 5);
	if (Term && Term->fontWidth)
	    win_width = Term->fontWidth * 60;
	gtk_widget_set_size_request(swin, win_width, 200);
	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	model = gtk_tree_store_new(1, G_TYPE_STRING);
	last = modemHistoryWrapped? ModemMaxHistory: modemHistoryIdx;
	for (i = 0; i < last; i++)
	{
	    char *s = PC_IconvStr(modemHistoryBuf[i], -1);
	    gtk_tree_store_append(model, &iter, NULL);
	    gtk_tree_store_set(model, &iter, 0, s, -1);
	    PC_IconvStrFree(modemHistoryBuf[i], s);
	}

	modemHistModel = model;
	modemHistView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(modemHistModel));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(modemHistView), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(modemHistView), FALSE);
	gtk_container_add(GTK_CONTAINER(swin), modemHistView);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Commands");

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text",
					    0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(modemHistView), column);

	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	button = gtk_button_new_with_label(_("Send+CR"));
	gtk_container_set_border_width(GTK_CONTAINER(button), 5);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	g_signal_connect_object(G_OBJECT(button), "clicked",
				G_CALLBACK(modemHistorySendCR),
				G_OBJECT(modemHistView), G_CONNECT_SWAPPED);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

	button = gtk_button_new_with_label(_("Send"));
	gtk_container_set_border_width(GTK_CONTAINER(button), 5);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	g_signal_connect_object(G_OBJECT(button), "clicked",
				G_CALLBACK(modemHistorySend),
				G_OBJECT(modemHistView), G_CONNECT_SWAPPED);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_container_set_border_width(GTK_CONTAINER(button), 5);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	g_signal_connect_object(G_OBJECT(button), "clicked",
				G_CALLBACK(gtk_widget_destroy),
				G_OBJECT(modemHistWin), G_CONNECT_SWAPPED);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

	gtk_widget_show_all(modemHistWin);
    }
}

/* modemAddHistory() {{{1 */
static void
modemAddHistory(const guchar *s, int len)
{
    for (; len > 0; s++, len--)
    {
	int c = *s;
	if (c == '\n' || c == '\r')
	{
	    if (modemHistoryPos == 0)
		/* if user typed ENTER only */
		continue;

add_history_end:
	    modemHistoryBuf[modemHistoryIdx][modemHistoryPos] = '\0';
	    if (modemHistWin != NULL)
	    {
		GtkTreeIter iter;
		guint i;
		char *p = PC_IconvStr(modemHistoryBuf[modemHistoryIdx], -1);
		if (modemHistoryWrapped)
		{
		    GtkTreeModel *model = GTK_TREE_MODEL(modemHistModel);
		    if (gtk_tree_model_get_iter_first(model, &iter))
		    {
			for (i = 0; i < modemHistoryIdx; i++)
			    if (gtk_tree_model_iter_next(model,
							 &iter) == FALSE)
				return;
			gtk_tree_store_set(modemHistModel, &iter, 0, p, -1);
		    }
		}
		else
		{
		    gtk_tree_store_append(modemHistModel, &iter, NULL);
		    gtk_tree_store_set(modemHistModel, &iter, 0, p, -1);
		}
		PC_IconvStrFree(modemHistoryBuf[modemHistoryIdx], p);
	    }

	    if (++modemHistoryIdx == ModemMaxHistory)
	    {
		modemHistoryIdx = 0;
		modemHistoryWrapped = TRUE;
	    }
	    modemHistoryPos = 0;
	}
	else if (c == '\b')
	{
	    if (modemHistoryPos)
		modemHistoryPos--;
	}
	else
	{
	    modemHistoryBuf[modemHistoryIdx][modemHistoryPos] = c;
	    if (++modemHistoryPos == ModemHistoryBufSize)
	    {
		--modemHistoryPos;
		goto add_history_end;
	    }
	}
    }
}

/* modemHistoryRealSend() {{{1 */
static void
modemHistoryRealSend(GtkTreeView *v, gboolean addCR)
{
    GtkTreeSelection *sel = gtk_tree_view_get_selection(v);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(sel, &model, &iter) == TRUE)
    {
	gchar *s;

	gtk_tree_model_get(model, &iter, 0, &s, -1);
	ModemWrite((guchar*) s, -1);
	if (addCR)
	{
	    guchar c = '\n';
	    ModemWrite(&c, 1);
	}
	g_free(s);

	/* 여러 명령을 너무 빨리 보내면 사라지는 명령이 있다.
	 * target host에서 잃어버릴 가능성이 크며, 명령을 수행할 충분한 시간을
	 * 주는 것이 좋겠다. optional하게 처리?
	 */
	PC_Sleep(300);
    }
}

/* modemHistorySendCR() {{{1 */
static void
modemHistorySendCR(GtkTreeView *v)
{
    modemHistoryRealSend(v, TRUE);
}

/* modemHistorySend() {{{1 */
static void
modemHistorySend(GtkTreeView *v)
{
    modemHistoryRealSend(v, FALSE);
}

/* ModemHistoryNew() {{{1 */
void
ModemHistoryNew(void)
{
    modemHistoryDelete();

    if (ModemMaxHistory && ModemHistoryBufSize)
    {
	modemHistoryBuf = (char **) malloc(ModemMaxHistory * sizeof(char *));
	if (modemHistoryBuf)
	{
	    guint32 i;

	    for (i = 0; i < ModemMaxHistory; i++)
	    {
		modemHistoryBuf[i] = (char *) malloc(ModemHistoryBufSize);
		if (modemHistoryBuf[i] == NULL)
		{
		    ModemMaxHistory = i;
		    if (ModemMaxHistory == 0)
		    {
			free(modemHistoryBuf);
			modemHistoryBuf = NULL;
		    }
		    g_warning(_("History buffer alloc failed.\n"
				"Max available history number = %lu\n"),
			      (unsigned long) ModemMaxHistory);
		    break;
		}
	    }
	}
    }
    else
	modemHistoryBuf = NULL;
    modemHistoryIdx = 0;
    modemHistoryPos = 0;
    modemHistoryWrapped = FALSE;
}

/* modemHistoryDelete() {{{1 */
static void
modemHistoryDelete(void)
{
    if (modemHistoryBuf)
    {
	guint32 i;

	for (i = 0; i < ModemMaxHistory; i++)
	    free(modemHistoryBuf[i]);
	free(modemHistoryBuf);
	modemHistoryBuf = NULL;
    }
}
