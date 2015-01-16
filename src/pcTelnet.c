/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * telnet
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
#include <signal.h>
#include <sys/wait.h>	/* waitpid() */

#include "pcMain.h"
#include "pcTerm.h"

/* Global variables/functions {{{1 */

char *TelnetCommand;

/* 설정 파일에서 읽어들인 telnet info 목록 */
GSList *TelnetInfoList = NULL;

/* 현재 사용중인 telnet info. */
TelnetInfoType *TelnetInfo = NULL;

/* Local function prototypes {{{1 */

static void TelnetInfoFree(void);

/* Local variables {{{1 */

/* 최초 telnet 접속된 시간 */
static guint32 TelnetConnectTime = 0UL;

/* telnet 접속후 흐른 초단위 tick counter */
static gint32 TelnetTimeTick;

/* telnet process id */
static pid_t TelnetPid = -1;

static gboolean UseSSH = FALSE;

static TelnetInfoType DfltTelnetInfo;

/* TelnetTimeReport() {{{1 */
static int
TelnetTimeReport(void)
{
    register int newTick;
    guint32 currTime;
    TermType *term;

    currTime = gdk_time_get();
    if (TelnetConnectTime)
    {
	newTick = (currTime - TelnetConnectTime) / 1000;
	if (newTick != TelnetTimeTick)
	{
	    TelnetTimeTick = newTick;
	    StatusShowTime(newTick);
	}
    }
    if (ReadFD > 0)
    {
	if (IdleGuardString && *IdleGuardString && TRxRunning == TRX_IDLE
	    && !ScriptRunning)
	    if ((LastOutTime + IdleGuardInterval * 1000) < currTime)
		Term->remoteWrite((guchar*) IdleGuardString, -1);

	if (AR_CheckFlag)
	{
	    AR_CheckFlag = FALSE;
	    term = Term;
	    AutoResponseCheckRun((char*) term->buf
				 + term->col * TERM_LINENUM(term->currY),
				 term->currX);
	}

	return TRUE;
    }
    else
    {
	StatusShowMessage(_("Disconnected from %s."), TelnetInfo->name);
	SoundPlay(SoundDisconnectFile);

	PC_State = PC_IDLE_STATE;
	PC_ButtonControl(TB_IDLE_STATE);

	CloseLogFile(TelnetTimeTick);

	ScriptCancel(); /* 현재 수행중인 스크립트가 있는 경우 취소 */
	if (TelnetInfo->logoutScript)
	    ScriptRunFromFile(TelnetInfo->logoutScript);

	TelnetInfoFree();

	return FALSE;
    }
}

/* TelnetWrite() {{{1 */
static int
TelnetWrite(const guchar *buf, int len)
{
    guchar *p;
    gchar *cp = NULL;
    register int n;

    if (buf == NULL)
	return 0;
    if (len < 0)
	len = strlen((char*) buf);
    if (len == 0)
	return 0;

    LastOutTime = gdk_time_get();

    if (DoLocalEcho)
	Term->localWrite(Term, buf, len);

    p = (guchar *) buf;
    if (Term->isRemoteUTF8)
    {
	gsize bw;

	if (gu_euckr_validate_conv((char*) buf, len, &cp, &bw) == TRUE)
	{
	    p = (guchar *) cp;
	    len = bw;
	}
    }

    if ((n = write(ReadFD, p, len)) != len)
	g_warning(_("%s: write failed: %d, %d"), __FUNCTION__, len, n);

    ControlLabelLineStsToggle('t');

    if (cp != NULL)
	g_free(cp);

    return n;
}

/* TelnetConnectionCheck() {{{1 */
static int
TelnetConnectionCheck(char *buf, int size)
{
    register int i;
    GSList *slist;
    static const char tcpConnectStr[] = "Escape";
    static const char *p = tcpConnectStr;

    for (i = 0; i < size; i++)
    {
	if (UseSSH == FALSE && buf[i] != *p)
	    p = tcpConnectStr;
	else if (*++p == '\0' || UseSSH == TRUE)
	{
	    /* connected */

	    /* logging */
	    if (LogFile)
		OpenLogFile(TelnetInfo->name, TelnetInfo->host);

	    InputFilterList = g_slist_remove(InputFilterList,
					     TelnetConnectionCheck);

	    TelnetTimeTick = -1;
	    StatusShowMessage(_("Connected to %s."), TelnetInfo->name);
	    SoundPlay(SoundConnectFile);
	    SetWindowTitle(TelnetInfo->name);

	    for (slist = ConnectFuncList; slist;)
	    {
		void (*func) (void) = slist->data;

		/* list remove가 일어나기 전에 다음 list를 얻어오는 것이
		 * 안전하다. func가 자기 자신을 remove할 수 있기 때문이다.
		 */
		slist = slist->next;

		(*func) ();
	    }
	    gtk_timeout_add(DEFAULT_CONNECT_CHK_INTERVAL,
			    (GtkFunction) TelnetTimeReport, NULL);
	    TelnetConnectTime = gdk_time_get();

	    if (UseSSH)
		Term->localWrite(Term, (guchar*) buf, size);
	    break;
	}
    }
    return 0;
}

