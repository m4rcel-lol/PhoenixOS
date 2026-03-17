BINS := ls cat cp mv rm mkdir echo pwd hostname uname ps kill head tail wc touch stat df mount

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
PREFIX  := /bin

.PHONY: all clean install $(BINS)

all: $(BINS)

ls: ls.c
	$(CC) $(CFLAGS) -o $@ $<

cat: cat.c
	$(CC) $(CFLAGS) -o $@ $<

cp: cp.c
	$(CC) $(CFLAGS) -o $@ $<

mv: mv.c
	$(CC) $(CFLAGS) -o $@ $<

rm: rm.c
	$(CC) $(CFLAGS) -o $@ $<

mkdir: mkdir.c
	$(CC) $(CFLAGS) -o $@ $<

echo: echo.c
	$(CC) $(CFLAGS) -o $@ $<

pwd: pwd.c
	$(CC) $(CFLAGS) -o $@ $<

hostname: hostname.c
	$(CC) $(CFLAGS) -o $@ $<

uname: uname.c
	$(CC) $(CFLAGS) -o $@ $<

ps: ps.c
	$(CC) $(CFLAGS) -o $@ $<

kill: kill.c
	$(CC) $(CFLAGS) -o $@ $<

head: head.c
	$(CC) $(CFLAGS) -o $@ $<

tail: tail.c
	$(CC) $(CFLAGS) -o $@ $<

wc: wc.c
	$(CC) $(CFLAGS) -o $@ $<

touch: touch.c
	$(CC) $(CFLAGS) -o $@ $<

stat: stat.c
	$(CC) $(CFLAGS) -o $@ $<

df: df.c
	$(CC) $(CFLAGS) -o $@ $<

mount: mount.c
	$(CC) $(CFLAGS) -o $@ $<

install: all
	mkdir -p $(PREFIX)
	cp $(BINS) $(PREFIX)/

clean:
	rm -f $(BINS) *.o
