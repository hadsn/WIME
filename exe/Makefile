override CFLAGS+=-fno-builtin -mconsole -I..
override LDFLAGS+=-limm32 -lgdi32 -lrt -lpthread
override CC=winegcc

ifeq "$(WOW64)" "1"
override CC:=$(CC32_ENV) $(CC)
override CFLAGS+=-m32
override LDFLAGS+=-m32
endif

ifneq "$(OS)" "Linux"

#/usr/local/includeがないとcanna/RK.hが読み込まれない。
#すべてのCFLAGSに/usr/local/includeをつけると/usr/local/includeのiconv.hが使われてしまう。
#ということで、exeのコンパイル時のみlocalをつける。
override CFLAGS+=-I/usr/local/include

#clangのときはこれをつけないとWCHARが32bitになってしまう。
ifeq "$(USE_CLANG)" "1"
override CFLAGS+=-fno-short-wchar
endif

endif

winever=$(shell  wine --version|sed -r 's/^[^0-9]+//')
ifeq "$(shell ../vercmp $(winever) -ge 8.2)" "1"
override CFLAGS+=-DHAVE_IMMDEV
endif

app=wime
app2=$(app).exe.so
objs=apisup.o canna.o main.o at.o
solibs=../io/libwimeio.so

include ../def.mk

$(objs):override CFLAGS+=-DCALLINGCONV=$(shell ./cdecl)

#wine1.1.16ではwinegccは出力ファイル名に意地でも拡張子を付けるみたい。
#拡張子がなければexeをつけてfoo.exeとfoo.exe.soを出力し、
#何か拡張子があればfoo.extとfoo.ext.soを出力する。
#wime.exeが出力されればwimeに変更する。
$(app):$(objs) $(libwimeutarch) $(solibs) $(app3)
	$(CC)  -o $(app) $(objs) $(libwimeutarch) $(LDFLAGS)
	[ -e $(app).exe ] && mv $(app).exe $(app)

install:
	$(MKDIRP) $(PREFIX)/bin
	$(INSTALL) $(app) $(app2) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)
	$(RM) $(PREFIX)/bin/$(app2)
