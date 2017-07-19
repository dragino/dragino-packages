# single_chan_pkt_fwd
# Single Channel LoRaWAN Gateway

all: lora_udp_fwd

lora_udp_fwd: base64.o main.o
	$(CC) $(LDFLAGS) main.o base64.o $(LIBS) -luci -o lora_udp_fwd

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

base64.o: base64.c
	$(CC) $(CFLAGS) -c base64.c

clean:
	rm *.o lora_udp_fwd
