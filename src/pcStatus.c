/* vim:tabstop=8:softtabstop=4:shiftwidth=4:noexpandtab:
 *
 * Status bar
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
#include "config.h"

#include "pcMain.h"
#include "pcTerm.h"	/* Term */

static GtkWidget *StatusLabel = NULL;
static GtkWidget *TimeLabel = NULL;

/* ProgressBarUpdate() {{{1 */
static gint
ProgressBarUpdate(gpointer data)
{
    static guint oldVal = 0;
    guint a = (guint) (TRxPercent * 100.0);

    if (a != oldVal)
    {
	gdouble f = (gdouble) TRxPercent;
	char buf[5];

	if (f > 1.0)
	    f = 1.0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(data), f);
	g_snprintf(buf, sizeof(buf), "%d%%", a);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(data), buf);
	oldVal = a;
    }

    return TRUE;
}

/* ProgressBarControl() {{{1 */
void
ProgressBarControl(gboolean start)
{
    static GtkWidget *align, *pbar = NULL;
    static guint ProgressTag;

    if (start)
    {
	if (pbar == NULL)
	{
	    TRxPercent = 0.00;

	    /* progress bar */
	    align = gtk_alignment_new(0.5, 0.5, 0, 0);
	    gtk_box_pack_end(GTK_BOX(StatBox), align, FALSE, FALSE, 5);
	    gtk_container_set_border_width(GTK_CONTAINER(align), 0);
	    pbar = gtk_progress_bar_new();
	    gtk_widget_set_size_request(pbar, 100, Term->fontHeight + 6);
	    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pbar),
					     GTK_PROGRESS_LEFT_TO_RIGHT);
	    gtk_container_add(GTK_CONTAINER(align), pbar);
	    gtk_widget_show(pbar);
	    gtk_widget_show(align);

	    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), TRxPercent);

	    ProgressTag = gtk_idle_add(ProgressBarUpdate, pbar);
	}
    }
    else
    {
	if (pbar)
	{
	    gtk_idle_remove(ProgressTag);
	    gtk_widget_destroy(pbar);
	    gtk_widget_destroy(align);
	    pbar = NULL;
	}
    }
}

/* StatusAddFrameWithLabel() {{{1 */
GtkWidget *
StatusAddFrameWithLabel(const char *str)
{
    GtkWidget *frame, *label;

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
    gtk_box_pack_end(GTK_BOX(StatBox), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    label = gtk_label_new(str);
    gtk_container_add(GTK_CONTAINER(frame), label);
    gtk_widget_show(label);

    return frame;
}

/* StatusRemoveFrame() {{{1 */
void
StatusRemoveFrame(GtkWidget *frame)
{
    gtk_widget_destroy(frame);
}

/* CreateStatusBar() {{{1 */
void
CreateStatusBar(GtkWidget *w)
{
    GtkWidget *frame;

    StatusLabel = gtk_label_new(_("Welcome!!!"));
    gtk_box_pack_start(GTK_BOX(w), StatusLabel, FALSE, FALSE, 5);
    gtk_misc_set_alignment(GTK_MISC(StatusLabel), 0.0, 0.5);
    gtk_widget_show(StatusLabel);

    /* 시간 레이블 */
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 0);
    gtk_box_pack_end(GTK_BOX(w), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    TimeLabel = gtk_label_new(" 00:00:00 ");
    gtk_container_add(GTK_CONTAINER(frame), TimeLabel);
    gtk_widget_show(TimeLabel);
}

/* StatusShowMessage() {{{1 */
void
StatusShowMessage(const gchar *format, ...)
{
    int i, j, len;
    va_list args;
    gchar *buf, *p;

    va_start(args, format);
    buf = g_strdup_vprintf(format, args);
    va_end(args);

    /* FIXME: status area의 길이를 얻어와서 처리 */
    if ((len = strlen(buf)) > 60)
    {
	buf[60] = '\0';
	/* avoid multibyte corruption */
	for (i = 59; i > 0; --i)
	{
	    if (isspace(buf[i]))
	    {
		for (j = 0; i < len && j < 3; ++j)
		    buf[i++] = '.';
		buf[i] = '\0';
		break;
	    }
	}
    }
    /* change '\n' to space */
    for (p = buf; (p = strchr(p, '\n')) != NULL; p++)
	*p = ' ';

    gtk_label_set_text(GTK_LABEL(StatusLabel), buf);
    g_free(buf);
}

/* StatusShowTime() {{{1 */
void
StatusShowTime(gint32 secs)
{
    gchar buf[20];
    gint hour;

    if (GTK_WIDGET_HAS_FOCUS(Term))
    {
	hour = (gint) secs / 3600;
	secs %= 3600UL;
	g_snprintf(buf, sizeof(buf), " %02d:%02d:%02d ", hour,
		   (gint) (secs / 60), (gint) (secs % 60));
	gtk_label_set_text(GTK_LABEL(TimeLabel), buf);
	gtk_widget_set_sensitive(GTK_WIDGET(TimeLabel), TRUE);
    }
    else
	gtk_widget_set_sensitive(GTK_WIDGET(TimeLabel), FALSE);
}

/* DialogShowMessage() {{{1 */
GtkWidget *
DialogShowMessage(GtkWidget *parent, const char *title,
		  const char *labelName, const char *msg)
{
    GtkWidget *w;
    GtkWidget *vbox, *label, *sep, *button;

    g_return_val_if_fail(msg != NULL, NULL);

    w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (title)
	gtk_window_set_title(GTK_WINDOW(w), title);
    else
	gtk_window_set_title(GTK_WINDOW(w), PACKAGE " - " VERSION);
    if (parent)
	gtk_window_set_transient_for(GTK_WINDOW(w), GTK_WINDOW(parent));
    gtk_container_set_border_width(GTK_CONTAINER(w), 4);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_MOUSE);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(w), vbox);

    label = gtk_label_new(msg);
    gtk_container_add(GTK_CONTAINER(vbox), label);

    sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 1);

    button = gtk_button_new_with_label(labelName);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 1);
    g_signal_connect_object(G_OBJECT(button), "clicked",
			    G_CALLBACK(gtk_widget_destroy),
			    G_OBJECT(w), G_CONNECT_SWAPPED);

    gtk_widget_show_all(w);
    return w;
}
