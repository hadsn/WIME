include ../conf.mk

qtpc=QtGui
override CXXFLAGS+=-fPIC -I.. $$(pkg-config $(qtpc) x11 --cflags) -Wno-unused-private-field
override LDFLAGS+=$$(pkg-config $(qtpc) --libs)

qtlibdir=$(shell pkg-config $(qtpc) --variable=libdir)/qt4
immoddir=$(DESTDIR)$(qtlibdir)/plugins/inputmethods

vpath %.cc ../qt5
vpath %.h ../qt5

app=libwimeqim.so
objs=qim.o
libs=../lib/libwimeut.a
solibs=../so/libwime.so

include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CXX) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(immoddir)
	$(INSTALL) $(PERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)
