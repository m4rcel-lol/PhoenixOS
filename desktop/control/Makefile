BIN     := forge

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE \
           -I../../lib/libgui/include
LDFLAGS := -L../../lib/libgui -lgui

SRCS    := forge.c
OBJS    := $(SRCS:.c=.o)

LIBGUI  := ../../lib/libgui/libgui.a

PREFIX  := /usr/bin

.PHONY: all clean install libs

all: libs $(BIN)

libs:
	$(MAKE) -C ../../lib/libgui

$(BIN): $(OBJS) $(LIBGUI)
	@echo "[LD ] $@"
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	@mkdir -p $(PREFIX)
	cp $(BIN) $(PREFIX)/

clean:
	rm -f $(OBJS) $(BIN)

-include $(OBJS:.o=.d)
