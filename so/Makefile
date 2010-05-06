override CFLAGS+=-fpic -I.. $$(pkg-config --cflags x11)
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=libwime.so
objs=wimeapi.o corr.o winkey.o xres.o wimelog.o
libs=../lib/$(shell $(ARCH))/libwimeut.a

-include $(DEPEND)
include ../def.mk


$(app):$(objs) $(libs)
	$(CC) -shared -o $@ $^  $(LDFLAGS)

win.h:
	echo '#ifndef WIME_SO_WIN' >$@
	echo '#define WIME_SO_WIN' >>$@
	grep VK_ ${WINEINCDIR}/windows/winuser.h >>$@
	echo '#endif' >>$@


all:$(app) win.h

clean:
	$(RM) $(app) $(objs) win.h $(DEPEND)

install:
	$(INSTALL) $(app) $(PREFIX)/lib

uninstall:
	$(RM) $(PREFIX)/lib/$(app)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
