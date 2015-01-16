/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * SChat
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
#include "pcChat.h"

/* Definitions {{{1 */

/* 현재는 entry로 1줄짜리 만듦 */
#define CHAT_WIN_SIZE	1

/* Global variables/functions {{{1 */

ScrolledChatType *SChat = NULL;
char *ChatPrefix;
char *ChatSuffix;

/* ChatKeyMapType {{{1 */
typedef struct {
    guint key;
    guint state;
    char *str;
    int len;
} ChatKeyMapType;

/* Local variables {{{1 */

static GSList *ChatKeyMapList = NULL;

/* ChatAddKeyMap() {{{1 */
static void
ChatAddKeyMap(char *modStr, char *keyStr, char *str)
{
    ChatKeyMapType *keymap;
    char *p;
    int key, state;

    state = 0;
    do
    {
	if ((p = strchr(modStr, '_')) != NULL)
	    *p = '\0';

	if (!g_ascii_strcasecmp(modStr, "CTRL"))
	    state |= GDK_CONTROL_MASK;
	else if (!g_ascii_strcasecmp(modStr, "SHIFT"))
	    state |= GDK_SHIFT_MASK;
	else if (!g_ascii_strcasecmp(modStr, "ALT"))
	    state |= GDK_MOD1_MASK;
	else
	{
	    state = 0;
	    break;
	}
	modStr = p + 1;
    }
    while (p);

    if (sscanf(keyStr, "%x", &key) != 1)
    {
	g_warning(_("Invalid key number in %s = %s"), PC_CHAT_RC_FILE, keyStr);
	return;
    }

    keymap = g_new(ChatKeyMapType, 1);
    keymap->key = key;
    keymap->state = state;
    keymap->str = ConvertCtrlChar(str);
    keymap->len = strlen(keymap->str);
    ChatKeyMapList = g_slist_append(ChatKeyMapList, keymap);
}

/* ChatInitKeyMap() {{{1 */
static void
ChatInitKeyMap(void)
{
    char *filename;
    FILE *fp;
    int n;
#define MAX_CHAT_PARAMS	3
    char *buf, *param[MAX_CHAT_PARAMS];

    if ((filename = GetRealFilename(PC_CHAT_RC_FILE, NULL)) != NULL)
    {
	if ((fp = fopen(filename, "r")) != NULL)
	{
	    while ((buf = GetLines(fp)) != NULL)
	    {
		if ((n = ParseLine(buf, MAX_CHAT_PARAMS, param)) !=
		    MAX_CHAT_PARAMS)
		    continue;
		ChatAddKeyMap(param[0], param[1], param[2]);
	    }
	    fclose(fp);
	}
	g_free((gpointer) filename);
    }
}

/* ChatWindowNew() {{{1 */
static ScrolledChatType *
ChatWindowNew(int col, int row, int visibleLine, GCallback func)
{
    ScrolledChatType *schat;
    ChatType *chat;

    schat = g_new(ScrolledChatType, 1);
    schat->w = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(schat->w), 0);

    chat = g_new(ChatType, 1);
    chat->w = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(chat->w), "");
    gtk_container_add(GTK_CONTAINER(schat->w), chat->w);
    gtk_widget_set_name(chat->w, "Chat");
    gtk_widget_show(chat->w);

    g_signal_connect(G_OBJECT(chat->w), "activate", G_CALLBACK(func), chat);

    schat->chat = chat;

    return schat;

    col = row = visibleLine = 0;
}

