include ../conf.mk

imconfig_data=$(DESTDIR)/usr/share/im-config/data
imconfig_rc=/usr/share/im-config/xinputrc.common
imconfig_ver=$(shell awk '/IM_CONFIG_VERSION/{match($$0,/[0-9]+(\.[0-9]+)+/);print substr($$0,RSTART,RLENGTH)}' $(imconfig_rc))

ifeq "$(shell ../vercmp $(imconfig_ver) -gt 0.5)" "1"
app=50_wime.conf 50_wime.rc
else
app=50_wime.dsc 50_wime.im
endif

include ../def.mk

install:
	$(MKDIRP) $(imconfig_data)
	$(INSTALL) $(PERM) $(app) $(imconfig_data)

uninstall:
	$(RM) $(addprefix $(imconfig_data)/,$(app))

50_wime.dsc:dsc
	sed 's|@PREFIX@|$(PREFIX)|;s|@DSC@|$(DSC)|' $^ >$@

50_wime.im:im
	sed 's|@GTKPC@|$(GTKPC)|g;s|@QTPC@|$(QTPC)|g' $^ >$@

50_wime.conf:conf
	sed 's|@PREFIX@|$(PREFIX)|;s|@DSC@|$(DSC)|' $^ >$@

50_wime.rc:im
	sed 's|@GTKPC@|$(GTKPC)|g;s|@QTPC@|$(QTPC)|g' $^ >$@
