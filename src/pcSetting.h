/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * 기본 설정
 *
 * Copyright (C) 2000-2003, Nam SungHyun and various contributors
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
#ifndef __PCSETTING_H
#define __PCSETTING_H 1

/* default values.
 * CAUTION: string인 경우에는 절대로 NULL 을 쓰지 말것.
 *	    DEFAULT 값은 항상 valid한 어떤 값을 갖는다고 가정.
 */
#define DEFAULT_MODEM_DEVICE  "/dev/modem"
#define DEFAULT_MODEM_SPEED   9600UL
#define DEFAULT_MODEM_PARITY  "No"	/* "EVEN" or "ODD" */
#define DEFAULT_MODEM_BITS    8U
#define DEFAULT_MODEM_INIT    "atz\r"
#define DEFAULT_CONNECT_CHK_INTERVAL  300

#define DEFAULT_FONT "monospace 11"
#define DEFAULT_COLUMN 80
#define DEFAULT_COLUMN_STR "80"
#define DEFAULT_ROW 24
#define DEFAULT_ROW_STR "24"
#define DEFAULT_SAVED_LINES_STR "512"
#define DEFAULT_WORD_SEPERATOR ";,-/\\()[]{}*+<>|!@#$%&:'\".?`~^"
#define MAX_SAVED_LINES 65535
#define DEFAULT_MAX_CAPTURE_SIZE_STR "10000000"

#define DEFAULT_REMOTE_CHARSET "EUC-KR"

/* path */
#ifndef MAX_PATH
# ifdef FILENAME_MAX
#  define MAX_PATH	      (FILENAME_MAX + 1)
# else
#  define MAX_PATH	      4096
# endif
#endif
#define PC_RC_BASE	      "~/."PACKAGE
#define MODEM_LOCK_PATH	      "/var/lock"
#define PC_GTK_RC_FILE	      PC_RC_BASE"/gtkrc"
#define PC_ACCEL_RC_FILE      PC_RC_BASE"/accelrc"
#define PC_RC_FILE	      PC_RC_BASE"/gtkcommrc"
#define PC_BOOKMARK_FILE      PC_RC_BASE"/bookmarks"
#define PC_ISEL_RC_FILE	      PC_RC_BASE"/iselrc"
#define PC_CHAT_RC_FILE	      PC_RC_BASE"/chatrc"
#define DEFAULT_CAPTURE_PATH  PC_RC_BASE"/capture"
#define DEFAULT_CAPTURE_FILE  "?"	/* ? = 사용자에게 물어봄 */
#define DEFAULT_DOWNLOAD_PATH PC_RC_BASE"/down"
#define DEFAULT_UPLOAD_PATH   PC_RC_BASE"/up"
#define DEFAULT_SCRIPT_PATH   PC_RC_BASE"/script"
#define DEFAULT_SOUND_PATH    PC_RC_BASE"/sound"
#define DEFAULT_PIXMAP_PATH   PC_RC_BASE"/pixmap"

#define DEFAULT_SCRIPT_EXT	    ".scr"
#define DEFAULT_STARTUP_SCRIPT	    "startup"DEFAULT_SCRIPT_EXT
#define DEFAULT_SCRIPT_ANIM_FILE    "tux-anim.png"

#define DEFAULT_TELNET_COMMAND	    "telnet -8 -e ''"
#define DEFAULT_SOUND_PLAY_COMMAND  "wavplay"

#define DEFAULT_ISEL_HTTP_COMMAND   "netscape"
#define DEFAULT_ISEL_FTP_COMMAND    "netscape"
#define DEFAULT_ISEL_TELNET_COMMAND PACKAGE" --telnet"

#define DEFAULT_SX_COMMAND	    "sz --xmodem -b -B 128 -vv"
#define DEFAULT_RX_COMMAND	    "rz --xmodem -b -B 128 -vv"
#define DEFAULT_SB_COMMAND	    "sz --ymodem -b -vv"
#define DEFAULT_RB_COMMAND	    "rz --ymodem -b -vv"
#define DEFAULT_SZ_COMMAND	    "sz -O -b -r -vv"
#define DEFAULT_RZ_COMMAND	    "rz -O -b -r -vv"

#define DEFAULT_IDLE_GUARD_STRING   " \b"

/* NONE, ICONS, TEXT, BOTH (icons and text) */
#define DEFAULT_TOOLBAR_TYPE	    "BOTH"

#endif /* __PCSETTING_H */
