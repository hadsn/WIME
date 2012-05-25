#各変数はconf.mkにあります。
include conf.mk

###################################

subdirs=lib io so dll exe wimectrl

ifeq ($(enable_xim),1)
subdirs+=xim
endif
ifeq ($(enable_gim),1)
subdirs+=gim
endif
ifeq ($(enable_qim),1)
subdirs+=qim
endif
ifeq ($(enable_imconfig),1)
subdirs+=im-config
endif
ifeq ($(enable_ibus),1)
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
	for f in $(rcfile);do [ -e $(dotdir)/$$f ]||$(INSTALL) $(INSPERM) $$f $(dotdir);done

uninstall:
	$(callsubmake)
	$(RM) -r $(dotdir)

