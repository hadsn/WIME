include ../conf.mk

libdir=$(PREFIX)/lib

override CFLAGS+=-fPIC -I.. $$(pkg-config --cflags x11)
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=libwime.so
app2=win.h
objs=wimeapi.o corr.o xres.o
libs=../lib/libwimeut.a

include ../def.mk

$(app):$(objs) $(libs) $(app2)
	$(CC) -shared -o $@ $(filter-out $(app2),$^)  $(LDFLAGS)

$(app2):
	echo '#ifndef WIME_SO_WIN' >$@
	echo '#define WIME_SO_WIN' >>$@
	grep -h '\<VK_' ${WINEINCDIR}/windows/winuser.* >>$@
	echo '#endif' >>$@


install:
	$(MKDIRP) $(libdir)
	$(INSTALL) $(PERM) $(app) $(libdir)

uninstall:
	$(RM) $(libdir)/$(app)
