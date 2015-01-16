/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Auto response
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

#include "pcMain.h"
#include "pcTerm.h"

/* Global variables/functions {{{1 */

/* 토글 menu에 의해 toggle된다. 이 flag가 FALSE이면 auto-response 기능이
 * 꺼진다. flag를 사용하지 않고 handler를 add/remove하는 방법도 있을 수 있는
 * 데, 지금은 너무 골치아퍼서 간단하게 flag를 사용하도록 했다.
 *   여기에도 약간의 꽁수가 있는 데, toggle menu가 처음에 꺼져서 만들어 지기
 * 때문에 여기에서 false로 해 놓고 menu를 active로 바꾸면서 자동으로 TRUE가
 * 되도록 하였다. 저 이렇게 살고 있어용! 제발 좀 고쳐줘용!
 */
gboolean AutoResCheckEnabled = FALSE;

/* auto-response check routine에 연속적으로 불려지면서 하나의 data input에
 * 대해 두번이상 반응하는 것을 방지하기 위한 flag. auto-response check
 * routine에 의해 FALSE가 되고 key 입력이나 data input이 발생한 경우에
 * 자동으로 TRUE된다.
 */
gboolean AR_CheckFlag = TRUE;

/* AutoResponseType {{{1 */
/* auto response type.
 * NOTE: AR_STRING, AR_SCRIPT, AR_COMPLEX_STRING, AR_COMPLEX_SCRIPT는
 *	 4개중 하나만 설정된다.
 */
#define AR_STRING	   0x01
#define AR_SCRIPT	   0x02
#define AR_SCRIPTFILE	   0x04
#define AR_COMPLEX_STRING  0x08	/* not supported */
#define AR_COMPLEX_SCRIPT  0x10	/* not supported */
#define AR_GLOBAL	   0x80

typedef struct {
    gint type;
    gchar *pattern;
    gint patternLen;
    gchar *action;
    gint actionLen;
} AutoResponseType;

/* Local variables {{{1 */

static GSList *AR_List = NULL;
static gboolean AR_Removed = TRUE;

/* AutoResponseHandlerControl() {{{1 */
static void
AutoResponseHandlerControl(gboolean add)
{
    static guint handlerId = 0;

    if (Term)
    {
	if (add)
	{
	    if (AR_List && !handlerId)
		handlerId = g_signal_connect(G_OBJECT(Term),
					     "line_changed",
					     G_CALLBACK(TermLineChanged),
					     NULL);
	    else
		g_warning("%s: AR_List = %p, handlerId = %u\n", __FUNCTION__,
			  AR_List, handlerId);
	}
	else
	{
	    if (handlerId > 0)
	    {
		g_signal_handler_disconnect(G_OBJECT(Term), handlerId);
		handlerId = 0;
	    }
	    else
		g_warning("%s: handlerId = %u\n", __FUNCTION__, handlerId);
	}
    }
}

/* AutoResponseCheckRun() {{{1 */
int
AutoResponseCheckRun(const char *buf, int len)
{
    char *file;
    GSList *slist;
    AutoResponseType *ar;

    if (AutoResCheckEnabled)
    {
	for (slist = AR_List; slist; slist = slist->next)
	{
	    ar = slist->data;

	    /* pattern은 버퍼크기보다는 무조건 작다고 가정한다. */
	    if (len < (int) ar->patternLen)
		continue;

	    if (FindSubString(buf, len, ar->pattern, ar->patternLen))
	    {
		if (ar->type & AR_STRING)
		    Term->remoteWrite((guchar*) ar->action, ar->actionLen);
		else if (ar->type & AR_SCRIPT)
		    ScriptRunFromString(ar->action);
		else if (ar->type & AR_SCRIPTFILE)
		{
		    file = GetRealFilename(ar->action, ScriptPath);
		    if (file != NULL)
		    {
			g_free(ar->action);
			ar->action = file;
			ScriptRunFromFile(ar->action);
		    }
		}
		else
		    FatalExit(NULL, "Unknown auto response type = %d",
			      ar->type);
		break;
	    }
	}
    }

    return FALSE;
}

/* AutoResponseRemove() {{{1 */
static void
AutoResponseRemove(void)
{
    GSList *slist;
    AutoResponseType *ar;
    gboolean globalFound;

    if (AR_List && !AR_Removed)
    {
	AR_Removed = TRUE;
	DisconnectFuncList = g_slist_remove(DisconnectFuncList,
					    AutoResponseRemove);

	globalFound = FALSE;
	for (slist = AR_List; slist;)
	{
	    ar = slist->data;

	    /* list remove가 일어나기 전에 다음 list를 얻어오는 것이 안전하다. */
	    slist = slist->next;

	    if (ar->type & AR_GLOBAL)
		globalFound = TRUE;
	    else
	    {
		g_free(ar->pattern);
		g_free(ar->action);
		g_free(ar);
		AR_List = g_slist_remove(AR_List, ar);
	    }
	}

	if (!globalFound)
	{
	    g_slist_free(AR_List);
	    AR_List = NULL;
	    AutoResponseHandlerControl(FALSE);
	}
    }
}

/* AutoResponseAdd() {{{1 */
int
AutoResponseAdd(const char *token, const char *wait, const char *type,
		const char *action)
{
    register int arType;
    AutoResponseType *ar;

    arType = 0;

    if (!g_ascii_strcasecmp(token, "gadd"))
	arType |= AR_GLOBAL;
    else if (g_ascii_strcasecmp(token, "add"))
    {
	g_warning(_("%s: Unknown token '%s'."), __FUNCTION__, token);
	return -1;
    }

    if (!g_ascii_strcasecmp(type, "string"))
	arType |= AR_STRING;
    else if (!g_ascii_strcasecmp(type, "script"))
	arType |= AR_SCRIPT;
    else if (!g_ascii_strcasecmp(type, "sfile"))
	arType |= AR_SCRIPTFILE;
    else
    {
	g_warning(_("%s: Unknown type '%s'"), __FUNCTION__, type);
	return -1;
    }

    ar = g_new(AutoResponseType, 1);
    ar->type = arType;
    ar->pattern = ConvertCtrlChar(wait);
    ar->patternLen = strlen(ar->pattern);
    ar->action = ConvertCtrlChar(action);
    ar->actionLen = strlen(ar->action);

    AR_List = g_slist_append(AR_List, ar);

    if (AR_Removed)
    {
	DisconnectFuncList = g_slist_append(DisconnectFuncList,
					    AutoResponseRemove);
	AR_Removed = FALSE;
	AutoResponseHandlerControl(TRUE);
    }

    return 0;
}

/* AutoResponseInit() {{{1 */
void
AutoResponseInit(void)
{
    AR_CheckFlag = TRUE;
    AR_List = NULL;
    AR_Removed = TRUE;
}

/* AutoResponseExit() {{{1 */
void
AutoResponseExit(void)
{
    if (AR_List)
    {
	g_slist_free(AR_List);
	AR_List = NULL;
    }
}

