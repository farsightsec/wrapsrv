CC = gcc
WARN = -Wall -Wextra -Werror
CFLAGS = -O2 -g $(WARN)
INCLUDE =
LDFLAGS = -lresolv
DESTDIR ?=
PREFIX = /usr/local

BINDIR ?= $(DESTDIR)$(PREFIX)/bin
MANDIR ?= $(DESTDIR)$(PREFIX)/share/man/man1

BIN = wrapsrv
MAN = wrapsrv.1
SRC = wrapsrv.c

all: $(BIN) $(DOC)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(INCLUDE) $(LDFLAGS)

$(MAN): wrapsrv.docbook
	docbook2x-man $<

clean:
	rm -f $(BIN)

install: $(BIN)
	mkdir -p $(BINDIR)
	mkdir -p $(MANDIR)
	install -m 0755 $(BIN) $(BINDIR)
	install -m 0644 $(MAN) $(MANDIR)

.PHONY: all clean install
