include ../conf.mk

override CFLAGS+=-fno-builtin -mconsole -mno-cygwin -DLOGMARK=SERVER_MARK -DWIME_VERSION=$(VERSION) -I..
override LDFLAGS+=-limm32 -lgdi32 -mno-cygwin -lrt -lpthread
override CC=winegcc

ifeq ($(WINE32) , 1)
override CFLAGS+=-m32
override LDFLAGS+=-m32
endif

ifeq ($(FREEBSD),1)
#/usr/local/includeがないとcanna/RK.hが読み込まれない。
#すべてのCFLAGSに/usr/local/includeをつけると/usr/local/includeのiconv.hが使われてしまう。
#ということで、exeのコンパイル時のみlocalをつける。
override CFLAGS+=-I/usr/local/include
endif

app=wime.exe.so
objs=apisup.o canna.o main.o at.o
solibs=../io/libwimeio.so

exename=$(basename $(app))	# .soなし
app2=$(basename $(exename))	# .exe.soなし

include ../def.mk

#wine1.1.16ではwinegccは出力ファイル名に意地でも拡張子を付けるみたい。
#拡張子がなければexeをつけてfoo.exeとfoo.exe.soを出力し、
#何か拡張子があればfoo.extとfoo.ext.soを出力する。
#16以上ならwime.exeをwimeに変更する。
winever=$(word 1,$(sort $(word 1,$(shell wine --version)) wine-1.1.16))
ifeq ($(winever),wine-1.1.16)	
renamecmd=mv
else
renamecmd=:
endif


$(app):$(objs) $(libwimeutarch) $(solibs)
	$(CC) -o $(app2) $(objs) $(libwimeutarch) $(LDFLAGS)
	$(renamecmd) $(exename) $(app2)

install:
	$(MKDIRP) $(PREFIX)/bin
	$(INSTALL) $(app) $(app2) $(PREFIX)/bin

uninstall:
	$(RM) $(PREFIX)/bin/$(app)
	$(RM) $(PREFIX)/bin/$(app2)
