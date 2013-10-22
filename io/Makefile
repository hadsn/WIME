include ../conf.mk

override CFLAGS+=-fPIC $$(pkg-config --cflags x11) -DCONFDIR=\"$(CONFDIR)\" -DPREFIX=\"$(PREFIX)\" -I..
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

ifeq ($(WINE32) , 1)
override CFLAGS+=-m32
override LDFLAGS+=-m32
endif

app=libwimeio.so
objs=wimeio.o

include ../def.mk

install:
	$(MKDIRP) $(PREFIX)/lib
	$(INSTALL) $(PERM) $(app) $(PREFIX)/lib

uninstall:
	$(RM) $(PREFIX)/lib/$(app)


$(app):$(objs) $(libwimeutarch)
	$(CC) -shared -o $@ $^  $(LDFLAGS)
