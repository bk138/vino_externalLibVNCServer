NULL =

vino_clientdir = $(datadir)/gnome/vino
vino_client_DATA = vino-client.jar vino-client.html

if JAVA_SDK_AVAILABLE
vino-client.jar: $(vino_client_CLASSES)
	$(JAR) cmf $(srcdir)/vino-client.manifest $@ $^

%.class: %.java
	(builddir=$$(pwd) && cd $(srcdir) && $(JAVAC) -d $$builddir $^)
endif

EXTRA_DIST = 			\
	vino-client.jar		\
	vino-client.html	\
	$(vino_client_SRCS)	\
	README			\
	vino-client.manifest	\
	$(NULL)

CLEANFILES = $(vino_client_CLASSES)

dist-hook:
	cd $(distdir) ; rm -f $(CLEANFILES)

vino_client_CLASSES = $(vino_client_SRCS:.java=.class) vncviewer/ViewportFrame.class
vino_client_SRCS = \
	rdr/EndOfStream.java \
	rdr/Exception.java \
	rdr/IOException.java \
	rdr/InStream.java \
	rdr/JavaInStream.java \
	rdr/JavaOutStream.java \
	rdr/MemInStream.java \
	rdr/MemOutStream.java \
	rdr/OutStream.java \
	rdr/ZlibInStream.java \
	rfb/AliasParameter.java \
	rfb/AuthFailureException.java \
	rfb/BoolParameter.java \
	rfb/CConnection.java \
	rfb/CMsgHandler.java \
	rfb/CMsgReader.java \
	rfb/CMsgReaderV3.java \
	rfb/CMsgWriter.java \
	rfb/CMsgWriterV3.java \
	rfb/CSecurity.java \
	rfb/CSecurityNone.java \
	rfb/CSecurityTls.java \
	rfb/CSecurityVncAuth.java \
	rfb/Configuration.java \
	rfb/ConnFailedException.java \
	rfb/ConnParams.java \
	rfb/Cursor.java \
	rfb/Decoder.java \
	rfb/DesCipher.java \
	rfb/Encoder.java \
	rfb/Encodings.java \
	rfb/Exception.java \
	rfb/Hextile.java \
	rfb/HextileDecoder.java \
	rfb/Hostname.java \
	rfb/IntParameter.java \
	rfb/Keysyms.java \
	rfb/LogWriter.java \
	rfb/ManagedPixelBuffer.java \
	rfb/MsgTypes.java \
	rfb/PasswdGetter.java \
	rfb/PixelBuffer.java \
	rfb/PixelFormat.java \
	rfb/RREDecoder.java \
	rfb/RawDecoder.java \
	rfb/SecTypes.java \
	rfb/StringParameter.java \
	rfb/UnicodeToKeysym.java \
	rfb/VncAuth.java \
	rfb/VoidParameter.java \
	rfb/ZRLEDecoder.java \
	vncviewer/AboutDialog.java \
	vncviewer/CConn.java \
	vncviewer/ClipboardDialog.java \
	vncviewer/DesktopWindow.java \
	vncviewer/Dialog.java \
	vncviewer/F8Menu.java \
	vncviewer/InfoDialog.java \
	vncviewer/MessageBox.java \
	vncviewer/OptionsDialog.java \
	vncviewer/OptionsDialogCallback.java \
	vncviewer/PasswdDialog.java \
	vncviewer/PixelBufferImage.java \
	vncviewer/ServerDialog.java \
	vncviewer/VNCViewer.java \
	$(NULL)
