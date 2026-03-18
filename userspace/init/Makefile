BIN     := kindle

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE
PREFIX  := /sbin

.PHONY: all clean install

all: $(BIN)

$(BIN): kindle.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)/

clean:
	rm -f $(BIN) *.o
