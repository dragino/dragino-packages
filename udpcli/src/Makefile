# socket_io Makefile

.PHONY: all
all: udplistener udpcli

udplistener: udplistener.o
	$(CC) $(LDFLAGS) udplistener.o -o udplistener
socket_io.o: udplistener.c
	$(CC) $(CFLAGS) -c udplistener.c

udpcli: udpcli.o
	$(CC) $(LDFLAGS) udpcli.o -o udpcli
udpcli.o: udpcli.c
	$(CC) $(CFLAGS) -c udpcli.c

# remove object files and executable when user executes "make clean"
.PHONY: clean
clean:
	rm *.o udplistener udpcli 
