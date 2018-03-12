include ../conf.mk

override CXXFLAGS+=-fPIC -I.. $$(pkg-config $(QTPC) x11 --cflags)
override LDFLAGS+=$$(pkg-config $(QTPC) --libs)

qtlibdir=$(shell pkg-config $(QTPC) --variable=libdir)/qt4
immoddir=$(DESTDIR)$(qtlibdir)/plugins/inputmethods

app=libwimeqim.so
objs=qim.o
solibs=../so/libwime.so

include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CXX) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(MKDIRP) $(immoddir)
	$(INSTALL) $(PERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)
