
CC=gcc
CFLAGS= -Wall
CPPFLAGS=
LDFLAGS=

VPATH=

OBJS=sha1.o base64.o indexer.o wsserv.o
EXEC=wsserv

.PHONY: all, clean, distclean

all: ${EXEC} idx tags

idx: indexer.c
	@echo make tools...
	${CC} indexer.c -DIDX_TOOL -o idx -levent

${EXEC}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} -o $@ ${LDFLAGS} -levent
	@ls -lh $@
	@size $@

%.o: %.c %.h
	${CC} -c ${CFLAGS} $< -o $@

tags:
	./ctags_with_deps.sh *.c

clean:
	@rm -f *.o a.out ${EXEC} idx

distclean: clean
	rm -f *~ tags

