# single_chan_pkt_fwd
# Single Channel LoRaWAN Gateway

all: lg01_pkt_fwd

lg01_pkt_fwd: parson.o base64.o main.o
	$(CC) $(LDFLAGS) main.o base64.o parson.o $(LIBS) -luci -lrt -ldl -lpthread -o lg01_pkt_fwd

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

base64.o: base64.c
	$(CC) $(CFLAGS) -c base64.c

parson.o: parson.c
	$(CC) $(CFLAGS) -c parson.c

clean:
	rm *.o lg01_pkt_fwd
