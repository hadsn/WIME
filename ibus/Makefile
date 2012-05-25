include ../conf.mk
ifeq ($(shell ../vercmp $(shell pkg-config --modversion $(IBUSPC)) -ge 1.4) , 1)
  pkgdatadir=$(shell pkg-config --variable=pkgdatadir $(IBUSPC))
else
  pkgdatadir=$(shell pkg-config --variable=prefix $(IBUSPC))/share/ibus
endif
app=ibus-engine-wime
app2=wime.xml
objs=engine.o main.o
icon=ibus-wime.png
override CFLAGS+=-DLOGMARK=\'i\' -I.. $(shell pkg-config --cflags $(IBUSPC)) -DPKGDATADIR=$(pkgdatadir) -DWIMEVER=$(VERSION) -DAPPNAME=$(app) -DENGINEDSC="$(DSC)"
override LDFLAGS+=$(shell pkg-config --libs $(IBUSPC))
solibs=../so/libwime.so
include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(INSTALL) $(app) $(pkgdatadir)/engine
	$(INSTALL) $(INSPERM) $(app2) $(pkgdatadir)/component
	$(INSTALL) $(INSPERM) $(icon) $(pkgdatadir)/icons

uninstall:
	$(RM) $(immoddir)/$(app)

$(app2): wime.xml.tmp
	cpp -P -traditional $(CFLAGS) $< >$@

main.o $(app2):inf.h
