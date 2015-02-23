/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Signal
 *
 * Copyright (C) 2000-2015, SungHyun Nam and various contributors
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

/* ChildSignalHandler() {{{1 */
static void
ChildSignalHandler(int sig G_GNUC_UNUSED)
{
    GSList *slist;
    SignalType *handler;
    pid_t pid;

    pid = wait(NULL);

    for (slist = SignalCallbackList; slist;)
    {
	handler = slist->data;
	slist = slist->next;

	if (handler && pid == handler->pid)
	{
	    handler->func();
	    RemoveChildSignalHandler(handler);
	    break;
	}
    }
    RegisterChildSignalHandler();
}
