/*
 * gtkutil library header
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
#ifndef __GTKUTIL_H
#define __GTKUTIL_H 1

#include <gtk/gtk.h>

typedef struct _GUDraw {
    PangoContext *context;
    PangoAttrList *attrList;
    PangoFontDescription *fontDesc;
    int fontWidth;
    int fontHeight;
    int fontAscent;
    int fontDescent;
} GUDraw;

extern GUDraw *gu_draw_new(GtkWidget *w, const char *fontname);
extern void gu_draw_free(GUDraw *d);
extern int gu_draw_string(GUDraw *d, GdkWindow *win, GdkGC *gc, int x, int y,
			   const guchar *str, int len);

extern int is_cjk_font(PangoContext *context, PangoFontDescription *font_desc);

#endif /* __GTKUTIL_H */
