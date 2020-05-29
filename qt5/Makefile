include ../conf.mk

app=libwimeplatforminputcontextplugin.so
mk=Makefile.qt

include ../def.mk

$(app):$(mk) qim.cc qim.h
	$(MAKE) -f $(mk)

$(mk): qim.h qt5.pro
	qmake -makefile -o $(mk)

clean:
	[ -e $(mk) ] && $(MAKE) -f $(mk) clean || true
	$(RM) $(mk) $(app) *.d .qmake*

install uninstall:
	$(MAKE) -f $(mk) $@ INSTALL_ROOT=$(DESTDIR)
#	$(MAKE) -f $(mk) $@ INSTALL_ROOT=$(DESTDIR) QINSTALL_PROGRAM="$(INSTALL)"

#(C) 2020 thomas
