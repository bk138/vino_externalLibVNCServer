NULL =

INCLUDES = \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DVINO_GLADEDIR=\""$(datadir)/gnome/vino"\" \
	-DVINO_ICONDIR=\""$(datadir)/pixmaps\"" \
	$(VINO_CAPPLET_CFLAGS) \
	$(VINO_KEYRING_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

bin_PROGRAMS = vino-preferences

vino_preferences_SOURCES = \
	vino-preferences.c \
	vino-url.c \
	vino-url.h \
	$(NULL)
vino_preferences_LDADD = \
	$(VINO_CAPPLET_LIBS) \
	$(VINO_KEYRING_LIBS) \
	$(X_LIBS) \
	$(NULL)

gladedir   = $(datadir)/gnome/vino
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
