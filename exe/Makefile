override CFLAGS+=-m32 -I.. -mconsole -mno-cygwin -DLOGMARK=SERVER_MARK
override LDFLAGS+=-m32 -limm32 -lgdi32 -mno-cygwin -lrt -lpthread

CC=winegcc
app=wime.exe.so
objs=apisup.o canna.o main.o at.o
libs=../lib/$(BIN32)/libwimeut.a
solibs=../io/libwimeio.so

#-L
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))
#-l
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))

ifdef DEBUG
override CFLAGS+=-DDEBUG
endif

include ../def.mk
-include $(DEPEND)

all:$(app)

$(app):$(objs) $(libs) $(solibs)
	$(CC) -o $(APPSCRIPT) $(objs) $(libs) $(LDFLAGS)
	$(RENAMECMD) $(EXENAME) $(APPSCRIPT)

install:
	$(INSTALL) $(app) $(APPSCRIPT) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)
	$(RM) $(PREFIX)/bin/$(APPSCRIPT)

clean:
	$(RM) $(app) $(APPSCRIPT) $(objs) $(DEPEND)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
