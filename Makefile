CC = gcc
CFLAGS = -O2 -g
WARN = -Wall -Wextra -Werror
INCLUDE = -I/usr/include/bind -I/usr/local/include/bind
LDFLAGS = -L/usr/local/lib -lbind
DESTDIR = /usr/local

BIN = wrapsrv
SRC = wrapsrv.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(WARN) -o $@ $(SRC) $(INCLUDE) $(LDFLAGS)

clean:
	rm -f $(BIN)

install:
	install -m 0755 $(BIN) $(DESTDIR)/bin

.PHONY: all clean install
