/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * main.
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
#ifndef __PCMAIN_H
#define __PCMAIN_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>
#include <gtk/gtkdrawingarea.h>
#include <gtk/gtkscrolledwindow.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(String) gettext(String)
#else
#define _(String) String
#endif
#define N_(String) String

#include "pcSetting.h"	/* 기본 설정 정의 */
#include "pcCompat.h"

#define UNUSED(a) ((void) (a))

typedef struct {
    char *name;
    char *host;
    char *port;
    char *loginScript;
    char *logoutScript;
    char *remoteCharset;
} TelnetInfoType;

extern TelnetInfoType *TelnetInfo;

typedef struct {
    char *name;
    char *number;
    guint32 speed;
    char *parity;
    guint bits;
    gboolean useSWF;
    gboolean useHWF;
    gboolean directConn;
    char *loginScript;
    char *logoutScript;
    char *device;	/* modem device */
    char *initString;
    char *remoteCharset;
} ModemInfoType;

extern ModemInfoType *ModemInfo;

/* signal handler type */
typedef struct {
    pid_t pid;
    void (*func) (void);
} SignalType;

/* pcTRx.c의 protocol names와 순서가 같아야 한다 */
enum {
    TRX_PROT_ASCII,
    TRX_PROT_RAW_ASCII,
    TRX_PROT_KERMIT,
    TRX_PROT_XMODEM,
    TRX_PROT_YMODEM,
    TRX_PROT_ZMODEM
};
extern const char *TRxProtocolNames[];
extern guint32 TRxProtocol;

/* pcCtrl.c의 emulate names와 순서가 같아야 한다 */
enum { EMULATE_ANSI, EMULATE_VT100 };
extern const char *EmulateNames[];
extern guint32 EmulateMode;

extern const char *MyName; /* 실행파일 이름 */

extern GtkWidget *MainWin;
extern GtkWidget *MainBox;
extern GtkWidget *TermBox;
extern GtkWidget *CtrlBox;
extern GtkWidget *StatBox;
extern GtkItemFactory *MenuFactory;
extern GtkWidget *ScriptBox;

enum {
    PC_IDLE_STATE,
    PC_RUN_STATE,
    PC_MAX_STATE
};
extern guint PC_State;

extern gboolean UseModem;

extern int ReadFD;
extern guint32 LastOutTime;
extern gboolean DisconnectAfterTransfer;

/* configurable variables */
extern char *LeftMouseClickStr;
extern guint32 IdleGuardInterval;
extern char *IdleGuardString;
extern gboolean DoLocalEcho;
extern gboolean DoRemoteEcho;
extern gboolean DoLock;
extern gboolean ControlBarShow;
extern char *ToolBarType;
extern gboolean ToolBarShow;
extern gboolean UseHandleBox;
extern gboolean BackspaceDeleteChar;
extern gboolean UseXtermCursor;

extern gboolean UseSound;
extern gboolean UseBeep;
extern gint32 BeepPercent;
extern char *SoundPath;
extern char *SoundPlayCommand;
extern char *SoundStartFile;
extern char *SoundExitFile;
extern char *SoundConnectFile;
extern char *SoundDisconnectFile;
extern char *SoundTRxEndFile;
extern char *SoundClickFile;
extern char *SoundBeepFile;

extern gboolean AR_CheckFlag;
extern gboolean AutoResCheckEnabled;

/* 혼잣말 스크립트가 실행중인가? */
extern gboolean ScriptRunning;

/* download/upload 중인가? */
enum { TRX_IDLE, TRX_UPLOAD, TRX_DOWNLOAD };
extern int TRxRunning;
extern gfloat TRxPercent;

/* Idle gap when transfer with ASCII protocol (usec) */
extern guint32 IdleGapBetweenChar;
extern guint32 IdleGapBetweenLine;

extern guint32 MaxCaptureSize; /* maximum capture file size */

extern guint32 ModemMaxHistory, ModemHistoryBufSize;

extern char *CapturePath;
extern char *CaptureFile;
extern char *DownloadPath;
extern char *UploadPath;
extern char *ScriptPath;
extern char *ScriptAnimFile;
extern char *LogFile;

