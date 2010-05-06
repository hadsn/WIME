export CFLAGS?=-g -Wall
export CXXFLAGS:=$(CFLAGS)
export LDFLAGS?=
export PREFIX?=/usr/local
export WINEDIR?=/usr/local
export WINELIBDIR?=$(WINEDIR)/lib/wine
export WINEINCDIR?=$(WINEDIR)/include/wine
export INSTALL?=install
export ARCH?=arch
export BIN32?=$(shell $(ARCH))
export WIMEDLLDIR?=$(WINELIBDIR)
export CONFDIR=.wime

enable_xim?=1
enable_gim?=1

ifeq ($(enable_gim),1)
export GTKPC?=gtk+-2.0
export GTKLOCALEDIR?=$(shell pkg-config $(GTKPC) --variable=prefix)/share/locale
endif

###################################

override CFLAGS+=-std=gnu99 -Wno-multichar -fgnu89-inline
export DEPEND=depend

subdirs=lib io so dll exe wimectrl
dotdir=$(PREFIX)/share/wime
rcfile=hinshi

ifeq ($(enable_xim),1)
subdirs+=xim
endif
ifeq ($(enable_gim),1)
subdirs+=gim
endif
ifeq ($(enable_qim),1)
subdirs+=qim
endif

include def.mk

all: depend
	$(callsubmake)

depend clean:
	$(callsubmake)

install:
	$(callsubmake)
	$(INSTALL) -d $(dotdir)
	for f in $(rcfile);do [ -e $(dotdir)/$$f ]||$(INSTALL) -m 0644 $$f $(dotdir);done

uninstall:
	$(callsubmake)
	$(RM) -r $(dotdir)
