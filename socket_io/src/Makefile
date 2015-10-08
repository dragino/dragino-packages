# socket_io Makefile

.PHONY: all
all: socket_io udpcli test

socket_io: socket_io.o minIni.o
	$(CC) $(LDFLAGS) socket_io.o minIni.o -o socket_io
socket_io.o: socket_io.c
	$(CC) $(CFLAGS) -c socket_io.c

minIni.o: minIni.c
	$(CC) $(CFLAGS) -c minIni.c

test: test.o
	$(CC) $(LDFLAGS) test.o minIni.o -o test
test.o: test.c
	$(CC) $(CFLAGS) -c test.c

udpcli: udpcli.o
	$(CC) $(LDFLAGS) udpcli.o -o udpcli
udpcli.o: udpcli.c
	$(CC) $(CFLAGS) -c udpcli.c

# remove object files and executable when user executes "make clean"
.PHONY: clean
clean:
	rm *.o socket_io
