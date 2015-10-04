include ../conf.mk

libdir=$(PREFIX)/lib

override CFLAGS+=-fPIC $(shell pkg-config --cflags x11) -DCONFDIR=\"$(CONFDIR)\" -DPREFIX=\"$(PREFIX)\" -I..
override LDFLAGS+=$(shell pkg-config --libs x11) -lrt

#amd64でi386-wineを動かしているとき
ifeq ($(WINE32) , 1)
override CFLAGS+=-m32
override LDFLAGS+=-m32
#さらにfreebsdのとき。LDFLAGSのlibX11.soのパスを/usr/local/libから/usr/local/lib32にする。
ifeq ($(FREEBSD),1)
override LDFLAGS:=$(subst local/lib,local/lib32,$(LDFLAGS))
endif
endif

app=libwimeio.so
objs=wimeio.o

include ../def.mk

install:
	$(MKDIRP) $(libdir)
	$(INSTALL) $(PERM) $(app) $(libdir)

uninstall:
	$(RM) $(libdir)/$(app)


$(app):$(objs) $(libwimeutarch)
	$(CC) -shared -o $@ $^  $(LDFLAGS)
