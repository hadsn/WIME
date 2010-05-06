override CFLAGS+=-m32 -I.. $$(pkg-config --cflags x11) -DCONFDIR=\"$(CONFDIR)\" -DPREFIX=\"$(PREFIX)\"
override LDFLAGS+=-m32 $$(pkg-config --libs x11) -lrt

app=libwimeio.so
objs=wimeio.o
libs=../lib/$(BIN32)/libwimeut.a

include ../def.mk
-include $(DEPEND)


$(app):$(objs) $(libs)
	$(CC) -shared -o $@ $^  $(LDFLAGS)


all:$(app)

clean:
	$(RM) $(app) $(objs) $(DEPEND)

install:
	$(INSTALL) $(app) $(PREFIX)/lib

uninstall:
	$(RM) $(PREFIX)/lib/$(app)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
