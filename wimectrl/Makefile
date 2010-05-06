override CFLAGS+=-I.. $$(pkg-config --cflags x11) -DLOGMARK=\'c\'
override LDFLAGS+=$$(pkg-config --libs x11)

app=wimectrl
objs=main.o
libs=../lib/$(shell $(ARCH))/libwimeut.a
solibs=../so/libwime.so

#-L
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))
#-l
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))

-include $(DEPEND)


$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS)


all:$(app)

install:
	$(INSTALL) $(app) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)

clean:
	$(RM) $(app) $(objs) $(DEPEND)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
