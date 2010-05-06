override CFLAGS+=-m32 -I..
override LDFLAGS+=-m32 -mno-cygwin -lrt

CC=winegcc

app=wime
dll=$(app).dll.so
spec=$(app).spec
def=$(app).def
objs=libwime.o
solibs=../io/libwimeio.so

#-L
override LDFLAGS+=$(addprefix -L,$(dir $(solibs)))
#-l
override LDFLAGS+=$(patsubst lib%.so,-l%,$(notdir $(solibs)))

include ../def.mk
-include $(DEPEND)


$(dll): $(objs) $(libs) $(solibs) $(spec)
	$(CC) -shared -o $@ $(objs) $(libs) $(spec) $(LDFLAGS)

$(def): $(spec)
	winebuild -w --def -o $@ -E $(spec)


all:$(dll) $(def)

clean:
	$(RM) $(objs) $(dll) $(def) $(DEPEND)

install:
	$(INSTALL) $(dll) $(WIMEDLLDIR)

uninstall:
	$(RM) $(WIMEDLLDIR)/$(dll)

$(DEPEND): $(objs:.o=.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
