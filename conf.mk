CFLAGS?= -g -Wall
CXXFLAGS?= -g -Wall
LDFLAGS?=-g
PREFIX?=/usr/local
CONFDIR?=.wime
PROG?=xim gtk2 gtk3 qt4 qt5 ibus im-config

WINE?=wine
WINEDIR?=/usr/local
WINEINCDIR?=$(WINEDIR)/include/wine
WOW64?=1
#CC32_ENV?=schroot -c dev32 --
#CC32_ENV?=

INSTALL?=install
MKDIRP?=install -d

#DESTDIR=

###################################

PREFIX:=$(DESTDIR)$(PREFIX)

VERSION=4.1.3
BIN32NAME=bin32
PERM=-m 644
DSC=feigned canna
DATADIR:=$(PREFIX)/share/wime
CONFFILE=hinshi
OS:=$(shell uname)

override CFLAGS+=-Wno-multichar -fgnu89-inline -DWIME_VERSION=$(VERSION) -Wno-address-of-packed-member -Wformat=0
override CXXFLAGS+=-Wformat=0
override DEPFLAGS=-MM -MG


###################################
## clang
###################################
ifeq "$(USE_CLANG)" "1"
  override CFLAGS+=-Wno-invalid-source-encoding -Wno-gnu-designator
  override CXXFLAGS+=-std=c++11
  CC=clang
  CXX=clang++
endif
