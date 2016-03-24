CFLAGS= -g3 -Wall -std=c99 -pedantic

HWK5 = /c/cs323/Hwk5

all:    Bsh

Bsh:    ${HWK5}/mainBsh.o process.o ${HWK5}/parse.o ${HWK5}/getLine.o
	${CC} ${CFLAGS} -o $@ $^

mainBsh.o: ${HWK5}/getLine.h ${HWK5}/parse.h ${HWK5}/process-stub.h

process.o: process.c ${HWK5}/parse.h ${HWK5}/process-stub.h

clean:
	rm -f *.o Bsh
