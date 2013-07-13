#各変数はconf.mkにあります。
include conf.mk

###################################

subdirs=lib io so dll exe wimectrl

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

define callsubmake
for d in $(subdirs);do\
$(MAKE) -C $$d $@ || exit 1;\
done
endef

all:
	$(callsubmake)

clean:
	$(callsubmake)

install:
	$(callsubmake)
	$(INSTALL) -d $(dotdir)
	for f in $(rcfile);do [ -e $(dotdir)/$$f ]||$(INSTALL) $(PERM) $$f $(dotdir);done

uninstall:
	$(callsubmake)
	$(RM) -r $(dotdir)

