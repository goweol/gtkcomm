#
# Makefile.am
#

localedir = $(datadir)/locale
AM_CPPFLAGS = \
	$(GC_CFLAGS)					\
	-DLOCALE_DIR=\"$(localedir)\"			\
	-DG_DISABLE_DEPRECATED
#	-DGDK_PIXBUF_DISABLE_DEPRECATED
#	-DGDK_DISABLE_DEPRECATED
#	-DGTK_DISABLE_DEPRECATED


bin_PROGRAMS = gtkcomm
gtkcomm_LDADD = $(GC_LIBS) $(UTIL_LIBS) -lX11

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

EXTRA_DIST = test/Makefile test/test.c test/gtkscript.c \
	     pixmaps/README pixmaps/red.xpm pixmaps/green.xpm \
	     pixmaps/yellow.xpm

# For vi[m] users
ctags:
	ctags -f TAGS ../config.h *.[ch]

