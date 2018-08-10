# lg02_pkt_fwd

all: lg02_pkt_fwd lg02_single_rx_tx

lg02_pkt_fwd: base64.o parson.o radio.o jitqueue.o main.o
	$(CC) main.o base64.o parson.o radio.o jitqueue.o $(LIBS) -luci -lrt -ldl -lpthread -o lg02_pkt_fwd

lg02_single_rx_tx: radio.o
	$(CC) $(CFLAGS) radio.o $(LIBS) -lrt -ldl -lpthread lg02_single_rx_tx.c -o lg02_single_rx_tx

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
	rm *.o lg02_pkt_fwd lg02_single_rx_tx	
