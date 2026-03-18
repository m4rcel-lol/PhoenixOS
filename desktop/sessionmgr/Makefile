BIN     := sessionmgr

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE
PREFIX  := /usr/bin

SRCS    := sessionmgr.c
OBJS    := $(SRCS:.c=.o)

.PHONY: all clean install

all: $(BIN)

$(BIN): $(OBJS)
	@echo "[LD ] $@"
	$(CC) -o $@ $(OBJS)

%.o: %.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	@mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)/

clean:
	rm -f $(OBJS) $(BIN)

-include $(OBJS:.o=.d)
