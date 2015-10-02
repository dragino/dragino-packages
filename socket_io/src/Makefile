# socket_io Makefile

.PHONY: all
all: socket_io udpcli

socket_io: socket_io.o
	$(CC) $(LDFLAGS) socket_io.o -o socket_io
socket_io.o: socket_io.c
	$(CC) $(CFLAGS) -c socket_io.c

udpcli: udpcli.o
	$(CC) $(LDFLAGS) udpcli.o -o udpcli
udpcli.o: udpcli.c
	$(CC) $(CFLAGS) -c udpcli.c

# remove object files and executable when user executes "make clean"
.PHONY: clean
clean:
	rm *.o socket_io
