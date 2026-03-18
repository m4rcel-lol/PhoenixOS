BIN     := ash-panel

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE \
           -I../../lib/libgui/include \
           -I../../lib/libipc/include
LDFLAGS := -L../../lib/libgui -lgui \
           -L../../lib/libipc -lipc

SRCS    := panel.c
OBJS    := $(SRCS:.c=.o)

LIBGUI  := ../../lib/libgui/libgui.a
LIBIPC  := ../../lib/libipc/libipc.a

PREFIX  := /usr/bin

.PHONY: all clean install libs

all: libs $(BIN)

libs:
	$(MAKE) -C ../../lib/libgui
	$(MAKE) -C ../../lib/libipc

$(BIN): $(OBJS) $(LIBGUI) $(LIBIPC)
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