/* ChatFlushCB() {{{1 */
static void
ChatFlushCB(GtkWidget *w, ChatType *chat)
{
    char *s;
    const char *p;

    UNUSED(w);

    p = gtk_entry_get_text(GTK_ENTRY(chat->w));
    if (!*p)
	return;

    s = g_strdup(p);
    if (!g_ascii_strncasecmp(s, "/set ", 5))
    {
	char *param[4];

	if (!g_ascii_strncasecmp(s + 5, "config ", 7))
	{
	    /* 사용자는 다음과 같이 해서 prefix나 suffix를 바꿀 수 있다.
	     * 물론 모든 configurable한 값들은 같은 방법으로 바꿀 수 있다.
	     *    /set config chatprefix "nsh: "
	     *    /set config chatsuffix ""
	     *    /set config enterconvert 2
	     *    /set config isel_http_command "mozilla"
	     * NOTE: 여기에서 '/set config ToolBarType ICONS' 한다고 tool-bar
	     * type이 바뀌지는 않는다. 이 프로그램이 startup시 참조하는
	     * variable들은 프로그램이 도는 데, 아무 영향도 끼치지 않음. 어떤
	     * variable들은 잘못 바꿀 경우 프로그램이 오동작할 수도 있다.
	     * 그런것에 대한 대비가 되어있지 않기 때문이다.
	     */
	    if (ParseLine(s + 12, 2, param) == 2)
		ConfigVarChange(param[0], param[1], TRUE);
	}
	else if (!g_ascii_strncasecmp(s + 5, "script ", 7))
	    ScriptRunFromString(s + 12);
	else if (!g_ascii_strncasecmp(s + 5, "title ", 6))
	    SetWindowTitle(s + 11);
	else if (!g_ascii_strncasecmp(s + 5, "protocol ", 9))
	    TRxProtocolSetFromString(s + 14);
#ifdef _DEBUG_
	else if (!g_ascii_strncasecmp(s + 5, "debug ", 6))
	{
	    unsigned u;

	    if (ParseLine(s + 1, 3, param) == 3)
	    {
		if (sscanf(param[2], "%u", &u) == 1)
		{
		    if (u < MAX_OPRT_NUM)
		    {
			OPrintFlag[u] = !OPrintFlag[u];
			StatusShowMessage(_("Debug option %u = %s"), u,
					  OPrintFlag[u] ? "ON" : "OFF");
		    }
		}
	    }
	}
#endif
    }
    else
    {
	if (ChatPrefix)
	    Term->remoteWrite((guchar*) ChatPrefix, -1);
	Term->remoteWrite((guchar*) s, -1);
	if (ChatSuffix)
	    Term->remoteWrite((guchar*) ChatSuffix, -1);
	if (EnterConvert == ENTER_CR)
	    Term->remoteWrite((guchar*) "\n", 1);
	else if (EnterConvert == ENTER_CRLF)
	    Term->remoteWrite((guchar*) "\r\n", 2);
	else	/* LF or NONE */
	    Term->remoteWrite((guchar*) "\r", 1);
    }

    gtk_entry_set_text(GTK_ENTRY(chat->w), "");
    g_free(s);
}

/* ChatKeyPressCB() {{{1 */
static gint
ChatKeyPressCB(GtkWidget *w, GdkEventKey *ev, ChatType *chat)
{
    const char *s;
    GSList *slist;
    ChatKeyMapType *keymap;

    s = gtk_entry_get_text(GTK_ENTRY(chat->w));
    if (ev->keyval == 'z' && (ev->state & GDK_CONTROL_MASK) && !*s)
    {
	/* line이 비어있고, control-z */
	Term->remoteWrite((guchar*) "\032", 1);	/* send ^Z */
	return TRUE;
    }
    else
    {
	for (slist = ChatKeyMapList; slist; slist = slist->next)
	{
	    keymap = slist->data;
	    if (ev->keyval == keymap->key && ev->state == keymap->state)
	    {
		Term->remoteWrite((guchar*) keymap->str, keymap->len);
		return TRUE;
	    }
	}
    }

    /* check if it is menu hotkey */
    if (gtk_accel_groups_activate(G_OBJECT(w), ev->keyval, ev->state))
	return TRUE;

    return FALSE;
}

/* MenuChat() {{{1 */
void
MenuChat(void)
{
    if (SChat)
    {
	gtk_widget_destroy(SChat->w);
	gtk_widget_grab_focus(GTK_WIDGET(Term));

	ChatExit();
    }
    else
    {
	SChat = ChatWindowNew(Term->col, Term->row, CHAT_WIN_SIZE,
			      G_CALLBACK(ChatFlushCB));

	if (SChat)
	{
	    gtk_box_pack_end(GTK_BOX(TermBox), SChat->w, FALSE, FALSE, 0);
	    gtk_widget_show(SChat->w);
	    gtk_widget_grab_focus(GTK_WIDGET(SChat->chat->w));
	    g_signal_connect(G_OBJECT(SChat->chat->w), "key_press_event",
			     G_CALLBACK(ChatKeyPressCB), SChat->chat);

	    ChatInitKeyMap();
	}
	else
	    StatusShowMessage(_("Cannot open chatting window."));
    }
}

/* ChatExit() {{{1 */
void
ChatExit(void)
{
    GSList *slist;
    ChatKeyMapType *keymap;

    if (SChat)
    {
	g_free(SChat);
	SChat = NULL;
    }
    if (ChatKeyMapList)
    {
	for (slist = ChatKeyMapList; slist; slist = slist->next)
	{
	    keymap = slist->data;
	    g_free(keymap->str);
	    g_free(keymap);
	}
	g_slist_free(ChatKeyMapList);
	ChatKeyMapList = NULL;
    }
}

