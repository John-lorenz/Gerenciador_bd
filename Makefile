# make  —  FIFO + Pthreads (Linux ou WSL em ~/ ; nao use /mnt/c/ para mkfifo)
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

.PHONY: all clean

all: servidor cliente

servidor: servidor.c banco.h
	$(CC) $(CFLAGS) -o servidor servidor.c $(LDFLAGS)

cliente: cliente.c banco.h
	$(CC) $(CFLAGS) -o cliente cliente.c $(LDFLAGS)

clean:
	rm -f servidor cliente bd_req.fifo bd_resp.fifo
