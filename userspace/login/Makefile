BIN     := login

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE
LDFLAGS := -lcrypt
PREFIX  := /bin

.PHONY: all clean install

all: $(BIN)

$(BIN): login.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: all
	mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)/

clean:
	rm -f $(BIN) *.o
