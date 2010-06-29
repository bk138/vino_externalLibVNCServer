NULL =

INCLUDES = \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(VINO_TOOLS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	$(NULL)

bin_PROGRAMS = vino-passwd

vino_passwd_SOURCES = \
	vino-passwd.c \
	$(NULL)

vino_passwd_LDADD = \
	$(VINO_TOOLS_LIBS) \
	$(NULL)

