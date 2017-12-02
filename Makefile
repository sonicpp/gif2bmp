CC=gcc
CFLAGS=-std=c99 -Wall
EXEC=gif2bmp

$(EXEC): gif2bmp.o gif.o bmp.o
	$(CC) gif2bmp.o gif.o bmp.o -o $@
gif2bmp.o: gif2bmp.c gif2bmp.h gif.h bmp.h
	$(CC) $(CFLAGS) gif2bmp.c -c
gif.o: gif.c gif.h gif2bmp.h
	$(CC) $(CFLAGS) gif.c -c
bmp.o: bmp.c bmp.h gif2bmp.h
	$(CC) $(CFLAGS) bmp.c -c

clean:
	rm -f *.o $(EXEC)

