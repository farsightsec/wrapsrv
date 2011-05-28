CC = gcc
WARN = -Wall -Wextra -Werror
CFLAGS = -O2 -g $(WARN)
INCLUDE =
LDFLAGS = -lresolv
DESTDIR = /usr/local

BIN = wrapsrv
DOC = wrapsrv.1
SRC = wrapsrv.c

all: $(BIN) $(DOC)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(INCLUDE) $(LDFLAGS)

$(DOC): wrapsrv.docbook
	docbook2x-man $<

clean:
	rm -f $(BIN)

install: $(BIN)
	install -m 0755 $(BIN) $(DESTDIR)/bin

.PHONY: all clean install