/* 꾀돌이 마우스에서 URL 선택시 실행할 명령 */
extern gboolean UseISel;
extern char *ISel_HTTP_Command;
extern char *ISel_FTP_Command;
extern char *ISel_TELNET_Command;

extern char *RX_Command;
extern char *SX_Command;
extern char *RB_Command;
extern char *SB_Command;
extern char *RZ_Command;
extern char *SZ_Command;
extern gboolean AutoRunZmodem;
extern char *RX_InfoStr;
extern char *TX_InfoStr;

extern GSList *InputFilterList;
extern GSList *ConnectFuncList;
extern GSList *DisconnectFuncList;

extern char *TelnetCommand;
extern gboolean UseZtelnet;

/* 설정 파일에서 읽어들인 telnet info 목록 */
extern GSList *TelnetInfoList;

/* 설정 파일에서 읽어들인 phone info 목록 */
extern GSList *ModemInfoList;

extern guint32 RunIconLargeSize, RunIconSmallSize;

/*************************** function prototypes ***************************/

/* debug {{{1 */
#ifdef _DEBUG_
enum {
    OPRT_TERM, OPRT_ISEL, OPRT_SCRIPT, OPRT_CHAT, OPRT_TELNET,
    OPRT_MODEM, OPRT_TRX, OPRT_GENERAL,
    MAX_OPRT_NUM
};
extern gboolean OPrintFlag[MAX_OPRT_NUM];
#endif

#define here() g_print("%s,%s,%u\n",__FILE__,__FUNCTION__,__LINE__)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# if defined(_DEBUG_)
#  define OptPrintf(o,...)	 G_STMT_START{ \
	    if ((guint) (o) < MAX_OPRT_NUM && OPrintFlag[o]) \
		g_print(__VA_ARGS__); \
	  }G_STMT_END
# else /* !_DEBUG_ */
#  define OptPrintf(o,f...)
# endif	/* !_DEBUG_ */

#elif defined(__GNUC__)
# if defined(_DEBUG_)
#  define OptPrintf(o,f...)	 G_STMT_START{ \
	    if ((guint) (o) < MAX_OPRT_NUM && OPrintFlag[o]) g_print(f); \
	  }G_STMT_END
# else /* !_DEBUG_ */
#  define OptPrintf(o,f...)
# endif	/* !_DEBUG_ */

#else /* !__GNUC__ */
extern void OptPrintf(int opt, const char *form, ...);
#endif

/* misc (main) {{{1 */
extern void FatalExit(GtkWidget *parent, const gchar *form, ...)
			    G_GNUC_PRINTF(2, 3);
extern void PC_Hangup(void);
extern void DoExit(int code);
extern void SetWindowTitle(const char *title);
extern void ControlBarFix(void);

enum { TB_IDLE_STATE, TB_CONNECT_STATE, TB_TRX_STATE };
extern void PC_ButtonControl(int state);

extern void DoConfig(void);
extern void ConfigExit(void);
extern gint ConfigVarChange(char *var, char *val, gboolean apply);
extern void ResourceCheckInstall(void);
extern void AddToolsMenus(GtkItemFactory *factory);
extern void AddBookmarkMenus(GtkItemFactory *factory,
			     void (*modemCB) (gpointer),
			     void (*telnetCB) (gpointer));

extern GtkWidget *SW_New(void);

/* control {{{1 */
extern int ControlLabelBaudCtrl(gboolean create);
extern int ControlLabelEmulCtrl(gboolean create);
extern int ControlLabelProtCtrl(gboolean create);
extern void ControlBaudrateChange(guint baud);
extern void ControlProtocolChange(void);
extern void ControlEmulateChange(void);
extern void ControlBarAddMenu(gboolean __global, const char *title,
			      const char *name, const char *type,
			      const char *action);
extern void ControlMenuExit(void);

extern int ControlLabelLineStsCtrl(gboolean __create);
extern void ControlLabelLineStsToggle(int __id);

