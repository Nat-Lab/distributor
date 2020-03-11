CFLAGS+=-std=c++11 -O3 -Wall -Wextra
TARGETS=distributor dist-client
OBJS_distributor=src/distributor.o src/fdb.o src/switch.o src/udp-distributor.o
OBJS_client=src/client.o src/distributor-client.o src/tap-client.o
CC=c++

.PHONY: all clean
all: $(TARGETS)

distributor: $(OBJS_distributor)
	$(CC) -o distributor $(OBJS_distributor) $(CFLAGS) -lpthread

dist-client: $(OBJS_client)
	$(CC) -o dist-client $(OBJS_client) $(CFLAGS) -lpthread

%.o: %.cc
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGETS) *.o */*.o