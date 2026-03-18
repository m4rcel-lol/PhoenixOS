SC      := sc
CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11

EXAMPLES := examples/hello examples/fibonacci examples/kernel-demo

PREFIX  := /usr/local/bin
INCDIR  := /usr/local/include

.PHONY: all examples test clean install help

all: $(SC)

$(SC): sc.c
	@echo "[CC ] sc.c -> sc"
	$(CC) $(CFLAGS) -o $@ sc.c

examples: $(SC) $(EXAMPLES)

examples/hello: examples/hello.s $(SC)
	@echo "[SC ] $<"
	./$(SC) $< -o $@

examples/fibonacci: examples/fibonacci.s $(SC)
	@echo "[SC ] $<"
	./$(SC) $< -o $@

examples/kernel-demo: examples/kernel-demo.s $(SC)
	@echo "[SC ] $<"
	./$(SC) $< -o $@

test: $(SC) $(EXAMPLES)
	@echo ""
	@echo "=== Running hello ==="
	@./examples/hello
	@echo ""
	@echo "=== Running fibonacci (first 15 terms) ==="
	@./examples/fibonacci 15
	@echo ""
	@echo "=== Running kernel-demo ==="
	@./examples/kernel-demo
	@echo ""
	@echo "=== Checking transpiler output ==="
	@./$(SC) examples/hello.s --emit-c > /dev/null && echo "  hello.s transpiles OK"
	@./$(SC) examples/fibonacci.s --emit-c > /dev/null && echo "  fibonacci.s transpiles OK"
	@./$(SC) examples/kernel-demo.s --emit-c > /dev/null && echo "  kernel-demo.s transpiles OK"
	@echo ""
	@echo "All S language tests passed."

install: $(SC)
	@mkdir -p $(PREFIX) $(INCDIR)
	cp $(SC) $(PREFIX)/
	cp include/s.h $(INCDIR)/
	@echo "Installed sc -> $(PREFIX)/sc"
	@echo "Installed s.h -> $(INCDIR)/s.h"

clean:
	rm -f $(SC) $(EXAMPLES) /tmp/sc_*.c

help:
	@echo "S Language Compiler"
	@echo ""
	@echo "Targets:"
	@echo "  all       Build the sc compiler"
	@echo "  examples  Build all example programs"
	@echo "  test      Build and run examples, verify transpiler"
	@echo "  install   Install sc and s.h system-wide"
	@echo "  clean     Remove build artifacts"
