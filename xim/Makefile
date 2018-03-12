include ../conf.mk

override CFLAGS+=$$(pkg-config --cflags x11) -DWIME_VERSION=$(VERSION) -I..
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=wimexim
objs=callback.o main.o enc_nego.o connect.o im.o open.o query_ext.o ic.o sync.o forward_ev.o triggerkey.o commit.o overthespot.o onthespot.o offthespot.o rootwin.o
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
