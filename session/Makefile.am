NULL = 

INCLUDES = \
	-DVINO_LOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(VINO_SESSION_CFLAGS) \
	$(VINO_DEBUG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

if SESSION_SUPPORT
bin_PROGRAMS = vino-session

vino_session_SOURCES = vino-session.c
vino_session_LDADD   = $(VINO_SESSION_LIBS)
endif

EXTRA_DIST = \
	README \
	gsm-remote-desktop.diff \
	vino-session.c \
	$(NULL)
