CC=${CROSS_COMPILE}gcc

aesdsocket: aesdsocket.o signal.o server.o
	$(CC) -o $@ $^

all: aesdsocket

clean:
	rm -f aesdsocket *.o