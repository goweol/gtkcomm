/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * PTY
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
#include <signal.h>	/* SIGINT */
#if defined(__FreeBSD__)
# include <sys/types.h>
# include <libutil.h>
#else
# include <pty.h>	/* openpty() */
#endif
#include <utmp.h>	/* login_tty() */

#include "pcMain.h"

int PTY_Open(const char *cmd, pid_t *childPid, int col, int row)
{
    int slaveFD, masterFD;

    if (openpty(&masterFD, &slaveFD, NULL, NULL, NULL) == -1)
	return -1;

    if ((*childPid = fork()) < 0)
	return -1;

    if (*childPid == 0)
    {
	close(masterFD);
	login_tty(slaveFD);

	PTY_SetWinSize(0, col, row);

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	execlp("sh", "sh", "-c", cmd, NULL);
	_exit(1);
    }

    /* parent */
    close(slaveFD);
    return masterFD;
}

void PTY_SetWinSize(int fd, int col, int row)
{
#if defined(TIOCSWINSZ)
    static struct winsize ws;

    ws.ws_row = row;
    ws.ws_col = col;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    (void) ioctl(fd, TIOCSWINSZ, &ws);
#endif
}
