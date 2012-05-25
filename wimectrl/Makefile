include ../conf.mk
override CFLAGS+=-I.. $$(pkg-config --cflags x11) -DLOGMARK=\'c\'
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=wimectrl
objs=main.o
libs=../lib/libwimeut.a
solibs=../so/libwime.so
include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(INSTALL) $(app) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)
