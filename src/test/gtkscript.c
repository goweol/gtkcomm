/** =========================================================================
   GTK로 만든 script sample.

   TODO: python이나 tcl/tk같이 compile이 필요없는 language로 conversion.

   Copyright (C) 2000 Sung-Hyun Nam, Chi-Deok Hwang and various contributors

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
========================================================================== */
#include <stdio.h>
#include <gtk/gtk.h>

static void start_script_command(void)
{
    fprintf(stderr, "/set script ");
}

static void send_script_command(char *cmd)
{
    char *s;

    s = g_strconcat("/set script ", cmd, "; ", NULL);
    fprintf(stderr, s);
    g_free(s);
}

static void transmit(char *s)
{
    printf(s);
}

static void waitfor(char *s, int secs)
{
    char buf[256];
    int size;

    secs = 0;	/* TODO */

    for (;;)
    {
	if ((size = read(0, buf, sizeof(buf) - 1)) > 0)
	{
	    buf[size] = '\0';
	    if (strstr(buf, s))
		return;
	}
    }
}

static void file_upload(GtkWidget *w, char *filename)
{
    char *cmd;

#if 1
    /* start_script_command(); */
    transmit("\n");
    waitfor("$", 0);
    cmd = g_strconcat("sendfile ascii ", filename, NULL);
    send_script_command(cmd);
    g_free(cmd);
    send_script_command("waitfor \"$\"");
    send_script_command("pause 2");
    send_script_command("transmit \"ls\n\"");
    send_script_command("waitfor \"$\"");
    send_script_command("transmit \"dir\n\"");
#else

    cmd = g_strconcat("/set script transmit \"dir\n\"; waitfor \"$\"; "
		      "sendfile ascii ", filename,
		      "; transmit \"ls\n\"; waitfor \"$\"; transmit \"dir\n\"",
		      NULL);
    send_script_command(cmd);
    g_free(cmd);

#endif
    gtk_main_quit();
    w = NULL;
}

static void entry_upload(GtkWidget *w, GtkWidget *entry)
{
    gchar *s;

    s = gtk_entry_get_text(GTK_ENTRY(entry));
    if (!*s)
	return;

    file_upload(w, s);
}

static void do_job(void)
{
    GtkWidget *vbox, *hbox, *button;
    GtkWidget *win, *entry;

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(win, "main");
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_MOUSE);
    gtk_window_set_title(GTK_WINDOW(win), "GTK upload sample script");
    gtk_window_set_policy(GTK_WINDOW(win), TRUE, TRUE, FALSE);
    gtk_signal_connect(GTK_OBJECT(win), "destroy", gtk_main_quit, NULL);

    vbox = gtk_vbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show(vbox);

    hbox = gtk_hbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);
    gtk_widget_show(hbox);

    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(entry), "activate",
		       (GtkSignalFunc) entry_upload, entry);
    gtk_widget_show(entry);

    button = gtk_button_new_with_label("Upload");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc) entry_upload, entry);
    gtk_widget_show(button);

    hbox = gtk_hbox_new(FALSE, 3);
    gtk_container_add(GTK_CONTAINER(vbox), hbox);
    gtk_widget_show(hbox);

    button = gtk_button_new_with_label("test");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc) file_upload, "test.gz");
    gtk_widget_show(button);

    button = gtk_button_new_with_label("exam");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc) file_upload, "exam.gz");
    gtk_widget_show(button);

    button = gtk_button_new_with_label("sample");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
		       (GtkSignalFunc) file_upload, "sample.lod");
    gtk_widget_show(button);

    button = gtk_button_new_with_label("QUIT");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			      (GtkSignalFunc) gtk_main_quit, NULL);
    gtk_widget_show(button);

    gtk_widget_show(win);
}

int main(int argc, char *argv[])
{
    gtk_set_locale();
    gtk_init(&argc, &argv);

    do_job();

    gtk_main();
    return 0;
}
