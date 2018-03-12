include ../conf.mk

ifeq ($(shell ../vercmp $(shell pkg-config --modversion $(IBUSPC)) -ge 1.4) , 1)
  pkgdatadir=$(shell pkg-config --variable=pkgdatadir $(IBUSPC))
else
  pkgdatadir=$(shell pkg-config --variable=prefix $(IBUSPC))/share/ibus
endif
componentdir=$(DESTDIR)$(pkgdatadir)/component
immoddir=$(DESTDIR)/usr/lib/ibus

app=ibus-engine-wime
app2=wime.xml
objs=engine.o main.o
icon=ibus-wime.png

override CFLAGS+=-I.. $$(pkg-config --cflags $(IBUSPC)) -DIMMODDIR=$(immoddir) -DVERSION=$(VERSION) -DAPPNAME=$(app) -DDSC="$(DSC)" -DICON=$(DATADIR)/$(icon)

override LDFLAGS+=$$(pkg-config --libs $(IBUSPC) x11)

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
