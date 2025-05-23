CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -pthread
TARGETS=solicitante receptor

all: $(TARGETS)

solicitante: solicitante.c estructuras.h
	$(CC) $(CFLAGS) -o solicitante solicitante.c

receptor: receptor.c estructuras.h
	$(CC) $(CFLAGS) -o receptor receptor.c

clean:
	rm -f $(TARGETS) *.o

.PHONY: all clean
