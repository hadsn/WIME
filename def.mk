ifeq "$(WOW64)" "1"
libwimeutarch=../lib/$(BIN32NAME)/libwimeut.a
else
libwimeutarch=../lib/libwimeut.a
endif

ifdef solibs
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))		#-L
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))	#-l
endif

all:$(app) $(app2) $(app3)

clean:
	$(RM) -fr $(app) $(objs) $(objs:.o=.d) $(app2) $(app3)


ifneq "$(MAKECMDGOALS)" "clean"
    -include $(objs:.o=.d)
endif

%.d: %.c
	@set -e;\
	$(CC) $(DEPFLAGS) $(CFLAGS) $< |\
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@

%.d: %.cc
	@set -e;\
	$(CXX) $(DEPFLAGS) $(CXXFLAGS) $< |\
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@
