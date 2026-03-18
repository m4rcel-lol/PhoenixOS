LIB     := libgui.a

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -Iinclude -fPIC
AR      := ar
ARFLAGS := rcs

SRCS    := window.c widgets.c
OBJS    := $(SRCS:.c=.o)

DESTDIR ?= ../../build
LIBDIR  := $(DESTDIR)/lib
INCDIR  := $(DESTDIR)/include

.PHONY: all clean install

all: $(LIB)

$(LIB): $(OBJS)
	@echo "[AR ] $@"
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

install: all
	@mkdir -p $(LIBDIR) $(INCDIR)
	cp $(LIB)       $(LIBDIR)/
	cp include/gui.h $(INCDIR)/

clean:
	rm -f $(OBJS) $(LIB)

-include $(OBJS:.o=.d)
