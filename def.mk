ifeq ($(WINE32) , 1)
libwimeutarch=../lib/$(BIN32NAME)/libwimeut.a
else
libwimeutarch=../lib/libwimeut.a
endif

ifdef solibs
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))		#-L
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))	#-l
endif

all:$(app) $(app2)

clean:
	$(RM) -fr $(app) $(objs) $(objs:.o=.d) $(app2) $(app3)


-include $(objs:.o=.d)

%.d: %.c
	@set -e;\
	$(CC) -MM -MG $(CFLAGS) $< |\
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@

%.d: %.cc
	@set -e;\
	$(CC) -MM -MG $(CXXFLAGS) $< |\
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@
