include ../conf.mk
override CFLAGS+=-fno-builtin -I..
override LDFLAGS+=-mno-cygwin -lrt

ifeq ($(WINE32) , 1)
override CFLAGS+=-m32
override LDFLAGS+=-m32
endif

CC=winegcc

exe=wime
app=$(exe).dll.so
app2=$(exe).def
spec=$(exe).spec
objs=libwime.o
solibs=../io/libwimeio.so
include ../def.mk


$(app): $(objs) $(solibs) $(spec)
	$(CC) -shared -o $@ $(objs) $(spec) $(LDFLAGS)

$(app2): $(spec)
	winebuild -w --def -o $@ -E $(spec)


install:
	$(INSTALL) $(INSPERM) $(app) $(WIMEDLLDIR)

uninstall:
	$(RM) $(WIMEDLLDIR)/$(app)
