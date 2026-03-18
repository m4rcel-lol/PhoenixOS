BIN     := phoenix-install

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE
PREFIX  := /bin

.PHONY: all clean install

all: $(BIN)

$(BIN): phoenix-install.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)/

clean:
	rm -f $(BIN) *.o
