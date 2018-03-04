tcp_client: tcp_client.o
	$(CC) $(LDFLAGS) tcp_client.o -luci -lrt -o tcp_client

tcp_client.o: tcp_client.c
	$(CC) $(CFLAGS) -c tcp_client.c

clean:
	rm *.o tcp_client
