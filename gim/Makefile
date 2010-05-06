override CFLAGS+=-fpic -I.. $$(pkg-config $(GTKPC) --cflags) -DLOCALEDIR=\"$(GTKLOCALEDIR)\" -DLOGMARK=\'g\'
override LDFLAGS+=$$(pkg-config $(GTKPC) --libs)

immoddir=$(shell pkg-config $(GTKPC) --variable=libdir)/$(subst +,,$(GTKPC))/$(shell pkg-config $(GTKPC) --variable=gtk_binary_version)/immodules

app=im-wime.so
objs=gim.o
solibs=../so/libwime.so

#-L
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))
#-l
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))

-include $(DEPEND)

$(app):$(objs) $(libs) $(solibs)
	$(CC) -shared -o $@ $(objs) $(libs) $(LDFLAGS)

all: $(app) $(DEPEND)

clean:
	$(RM) $(app) $(objs) $(DEPEND)

install:
	$(INSTALL) $(app) $(immoddir)

uninstall:
	$(RM) $(immoddir)/$(app)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
