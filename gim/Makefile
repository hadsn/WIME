include ../conf.mk
override CFLAGS+=-fPIC -I.. $$(pkg-config $(GTKPC) --cflags) -DLOCALEDIR=\"$(GTKLOCALEDIR)\" -DLOGMARK=\'g\'
override LDFLAGS+=$$(pkg-config $(GTKPC) --libs)

immoddir=$(shell pkg-config $(GTKPC) --variable=libdir)/$(subst +,,$(GTKPC))/$(shell pkg-config $(GTKPC) --variable=gtk_binary_version)/immodules

app=im-wime.so
objs=gim.o
solibs=../so/libwime.so
include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(INSTALL) $(INSPERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)
