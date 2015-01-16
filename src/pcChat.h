/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Chat
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
#ifndef __PCCHAT_H
#define __PCCHAT_H 1

typedef struct {
    GtkWidget *w;
} ChatType;

typedef struct {
    GtkWidget *w;
    ChatType *chat;
} ScrolledChatType;

extern ScrolledChatType *SChat;
extern char *ChatPrefix;
extern char *ChatSuffix;

extern void MenuChat(void);
extern void ChatExit(void);

#endif /* __PCCHAT_H */
