override CFLAGS+=-I.. $$(pkg-config --cflags x11) -DLOGMARK=\'x\'
override LDFLAGS+=$$(pkg-config --libs x11) -lrt

app=wimexim
objs=callback.o main.o enc_nego.o connect.o im.o open.o query_ext.o ic.o sync.o forward_ev.o triggerkey.o commit.o overthespot.o onthespot.o offthespot.o rootwin.o
libs=../lib/$(shell $(ARCH))/libwimeut.a
solibs=../so/libwime.so

#-L
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))
#-l
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))

include ../def.mk
-include $(DEPEND)

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $@ $(objs) $(libs) $(LDFLAGS) 


all:$(app)

clean:
	$(RM) $(app) $(objs) $(DEPEND)

install:
	$(INSTALL) $(app) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)

$(DEPEND): $(objs:%.o=%.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