/* TelnetDone() {{{1 */
void
TelnetDone(void)
{
    GSList *slist;

    InputFilterList = g_slist_remove(InputFilterList, TelnetConnectionCheck);

    if (TelnetPid != -1)
    {
	/* kill the old telnet process */
	kill(TelnetPid, SIGKILL);
	waitpid(TelnetPid, NULL, 0);
	TelnetPid = -1;
    }

    if (TermInputChannel != NULL)
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

    for (slist = DisconnectFuncList; slist;)
    {
	void (*func) (void) = slist->data;

	/* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다.
	 * func가 자기 자신을 remove할 수 있기 때문이다.
	 */
	slist = slist->next;

	(*func)();
    }

    SetWindowTitle(NULL);

    if (PC_State != PC_IDLE_STATE)
    {
	ScriptCancel();
	StatusShowMessage(_("Telnet connection cancelled."));
	PC_State = PC_IDLE_STATE;
	PC_ButtonControl(TB_IDLE_STATE);
    }

    LastOutTime += 800000000UL;
    TelnetConnectTime = 0;
}

/* TelnetOpen() {{{1 */
static int
TelnetOpen(char *host, char *port, int col, int row)
{
    int result;
    pid_t pid;
    int status;
    char *telnetFullCmd;

    if (port && *port && *port != '0')
	telnetFullCmd = g_strconcat(TelnetCommand, " ", host, " ", port, NULL);
    else
	telnetFullCmd = g_strconcat(TelnetCommand, " ", host, NULL);

    UseSSH = FALSE;
    if (!strncmp(TelnetCommand, "ssh", 3))
	UseSSH = TRUE;

    result = PTY_Open(telnetFullCmd, &TelnetPid, col, row);
    g_free(telnetFullCmd);

    pid = waitpid(TelnetPid, &status, WUNTRACED | WNOHANG);
    if (pid == TelnetPid && pid >= 0)
    {
	TelnetPid = -1;
	result = -1;
    }

    if (result >= 0)
	AddChildSignalHandler(TelnetPid, TelnetDone);

    return result;
}

/* TelnetConnect() {{{1 */
static void
TelnetConnect(TelnetInfoType *info)
{
    TelnetInfo = info;

    /* no baudrate */
    ControlBaudrateChange(0);

    /* 이 호스트에 대한 특별한 환경설정이 있을 수 있으므로 login script를
     * 먼저 수행한다.
     */
    if (info->loginScript)
    {
	ScriptRunFromFile(info->loginScript);
	PC_Sleep(100); /* script가 실행되도록 */
    }

    StatusShowMessage(_("Trying to connect to %s..."), info->name);

    PC_State = PC_RUN_STATE;
    PC_ButtonControl(TB_CONNECT_STATE);

    g_assert(Term != NULL);

    TermConvOpen(Term, info->remoteCharset);

    ReadFD = TelnetOpen(info->host, info->port, Term->col, Term->row);

    if (ReadFD >= 0)
    {
	TermInputChannel = g_io_channel_unix_new(ReadFD);
	TermInputID = g_io_add_watch(TermInputChannel, G_IO_IN,
				     TermInputHandler, NULL);
	InputFilterList = g_slist_prepend(InputFilterList,
					  TelnetConnectionCheck);
	Term->remoteWrite = TelnetWrite;
    }
    else
    {
	ScriptCancel();
	g_warning(_("Cannot open telnet."));
    }
}

/* TelnetInfoFree() {{{1 */
static void
TelnetInfoFree(void)
{
    if (TelnetInfo == &DfltTelnetInfo)
    {
	if (TelnetInfo->host)
	{
	    g_free(TelnetInfo->host);
	    TelnetInfo->host = NULL;
	}
	if (TelnetInfo->name)
	{
	    g_free(TelnetInfo->name);
	    TelnetInfo->name = NULL;
	}
    }
    TelnetInfo = NULL;
}

/* TelnetConnectByHostname() {{{1 */
gint
TelnetConnectByHostname(const char *name)
{
    GSList *slist;
    TelnetInfoType *ti = NULL;

    if (TelnetPid != -1)
    {
	StatusShowMessage(_("Currently connected. Close first..."));
	return FALSE;
    }

    g_return_val_if_fail(Term != NULL, -1);
    g_return_val_if_fail(name != NULL, -1);

    Term->remoteWrite = DummyFromTerm;

    UseModem = FALSE;

    for (slist = TelnetInfoList; slist; slist = slist->next)
    {
	ti = slist->data;
	if (!strcmp(ti->name, name))
	    break;
    }
    if (!slist)
    {
	ti = &DfltTelnetInfo;
	memset(ti, 0, sizeof(TelnetInfoType));

	if (ti->host)
	    g_free(ti->host);
	if (ti->name)
	    g_free(ti->name);

	ti->host = g_strdup(name);
	ti->name = g_strdup(name);
	ti->port = NULL;
	ti->loginScript = NULL;
	ti->logoutScript = NULL;
    }

    TelnetConnect(ti);

    return FALSE;
}

/* TelnetExit() {{{1 */
void
TelnetExit(void)
{
    GSList *l;
    TelnetInfoType *ti;

    for (l = TelnetInfoList; l; l = l->next)
    {
	if ((ti = (TelnetInfoType *) l->data) == NULL)
	    continue;

	if (ti->name)
	    g_free(ti->name);
	if (ti->host)
	    g_free(ti->host);
	if (ti->port)
	    g_free(ti->port);
	if (ti->loginScript)
	    g_free(ti->loginScript);
	if (ti->logoutScript)
	    g_free(ti->logoutScript);
	g_free(ti);
    }
    if (TelnetInfoList)
    {
	g_slist_free(TelnetInfoList);
	TelnetInfoList = NULL;
    }

    TelnetInfoFree();
}

