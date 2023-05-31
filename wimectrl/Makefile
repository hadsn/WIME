override CFLAGS+=-I.. $$(pkg-config --cflags x11) -DWIME_VERSION=$(VERSION)
override LDFLAGS+=$$(pkg-config --libs x11) -lrt -pthread

app=wimectrl
objs=main.o
libs=../lib/libwimeut.a
solibs=../so/libwime.so

include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(PREFIX)/bin
	$(INSTALL) $(app) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)
