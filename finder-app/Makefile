CC=${CROSS_COMPILE}gcc

all: writer

clean:
	rm -f writer writer.o

writer: writer.o
	$(CC) -o $@ $^
