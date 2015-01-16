
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "pcSetting.h"

int
main(int argc, char *argv[])
{
    int handle, lck;
    char buf[128], fname[MAX_PATH];
    struct stat st;

    if (argc < 3)
	exit(1);

    if (!strcmp(argv[1], "-l"))
	lck = 1;
    else if (!strcmp(argv[1], "-u"))
	lck = 0;
    else
	exit(1);

    snprintf(fname, sizeof(fname), "%s/LCK..%s", MODEM_LOCK_PATH, argv[2]);
    if (lck)
    {
	if (stat(fname, &st) == 0)
	    exit(1);

	if ((handle = open(fname, O_CREAT | O_WRONLY | O_EXCL)) == -1)
	    exit(1);

	fchmod(handle, S_IWRITE | S_IREAD | S_IRGRP | S_IROTH);

	snprintf(buf, sizeof(buf), "%010d\n", getpid());
	write(handle, buf, strlen(buf));
	close(handle);
    }
    else
    {
	if (stat(fname, &st) == 0)
	    if (unlink(fname) < 0)
		exit(1);
    }

    exit(0);
}

