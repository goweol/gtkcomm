
rc_all = \
	bookmarks			\
	gtkcommrc			\
	gtkrc				\
	iselrc				\
	chatrc				\
	capture/DEFAULT_CAPTURE_DIR	\
	down/DEFAULT_DOWNLOAD_DIR	\
	pixmap/tb_capture.xpm		\
	pixmap/tb_close.xpm		\
	pixmap/tb_down.xpm		\
	pixmap/tb_exit.xpm		\
	pixmap/tb_open.xpm		\
	pixmap/tb_save.xpm		\
	pixmap/tb_up.xpm		\
	pixmap/tux-anim.png		\
	script/SCRIPT.txt		\
	script/chol-login.scr		\
	script/chol-logout.scr		\
	script/com-login.scr		\
	script/dialog.scr		\
	script/hitel-login.scr		\
	script/kldp-login.scr		\
	script/kldp-logout.scr		\
	script/local-login.scr		\
	script/local-logout.scr		\
	script/monitor.scr		\
	script/startup.scr		\
	script/now-login.scr		\
	script/download.scr		\
	script/down.ascii		\
	script/down.xdm			\
	script/flash.scr		\
	up/DEFAULT_UPLOAD_DIR

EXTRA_DIST = $(rc_all)			\
	gtkrc.in			\
	script/startup.scr.in

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir) $(DESTDIR)$(pkgdatadir)/capture \
		$(DESTDIR)$(pkgdatadir)/down $(DESTDIR)$(pkgdatadir)/pixmap \
		$(DESTDIR)$(pkgdatadir)/script $(DESTDIR)$(pkgdatadir)/up
	for f in $(rc_all); do \
	   $(INSTALL_DATA) $$f $(DESTDIR)$(pkgdatadir)/$$f; \
	done

