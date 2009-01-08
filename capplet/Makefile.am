NULL =

INCLUDES = \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DVINO_GLADEDIR=\""$(datadir)/vino"\" \
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
	$(NULL)
vino_preferences_LDADD = \
	$(VINO_CAPPLET_LIBS) \
	$(VINO_KEYRING_LIBS) \
	$(VINO_LIBUNIQUE_LIBS) \
	$(X_LIBS) \
	$(NULL)

if !HAVE_GETIFADDRS
INCLUDES += -I../server/libvncserver
vino_preferences_LDADD += ../server/libvncserver/libifaddrs.la
endif

gladedir   = $(datadir)/vino
glade_DATA = vino-preferences.glade

desktopdir       = $(datadir)/applications
desktop_in_files = vino-preferences.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

CLEANFILES = \
	vino-preferences.desktop \
	$(NULL)

EXTRA_DIST = \
	vino-preferences.glade \
	vino-preferences.desktop.in \
	$(NULL)
