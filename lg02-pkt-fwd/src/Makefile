# lg02_pkt_fwd

all: lg02_pkt_fwd

lg02_pkt_fwd: base64.o parson.o radio.o main.o
	$(CC) main.o base64.o parson.o radio.o $(LIBS) -luci -lrt -ldl -lpthread -o lg02_pkt_fwd

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

base64.o: base64.c
	$(CC) $(CFLAGS) -c base64.c

parson.o: parson.c
	$(CC) $(CFLAGS) -c parson.c

radio.o: radio.c
	$(CC) $(CFLAGS) -c radio.c

clean:
	rm *.o lg02_pkt_fwd	
