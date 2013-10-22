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

#ターゲットを引数にしてサブディレクトリのMakefileを呼び出す。
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
	$(INSTALL) -d $(DATADIR)
	for f in $(CONFFILE);do [ -e $(DATADIR)/$$f ]||$(INSTALL) $(PERM) $$f $(DATADIR);done

uninstall:
	$(callsubmake)
	$(RM) -r -f $(DATADIR)

