
CC=gcc
CFLAGS= -Wall
CPPFLAGS=
LDFLAGS=

VPATH=

OBJS=sha1.o base64.o wsserv.o
EXEC=wsserv

.PHONY: all, clean, distclean

all: ${EXEC}

${EXEC}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} -o $@ ${LDFLAGS}
	@ls -lh $@
	@size $@

%.o: %.c %.h
	${CC} -c ${CFLAGS} $< -o $@

clean:
	@rm -f *.o a.out

distclean: clean
	rm -f *~

