# lg02_pkt_fwd

all: lg01_pkt_fwd lg02_pkt_fwd lg02_single_rx_tx spirw

lg01_pkt_fwd: base64.o parson.o radio.o jitqueue.o lg01_pkt_fwd.o
	$(CC) lg01_pkt_fwd.o base64.o parson.o radio.o jitqueue.o $(LIBS) -luci -lmosquitto -lrt -ldl -lpthread -o lg01_pkt_fwd

lg02_pkt_fwd: base64.o parson.o radio.o jitqueue.o lg02_pkt_fwd.o
	$(CC) lg02_pkt_fwd.o base64.o parson.o radio.o jitqueue.o $(LIBS) -luci -lmosquitto -lrt -ldl -lpthread -o lg02_pkt_fwd

lg02_single_rx_tx: radio.o
	$(CC) $(CFLAGS) radio.o $(LIBS) -lrt -ldl lg02_single_rx_tx.c -o lg02_single_rx_tx

spirw: radio.o
	$(CC) $(CFLAGS) radio.o $(LIBS) -lrt -ldl simple_spi_rw.c -o spirw

lg01_pkt_fwd.o: lg01_pkt_fwd.c
	$(CC) $(CFLAGS) -c lg01_pkt_fwd.c

lg02_pkt_fwd.o: lg02_pkt_fwd.c
	$(CC) $(CFLAGS) -c lg02_pkt_fwd.c

jitqueue.o: jitqueue.c
	$(CC) $(CFLAGS) -c jitqueue.c

base64.o: base64.c
	$(CC) $(CFLAGS) -c base64.c

parson.o: parson.c
	$(CC) $(CFLAGS) -c parson.c

radio.o: radio.c
	$(CC) $(CFLAGS) -c radio.c

clean:
	rm *.o lg01_pkt_fwd lg02_pkt_fwd lg02_single_rx_tx spirw
