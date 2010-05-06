#ここの関数は32,64両方から使用される。
#archコマンドの出力とBIN32が違う場合は64環境であるとして32ビット用のディレクトリをつくる。

arch=$(shell $(ARCH))
subdirs=$(arch)

ifneq "$(arch)" "$(BIN32)"
subdirs+=$(BIN32)

#32ビット用ディレクトリ
$(BIN32):
	mkdir $(BIN32)
	ln -s ../Makefile32 $(BIN32)/Makefile
endif


#このマシン用バイナリのディレクトリ
$(arch):
	mkdir $(arch)
	ln -s ../Makefile.com $(arch)/Makefile

include ../def.mk

all install uninstall depend: $(subdirs)
	$(callsubmake)

clean:
	rm -rf $(subdirs)
