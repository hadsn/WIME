include ../../conf.mk

immoddir=$(DESTDIR)$(shell pkg-config $(PKG) --variable=libdir)/$(subst +,,$(PKG))/$(shell pkg-config $(PKG) --variable=gtk_binary_version)/immodules
localedir=$(shell pkg-config $(PKG) --variable=prefix)/share/locale

override CFLAGS+=-fPIC -I../.. $$(pkg-config $(PKG) --cflags) -DLOCALEDIR=\"$(localedir)\"
override LDFLAGS+=$$(pkg-config $(PKG) --libs)

vpath %.c ..
vpath %.h ..

app=im-wime.so
objs=gim.o
libs=../../lib/libwimeut.a
solibs=../../so/libwime.so

include ../../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(immoddir)
	$(INSTALL) $(INSPERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)
