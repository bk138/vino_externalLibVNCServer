NULL =

SUBDIRS = libvncserver

INCLUDES = \
	-I$(top_srcdir)/server/libvncserver \
	$(VINO_SERVER_CFLAGS) \
	$(VINO_DEBUG_CFLAGS) \
	$(LIBGNUTLS_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(WARN_CFLAGS) \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DVINO_ICONDIR=\""$(datadir)/pixmaps\"" \
	-DVINO_CLIENTDIR=\""$(datadir)/gnome/vino"\" \
	-DVINO_GLADEDIR=\""$(datadir)/gnome/vino"\" \
	$(NULL)

libexec_PROGRAMS = vino-server

vino_server_LDADD = \
	$(VINO_SERVER_LIBS) \
	$(LIBGNUTLS_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(X_LIBS) $(XTEST_LIBS) $(XSHM_LIBS) $(XDAMAGE_LIBS) \
	$(top_builddir)/server/libvncserver/libvncserver.la \
	$(NULL)

vino_server_SOURCES = \
	vino-cursor.c \
	vino-cursor.h \
	vino-enums.c \
	vino-enums.h \
	vino-fb.c \
	vino-fb.h \
	vino-http.c \
	vino-http.h \
	vino-input.c \
	vino-input.h \
	vino-main.c \
	vino-marshal.c \
	vino-marshal.h \
	vino-prefs.c \
	vino-prefs.h \
	vino-prompt.c \
	vino-prompt.h \
	vino-server.c \
	vino-server.h \
	vino-shell.c \
	vino-shell.h \
	vino-util.c \
	vino-util.h \
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

CORBA_SRCLIST = \
	GNOME_RemoteDesktop-skels.c \
	GNOME_RemoteDesktop-common.c \
	GNOME_RemoteDesktop.h \
	$(NULL)

$(CORBA_SRCLIST): $(top_srcdir)/server/GNOME_RemoteDesktop.idl $(ORBIT_IDL)
	$(ORBIT_IDL) --nostubs -I $(BONOBO_IDLDIR) $(top_srcdir)/server/GNOME_RemoteDesktop.idl

schemasdir       = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = vino-server.schemas.in
schemas_DATA     = $(schemas_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

%.server.in: %.server.in.in
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

serverdir       = $(libdir)/bonobo/servers
server_in_files = GNOME_RemoteDesktop.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

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

gladedir   = $(datadir)/gnome/vino
glade_DATA = vino-prompt.glade

EXTRA_DIST = \
	vino-prompt.glade \
	vino-server.schemas.in \
	vino-marshal.list \
	GNOME_RemoteDesktop.server.in.in \
	GNOME_RemoteDesktop.idl \
	$(NULL)

BUILT_SOURCES = \
	$(CORBA_SRCLIST) \
	vino-enums.c \
	vino-enums.h \
	vino-marshal.c \
	vino-marshal.h \
	$(NULL)

CLEANFILES = \
	GNOME_RemoteDesktop.server.in \
	GNOME_RemoteDesktop.server \
	$(BUILT_SOURCES) \
	$(NULL)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

$(vino_server_OBJECTS): $(BUILT_SOURCES)
