NULL =

INCLUDES = \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DVINO_UIDIR=\""$(datadir)/vino"\" \
	-DVINO_ICONDIR=\""$(datadir)/pixmaps\"" \
	$(VINO_CAPPLET_CFLAGS) \
	$(VINO_KEYRING_CFLAGS) \
	$(VINO_HTTP_CFLAGS) \
	$(VINO_LIBUNIQUE_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

bin_PROGRAMS = vino-preferences

vino_preferences_SOURCES = \
	vino-preferences.c \
	gedit-spinner.h gedit-spinner.c \
	vino-message-box.h vino-message-box.c \
	sexy-url-label.h sexy-url-label.c \
	vino-url-webservice.h vino-url-webservice.c \
	$(NULL)
vino_preferences_LDADD = \
	$(VINO_CAPPLET_LIBS) \
	$(VINO_KEYRING_LIBS) \
	$(VINO_LIBUNIQUE_LIBS) \
	$(X_LIBS) \
	$(NULL)

uidir   = $(datadir)/vino
ui_DATA = vino-preferences.ui webservices

desktopdir       = $(datadir)/applications
desktop_in_files = vino-preferences.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = \
	vino-preferences.desktop \
	$(NULL)

EXTRA_DIST = \
	vino-preferences.ui \
	vino-preferences.desktop.in \
	webservices \
	$(NULL)