extern void OpenLogFile(const char *name, const char *info);
extern void CloseLogFile(gint32 tick);
extern void AppendLogData(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

/* modem {{{1 */
extern int ModemConnectByName(const char *name);
extern void ModemHangup(void);
extern gboolean ModemSetSpeed(guint32 modemSpeed);
extern gboolean ModemGetConnectionStatus(void);
extern int ModemConnectionCheck(gpointer dummy);
extern int ModemWrite(const guchar *s, int len);
extern void ModemRunLoginScript(void);
extern void ModemRunLogoutScript(void);
extern guint32 ModemGetSpeed(void);
extern int ModemResetTTY(void);
extern void ModemExit(void);
extern void ModemOpenHistoryWin(void);
extern void ModemHistoryNew(void);

/* telnet {{{1 */
extern gint TelnetConnectByHostname(const char *host);
extern void TelnetDone(void);
extern void TelnetRunLoginScript(void);
extern void TelnetRunLogoutScript(void);
extern void TelnetExit(void);

/* status {{{1 */
extern void CreateStatusBar(GtkWidget *w);
extern void StatusShowMessage(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
extern GtkWidget *DialogShowMessage(GtkWidget *parent, const char *title,
				    const char *labelName, const char *msg);
extern void StatusShowTime(gint32 secs);
extern void ProgressBarControl(gboolean start);
extern GtkWidget *StatusAddFrameWithLabel(const char *str);
extern void StatusRemoveFrame(GtkWidget *frame);

/* pty {{{1 */
extern void PTY_SetWinSize(int fd, int col, int row);
extern int PTY_Open(const char *cmd, pid_t * pid, int col, int row);

/* signal {{{1 */
extern void RegisterChildSignalHandler(void);
extern SignalType *AddChildSignalHandler(pid_t pid, void (*func) (void));
extern void RemoveChildSignalHandler(SignalType *handler);
extern void ChildSignalHandlerExit(void);

/* utility {{{1 */
int pc_system(const char *fmt, ...);
extern gboolean IsFirstMB(const char *buf, int x);
extern gboolean IsSecondMB(const char *buf, int x);
extern char *ExpandPath(const char *str);
extern char *GetRealFilename(const char *filename, const char *path);
extern char *ConvertCtrlChar(const char *buf);
extern char *FindWordSeperator(const char *__s);
extern char *FindWhiteSpace(const char *s);
extern char *SkipWhiteSpace(const char *s);
extern char *FindChar(const char *s, char c);
extern char *SkipTrailWhiteSpace(const char *s);
extern int ParseLine(char *buf, int n, char *dest[]);
extern char *GetLines(FILE *fp);
extern char *FindSubString(const char *buf, int len, const char *patt,
			   int patLen);
extern void GeneralInputQuery(const char *title, const char *labelName,
			      const char *contents,
			      void (*okFunc) (char *),
			      void (*cancelFunc) (void));
extern void PC_Sleep(guint32 msec);
extern int SoundPlay(const char *file);

/* file transfer {{{1 */
extern void TRxInit(void);
extern int UploadFileQuery(void);
extern int DownloadFileQuery(void);
extern int RunRZ(void);
extern int TRxSendFile(const char *filename);
extern void TRxStop(void);
extern gint TRxProtocolSetFromString(const char *);

/* capture {{{1 */
extern void CaptureControl(void);
extern int CaptureStart(const char *filename, gboolean includeCurrBuf);
extern void CaptureFinish(void);
extern int CaptureInputFilter(const char *s, int len);
extern GtkWidget *CreateScriptFileList(GtkWidget *w, GCallback runCB);

/* script {{{1 */
extern void ScriptRunFromFile(const char *filename);
extern void ScriptRunFromString(const char *str);
extern void ScriptCancel(void);
extern char *ScriptGetFullPath(const char *filename);
extern GtkWidget *ScriptRunAnimInit(GCallback runCB);
extern void ScriptRunAnimDestroy(void);
extern void ScriptRunSizeChanged(gboolean large);
extern void ScriptExit(void);

/* auto response {{{1 */
extern void AutoResponseInit(void);
extern void AutoResponseExit(void);
extern int AutoResponseAdd(const char *token, const char *_wait,
			   const char *type, const char *act);
extern int AutoResponseCheckRun(const char *buf, int len);
/* }}} */

#endif /* __PCMAIN_H */
