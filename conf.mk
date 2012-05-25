CFLAGS?=-g -Wall
CXXFLAGS?=$(CFLAGS)
LDFLAGS?=
PREFIX?=/usr/local
WINEDIR?=/usr/local
WINELIBDIR?=$(WINEDIR)/lib/wine
WINEINCDIR?=$(WINEDIR)/include/wine
INSTALL?=install
WIMEDLLDIR?=$(WINELIBDIR)
CONFDIR?=.wime
WINE32?=1

enable_xim?=1
enable_gim?=1
enable_qim?=0
enable_imconfig?=0
enable_ibus?=0

ifeq ($(enable_gim),1)
GTKPC?=gtk+-2.0
GTKLOCALEDIR?=$(shell pkg-config $(GTKPC) --variable=prefix)/share/locale
endif

ifeq ($(enable_ibus),1)
IBUSPC?=ibus-1.0
endif

###################################

override CFLAGS+=-std=gnu99 -Wno-multichar -fgnu89-inline
DEPFLAGS=-MM -MG
VERSION=3.4.4
BIN32NAME=bin32
INSPERM=-m 644
DSC=feigned canna
dotdir=$(PREFIX)/share/wime
rcfile=hinshi
