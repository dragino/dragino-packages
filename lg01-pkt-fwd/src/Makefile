# lg01_pkt_fwd

all: lg01_pkt_fwd lg01_single_rx_tx

lg01_pkt_fwd: base64.o parson.o radio.o jitqueue.o main.o
	$(CC) main.o base64.o parson.o radio.o jitqueue.o $(LIBS) -luci -lmosquitto -lrt -ldl -lpthread -o lg01_pkt_fwd

lg01_single_rx_tx: radio.o
	$(CC) $(CFLAGS) radio.o $(LIBS) -lrt -ldl lg01_single_rx_tx.c -o lg01_single_rx_tx

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

jitqueue.o: jitqueue.c
	$(CC) $(CFLAGS) -c jitqueue.c

base64.o: base64.c
	$(CC) $(CFLAGS) -c base64.c

parson.o: parson.c
	$(CC) $(CFLAGS) -c parson.c

radio.o: radio.c
	$(CC) $(CFLAGS) -c radio.c

clean:
	rm *.o lg01_pkt_fwd lg01_single_rx_tx
