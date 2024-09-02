CC=${CROSS_COMPILE}gcc

aesdsocket: aesdsocket.o
	$(CC) -o $@ $^

all: aesdsocket

clean:
	rm -f aesdsocket aesdsocket.o

