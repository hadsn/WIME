include ../conf.mk

pc=ibus-1.0
ifeq "$(shell ../vercmp $(shell pkg-config --modversion $(pc)) -ge 1.4)" "1"
  pkgdatadir=$(shell pkg-config --variable=pkgdatadir $(pc))
else
  pkgdatadir=$(shell pkg-config --variable=prefix $(pc))/share/ibus
endif
componentdir=$(DESTDIR)$(pkgdatadir)/component
immoddir=$(DESTDIR)/usr/lib/ibus

app=ibus-engine-wime
app2=wime.xml
objs=engine.o main.o
icon=ibus-wime.png
libs=../lib/libwimeut.a

override CFLAGS+=-I.. $$(pkg-config --cflags $(pc)) -DIMMODDIR=$(immoddir) -DVERSION=$(VERSION) -DAPPNAME=$(app) -DDSC="$(DSC)" -DICON=$(DATADIR)/$(icon)

override LDFLAGS+=$$(pkg-config --libs $(pc) x11)

solibs=../so/libwime.so

include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(immoddir) $(componentdir) $(DATADIR)
	$(INSTALL) -t $(immoddir) $(app)
	$(INSTALL) $(PERM) $(app2) $(componentdir)
	$(INSTALL) $(PERM) $(icon) $(DATADIR)

uninstall:
	$(RM) $(immoddir)/$(app) $(componentdir)/$(app2) $(DATADIR)/$(icon)

$(app2): wime.xml.tmp
	cpp -P -traditional $(CFLAGS) -DXMLFILE=$@ $< |awk 'NF>0' >$@

main.o $(app2):inf.h
