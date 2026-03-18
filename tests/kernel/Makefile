CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -I../../kernel/include

TESTS   := test_pmm test_vmm
SRCS    := test_pmm.c test_vmm.c

.PHONY: all run clean

all: $(TESTS)

test_pmm: test_pmm.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -o $@ $<

test_vmm: test_vmm.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -o $@ $<

run: all
	@echo ""
	@echo "=== Running test_pmm ==="
	@./test_pmm; echo "Exit: $$?"
	@echo ""
	@echo "=== Running test_vmm ==="
	@./test_vmm; echo "Exit: $$?"
	@echo ""

clean:
	rm -f $(TESTS)
