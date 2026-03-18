CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -I../../kernel/include

TESTS   := test_pmm test_vmm test_drivers
SRCS    := test_pmm.c test_vmm.c test_drivers.c

.PHONY: all run clean

all: $(TESTS)

test_pmm: test_pmm.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -o $@ $<

test_vmm: test_vmm.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -o $@ $<

test_drivers: test_drivers.c
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
	@echo "=== Running test_drivers ==="
	@./test_drivers; echo "Exit: $$?"
	@echo ""

clean:
	rm -f $(TESTS)
