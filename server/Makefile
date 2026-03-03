CC := $(if $(CROSS_COMPILE),aarch64-none-linux-gnu-gcc,gcc)
CFLAGS = -g -Wall -Werror

all: aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket

clean:
	rm -f *.o aesdsocket
