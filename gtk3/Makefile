pc?=$(subst gtk,gtk+-,$(notdir $(shell pwd))).0
libdirname?=$(subst +,,$(pc))
immoddir=$(DESTDIR)$(shell pkg-config $(pc) --variable=libdir)/$(libdirname)/$(shell pkg-config $(pc) --variable=gtk_binary_version)/immodules
localedir=$(shell pkg-config $(pc) --variable=prefix)/share/locale

override CFLAGS+=-fPIC -I.. $$(pkg-config $(pc) --cflags) -DLOCALEDIR=\"$(localedir)\"
override LDFLAGS+=$$(pkg-config $(pc) --libs)

app?=im-wime.so
objs=gim.o
libs=../lib/libwimeut.a
solibs=../so/libwime.so

include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(immoddir)
	$(INSTALL) $(INSPERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)

#(C) 2020 thomas
