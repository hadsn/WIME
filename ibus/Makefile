include ../conf.mk
ifeq ($(shell ../vercmp $(shell pkg-config --modversion $(IBUSPC)) -ge 1.4) , 1)
  pkgdatadir=$(shell pkg-config --variable=pkgdatadir $(IBUSPC))
else
  pkgdatadir=$(shell pkg-config --variable=prefix $(IBUSPC))/share/ibus
endif
immoddir=$(pkgdatadir)/engine
app=ibus-engine-wime
app2=wime.xml
objs=engine.o main.o
icon=ibus-wime.png
override CFLAGS+=-DLOGMARK=\'i\' -I.. $$(pkg-config --cflags $(IBUSPC)) -DPKGDATADIR=$(pkgdatadir) -DWIMEVER=$(VERSION) -DAPPNAME=$(app) -DENGINEDSC="$(DSC)"
override LDFLAGS+=$$(pkg-config --libs $(IBUSPC))
solibs=../so/libwime.so
include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(INSTALL) -t $(immoddir) $(app)
	$(INSTALL) $(PERM) $(app2) $(pkgdatadir)/component
	$(INSTALL) $(PERM) $(icon) $(pkgdatadir)/icons

uninstall:
	$(RM) $(immoddir)/$(app) $(pkgdatadir)/component/$(app2) $(pkgdatadir)/icons/$(icon)

$(app2): wime.xml.tmp
	cpp -P -traditional $(CFLAGS) $< >$@

main.o $(app2):inf.h
