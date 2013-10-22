CFLAGS:=-g -Wall
CXXFLAGS:=-g -Wall
LDFLAGS?=
PREFIX?=/usr/local

WINEDIR?=/usr/local
WINEINCDIR?=$(WINEDIR)/include/wine

INSTALL?=install
CONFDIR?=.wime
WINE32?=1

USE_XIM?=1
#USE_IMCONFIG?=1
GTKPC?=gtk+-2.0 gtk+-3.0
#QTPC?=QtGui
#IBUSPC?=ibus-1.0

###################################

PREFIX:=$(DESTDIR)$(PREFIX)

override CFLAGS+=-std=gnu99 -Wno-multichar -fgnu89-inline
DEPFLAGS=-MM -MG
VERSION=3.5.0
BIN32NAME=bin32
PERM=-m 644
DSC=feigned canna
DATADIR=$(PREFIX)/share/wime
CONFFILE=hinshi
MKDIRP=mkdir -p
USE_SERVER=1
