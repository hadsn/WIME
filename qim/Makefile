include ../conf.mk
QTPC=QtGui
override CXXFLAGS+=-fPIC -I.. $$(pkg-config $(QTPC) --cflags) -DLOGMARK=\'q\'
override LDFLAGS+=$$(pkg-config $(QTPC) --libs)

qtlibdir=$(shell pkg-config $(QTPC) --variable=libdir)/qt4
immoddir=$(qtlibdir)/plugins/inputmethods

app=libwimeqim.so
objs=qim.o
solibs=../so/libwime.so
include ../def.mk

$(app):$(objs) $(libs) $(solibs)
	$(CXX) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

install:
	$(INSTALL) $(INSPERM) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)

foo:
	@echo $(CFLAGS)
	@echo $(CXXFLAGS)
