#各変数はconf.mkにあります。
include conf.mk

###################################

subdirs=lib so io exe wimectrl $(PROG)

.PHONY: $(subdirs)
all clean install uninstall: $(subdirs)

$(subdirs):
	+$(MAKE) -C $@ $(MAKECMDGOALS)

so io: lib
exe: lib io
wimectrl $(PROG): lib so

install:
	$(INSTALL) -d $(DATADIR)
	for f in $(CONFFILE);do [ -e $(DATADIR)/$$f ]||$(INSTALL) $(PERM) $$f $(DATADIR);done


uninstall:
	$(RM) -r -f $(DATADIR)



