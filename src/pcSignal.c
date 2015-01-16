/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Signal
 *
 * Copyright (C) 2000-2002, Nam SungHyun and various contributors
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
#include <sys/wait.h>	/* wait() */

#include "pcMain.h"

static GSList *SignalCallbackList = NULL;

static void ChildSignalHandler(int sig);

/* ChildSignalHandlerExit() {{{1 */
void
ChildSignalHandlerExit(void)
{
    GSList *l;

    if (SignalCallbackList)
    {
	for (l = SignalCallbackList; l; l = l->next)
	    g_free(l->data);
	g_slist_free(SignalCallbackList);
	SignalCallbackList = NULL;
    }
}

/* RegisterChildSignalHandler() {{{1 */
void
RegisterChildSignalHandler(void)
{
    signal(SIGCHLD, ChildSignalHandler);
}

/* AddChildSignalHandler() {{{1 */
SignalType *
AddChildSignalHandler(pid_t pid, void (*func)(void))
{
    SignalType *handler = g_new(SignalType, 1);

    handler->pid = pid;
    handler->func = func;
    SignalCallbackList = g_slist_append(SignalCallbackList, handler);
    return handler;
}

/* RemoveChildSignalHandler() {{{1 */
void
RemoveChildSignalHandler(SignalType *handler)
{
    SignalCallbackList = g_slist_remove(SignalCallbackList, handler);
    g_free(handler);
}

/* RunChildSignalHandler() {{{1 */
static gint
RunChildSignalHandler(void (*func)(void))
{
    (*func)();
    return FALSE;	/* end timer */
}

/* ChildSignalHandler() {{{1 */
static void
ChildSignalHandler(int sig)
{
    GSList *slist;
    SignalType *handler;
    pid_t pid;

    pid = wait(NULL);

    for (slist = SignalCallbackList; slist;)
    {
	handler = slist->data;

	/* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다.
	 * 밑에서 list remove후 break하므로 여기는 don't care이지만...
	 */
	slist = slist->next;

	if (handler && pid == handler->pid)
	{
	    /* handler를 바로 수행할 수 없는 데, 그것은 현재의 glib handler가
	     * 시그널에 의해 interrupt 당하는 경우를 고려하지 않고 있기 때문.
	     */
	    gtk_timeout_add(1, (GtkFunction) RunChildSignalHandler,
			    handler->func);
	    SignalCallbackList = g_slist_remove(SignalCallbackList, handler);
	    g_free(handler);
	    break;
	}
    }
    RegisterChildSignalHandler();
    sig = 0;	/* to make gcc happy */
}
