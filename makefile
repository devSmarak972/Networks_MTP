CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LIB = libmsocket.a

all: $(LIB) init
	gcc user_1.c -o runrecv msocket.o -pthread
	gcc user_2.c -o runsend msocket.o -pthread
$(LIB): msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	$(CC) $(CFLAGS) -c msocket.c -o msocket.o

init: init.o
	$(CC) $(CFLAGS) -o init init.o -L. -lmsocket -pthread

init.o: init.c
	$(CC) $(CFLAGS) -c init.c -o init.o

clean:
	rm -f $(LIB) msocket.o
