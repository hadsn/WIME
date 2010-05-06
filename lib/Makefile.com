override CFLAGS+=-fpic -I../.. -I..

app=libwimeut.a
objs=ut.o array.o link.o list.o wimeconn.o

vpath %.c ..
-include $(DEPEND)

$(app):$(objs)
	ar rcs $@ $^

all:$(app)

clean:
	$(RM) $(app) $(objs) $(DEPEND)

install:

uninstall:

$(DEPEND): $(objs:%.o=../%.c)
	$(CC) $(CFLAGS) -MM -MG $^ >$(DEPEND)
