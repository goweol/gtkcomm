#
# Makefile.am
#

localedir = $(datadir)/locale
INCLUDES = \
	$(GTK_CFLAGS) $(GTHREAD_CFLAGS)			\
	-DLOCALE_DIR=\"$(localedir)\"			\
	-DG_DISABLE_DEPRECATED				\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED
#	-DGTK_DISABLE_DEPRECATED


bin_PROGRAMS = gtkcomm gtkcomm_lock
gtkcomm_LDADD = $(GTK_LIBS) $(GTHREAD_LIBS) $(GDK_IMLIB_LIBS) \
		$(UTIL_LIBS) $(GLIB_LIBS) -lX11

gtkcomm_SOURCES = \
	pcAutoRes.c	\
	pcChat.c	\
	pcCompat.c	\
	pcConfig.c	\
	pcCtrl.c	\
	pcISel.c	\
	pcMain.c	\
	pcModem.c	\
	pcPTY.c		\
	pcScript.c	\
	pcSignal.c	\
	pcStatus.c	\
	pcTRx.c		\
	pcTelnet.c	\
	pcTerm.c	\
	pcUtil.c	\
	pcChat.h	\
	pcCompat.h	\
	pcMain.h	\
	pcSetting.h	\
	pcTerm.h	\
	gtkutil.c	\
	gtkutil.h	\
	mbyte.c		\
	mbyte.h

gtkcomm_lock_LDADD =
gtkcomm_lock_SOURCES = gtkcomm_lock.c

EXTRA_DIST = test/Makefile test/test.c test/gtkscript.c \
	     pixmaps/README pixmaps/red.xpm pixmaps/green.xpm \
	     pixmaps/yellow.xpm

install-exec-hook:
	-chgrp lock $(DESTDIR)$(bindir)/gtkcomm_lock
	-chmod 2755 $(DESTDIR)$(bindir)/gtkcomm_lock

# For vi[m] users
ctags:
	ctags -f TAGS ../config.h *.[ch]

