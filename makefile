CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LIB = libmsocket.a

all: $(LIB) init genfile
	gcc user2.c -o runrecv msocket.o -pthread
	gcc user1.c -o runsend msocket.o -pthread
$(LIB): msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	$(CC) $(CFLAGS) -c msocket.c -o msocket.o

init: init.o
	$(CC) $(CFLAGS) -o init init.o -L. -lmsocket -pthread -lrt

init.o: initmsocket
	$(CC) $(CFLAGS) -c initmsocket -o init.o -lrt
genfile:
	gcc genfile.c -o genfile
clean:
	rm -f $(LIB) msocket.o
