NULL =

SUBDIRS = libvncserver miniupnp

INCLUDES = \
	-I$(top_srcdir)/server/libvncserver \
	-I$(top_srcdir)/server/miniupnp \
	$(VINO_SERVER_CFLAGS) \
	$(VINO_DEBUG_CFLAGS) \
	$(VINO_LIBNOTIFY_CFLAGS) \
	$(VINO_KEYRING_CFLAGS) \
	$(VINO_HTTP_CFLAGS) \
	$(VINO_NETWORKMANAGER_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(LIBGNUTLS_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(WARN_CFLAGS) \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DVINO_ICONDIR=\""$(datadir)/pixmaps\"" \
	-DVINO_CLIENTDIR=\""$(datadir)/vino"\" \
	-DVINO_GLADEDIR=\""$(datadir)/vino"\" \
	$(NULL)

libexec_PROGRAMS = vino-server

vino_server_LDADD = \
	$(VINO_SERVER_LIBS) \
	$(VINO_LIBNOTIFY_LIBS) \
	$(VINO_KEYRING_LIBS) \
	$(AVAHI_LIBS) \
	$(VINO_NETWORKMANAGER_LIBS) \
	$(LIBGNUTLS_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(X_LIBS) $(XTEST_LIBS) $(XSHM_LIBS) $(XDAMAGE_LIBS) \
	$(top_builddir)/server/libvncserver/libvncserver.la \
	$(top_builddir)/server/miniupnp/libminiupnp.la \
	$(NULL)

if HTTP_SERVER
VINO_HTTP_SRC = \
	vino-http.c \
	vino-http.h \
	$(NULL)
endif

vino_server_SOURCES = \
	vino-cursor.c \
	vino-cursor.h \
	vino-enums.c \
	vino-enums.h \
	vino-fb.c \
	vino-fb.h \
	vino-input.c \
	vino-input.h \
	vino-main.c \
	vino-marshal.c \
	vino-marshal.h \
	vino-mdns.c \
	vino-mdns.h \
	vino-prefs.c \
	vino-prefs.h \
	vino-prompt.c \
	vino-prompt.h \
	vino-server.c \
	vino-server.h \
	vino-util.c \
	vino-util.h \
	vino-status-icon.c \
	vino-status-icon.h \
	vino-dbus-listener.c \
	vino-dbus-listener.h \
	vino-background.c \
	vino-background.h \
	vino-upnp.h \
	vino-upnp.c \
	$(VINO_HTTP_SRC) \
	$(CORBA_SRCLIST) \
	$(NULL)

vino-marshal.h: vino-marshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=vino_marshal > $@

vino-marshal.c: vino-marshal.list $(GLIB_GENMARSHAL)
	echo "#include \"vino-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) $< --body --prefix=vino_marshal >> $@

vino_enum_headers = \
	$(top_srcdir)/server/vino-server.h \
	$(top_srcdir)/server/vino-prompt.h \
	$(top_srcdir)/server/vino-status-icon.h \
	$(NULL)

vino-enums.c: @REBUILD@ $(vino_enum_headers)
	glib-mkenums    --fhead "#include <glib-object.h>\n" \
			--fhead "#include \"vino-enums.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--fprod "\n#include \"@filename@\"\n" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(vino_enum_headers) > $@

vino-enums.h: @REBUILD@ $(vino_enum_headers)
	glib-mkenums    --fhead "#ifndef __VINO_ENUMS_H__\n" \
			--fhead "#define __VINO_ENUMS_H__ 1\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __VINO_ENUMS_H__ */\n" \
			--fprod "\n/* --- @filename@ --- */" \
			--eprod "#define VINO_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(vino_enum_headers) >  $@

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = vino-server.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

vino-server.desktop.in: vino-server.desktop.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

desktopdir      = $(sysconfdir)/xdg/autostart
desktop_in_files = vino-server.desktop.in
desktop_DATA    = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schemas_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(top_builddir)/server/$$p ; \
		done ; \
        fi
else
install-data-local:
endif

gladedir   = $(datadir)/vino
glade_DATA = vino-prompt.glade

EXTRA_DIST = \
	vino-prompt.glade \
	vino-server.schemas.in \
	vino-marshal.list \
	vino-server.desktop.in.in \
	$(NULL)

BUILT_SOURCES = \
	vino-enums.c \
	vino-enums.h \
	vino-marshal.c \
	vino-marshal.h \
	$(NULL)

CLEANFILES = \
	vino-server.desktop     \
	vino-server.desktop.in

DISTCLEANFILES = \
	$(schemas_DATA) \
	$(BUILT_SOURCES) \
	$(NULL)

dist-hook:
	cd $(distdir) ; rm -f $(DISTCLEANFILES)

$(vino_server_OBJECTS): $(BUILT_SOURCES)
