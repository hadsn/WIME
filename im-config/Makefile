include ../conf.mk
imconfig_data=/usr/share/im-config/data
imconfig_rc=/usr/share/im-config/xinputrc.common
imconfig_ver=$(shell awk '/IM_CONFIG_VERSION/{match($$0,/[0-9]+(\.[0-9]+)+/);print substr($$0,RSTART,RLENGTH)}' $(imconfig_rc))

ifeq ($(shell ../vercmp $(imconfig_ver) -gt 0.5) , 1)
app=50_wime.conf 50_wime.rc
else
app=50_wime.dsc 50_wime.im
endif

include ../def.mk

install:
	$(INSTALL) $(INSPERM) $(app) $(imconfig_data)

uninstall:
	$(RM) $(addprefix $(imconfig_data)/,$(app))

50_wime.dsc:dsc
	sed 's|@PREFIX@|$(PREFIX)|;s|@DSC@|$(DSC)|' $^ >$@

50_wime.im:im
	cp $^ $@

50_wime.conf:conf
	sed 's|@PREFIX@|$(PREFIX)|;s|@DSC@|$(DSC)|' $^ >$@

50_wime.rc:im
	cp $^ $@
