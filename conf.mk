CFLAGS?= -g -Wall
CXXFLAGS?= -g -Wall
LDFLAGS?=-g
PREFIX?=/usr/local

WINE?=wine
WINEDIR?=/usr/local
WINEINCDIR?=$(WINEDIR)/include/wine
WOW64?=1
#CC32_ENV?=schroot -c dev32 --
#CC32_ENV?=

INSTALL?=install

CONFDIR?=.wime

USE_XIM?=1
USE_IMCONFIG?=0
GTKPC?=gtk+-2.0 gtk+-3.0
#QTPC?=QtGui
#IBUSPC?=ibus-1.0

USE_CLANG?=0
FREEBSD?=0

###################################

PREFIX:=$(DESTDIR)$(PREFIX)

override CFLAGS+=-Wno-multichar -fgnu89-inline
override DEPFLAGS=-MM -MG
VERSION=3.6.0
BIN32NAME=bin32
PERM=-m 644
DSC=feigned canna
DATADIR=$(PREFIX)/share/wime
CONFFILE=hinshi
MKDIRP=mkdir -p
USE_SERVER=1

###################################
## clang
###################################
ifeq ($(USE_CLANG),1)
  override CFLAGS+=-Wno-invalid-source-encoding -Wno-gnu-designator
  override CXXFLAGS+=-std=c++11
  CC=clang
  CXX=clang++
endif
