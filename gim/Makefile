include ../conf.mk

clean:
	$(RM) -rf $(GTKPC)

%:
	for dir in $(GTKPC);do \
		if [ ! -e $$dir ]; then \
			mkdir $$dir;\
		fi;\
		$(MAKE) -C $$dir -f ../Makefile-b.mk PKG=$$dir $@ || exit 1;\
	done
