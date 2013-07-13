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

USE_XIM?=1
#USE_IMCONFIG?=1
GTKPC?=gtk+-2.0 gtk+-3.0
#QTPC?=QtGui
#IBUSPC?=ibus-1.0

###################################

override CFLAGS+=-std=gnu99 -Wno-multichar -fgnu89-inline
DEPFLAGS=-MM -MG
VERSION=3.4.7
BIN32NAME=bin32
PERM=-m 644
DSC=feigned canna
dotdir=$(PREFIX)/share/wime
rcfile=hinshi
