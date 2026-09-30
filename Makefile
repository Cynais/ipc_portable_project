CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Iinclude -Isrc -Iexamples
LDLIBS = -pthread -lrt

COMMON = src/ipc_transport.c src/ipc_protocol.c src/ipc_platform_posix.c

all: ipc_server ipc_client

ipc_server: examples/server.c $(COMMON)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDLIBS)

ipc_client: examples/client.c $(COMMON)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f ipc_server ipc_client

.PHONY: all clean
