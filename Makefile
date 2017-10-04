CC=gcc
CFLAGS=-std=c99 -Wall
EXEC=gif2bmp

$(EXEC): gif2bmp.o
	$(CC) gif2bmp.o -o $@
gif2bmp.o: gif2bmp.c gif2bmp.h
	$(CC) gif2bmp.c -c

clean:
	rm -f *.o $(EXEC)

