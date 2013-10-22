include ../conf.mk

override CFLAGS+=-fPIC -I.. $$(pkg-config --cflags x11)
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=libwime.so
app2=win.h
objs=wimeapi.o corr.o winkey.o xres.o
libs=../lib/libwimeut.a

include ../def.mk

$(app):$(objs) $(libs)
	$(CC) -shared -o $@ $^  $(LDFLAGS)

win.h:
	echo '#ifndef WIME_SO_WIN' >$@
	echo '#define WIME_SO_WIN' >>$@
	grep -h '\<VK_' ${WINEINCDIR}/windows/winuser.* >>$@
	echo '#endif' >>$@


install:
	$(MKDIRP) $(PREFIX)/lib
	$(INSTALL) $(PERM) $(app) $(PREFIX)/lib

uninstall:
	$(RM) $(PREFIX)/lib/$(app)
