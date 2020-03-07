#各変数はconf.mkにあります。
include conf.mk

###################################

subdirs=lib so

ifeq "$(USE_SERVER)" "1"
subdirs+=io exe wimectrl
endif
ifeq "$(USE_XIM)" "1"
subdirs+=xim
endif
ifneq "$(GTKPC)" ""
subdirs+=gim
endif
ifneq "$(QTPC)" ""
subdirs+=qim
endif
ifeq "$(USE_IMCONFIG)" "1"
subdirs+=im-config
endif
ifneq "$(IBUSPC)" ""
subdirs+=ibus
endif


.PHONY: $(subdirs)
all clean install uninstall: $(subdirs)

$(subdirs):
	+$(MAKE) -C $@ $(MAKECMDGOALS)

so io: lib
exe: lib io
wimectrl xim gim qim ibus: lib so

install:
	$(INSTALL) -d $(DATADIR)
	for f in $(CONFFILE);do [ -e $(DATADIR)/$$f ]||$(INSTALL) $(PERM) $$f $(DATADIR);done


uninstall:
	$(RM) -r -f $(DATADIR)



