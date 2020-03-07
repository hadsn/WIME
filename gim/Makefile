#GTKPCの各pcに対してMakefile-b.mkを実行する。

include ../conf.mk

.PHONY: $(GTKPC)

all install uninstall: $(GTKPC)

$(GTKPC):
	mkdir -p $@
	$(MAKE) -C $@ -f ../Makefile-b.mk PKG=$@ $(MAKECMDGOALS)

clean:
	$(RM) -rf $(GTKPC)
