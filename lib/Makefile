#ここの関数は32,64両方から使用される。
#WOW64が1のとき$(BIN32NAME)/に32bitのバイナリが出力される。

include ../conf.mk
override CFLAGS+=-fPIC

#$(BIN32NAME)から呼ばれたとき用
vpath %.c ..
vpath %.h ..

app=libwimeut.a
objs=ut.o array.o link.o list.o wimeconn.o log.o

ifeq ($(WOW64) , 1)
app2=$(BIN32NAME)
endif

ifeq ($(FREEBSD) , 1)
objs+=freebsd.o
endif

include ../def.mk

install:

uninstall:

$(app):$(objs)
	ar rcs $@ $^

.PHONY:$(BIN32NAME)
$(BIN32NAME):
	mkdir -p $@
	$(MAKE) -C $@ -f ../Makefile WOW64=0 CC="$(CC32_ENV) $(CC)" CFLAGS="$(CFLAGS) -m32" -I ..
