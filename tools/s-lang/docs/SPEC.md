# S Language Specification

**S** is a simplified dialect of C designed for PhoenixOS development. It
transpiles to standard C11 before compilation, so it produces fully portable
native binaries with zero runtime overhead.

## Overview

| Property | Value |
|----------|-------|
| Full name | S Language |
| Version | 0.1.0 |
| Paradigm | Imperative, procedural |
| Transpiles to | C11 |
| Compiler | `sc` (the S compiler) |
| File extension | `.s` |
| Target | PhoenixOS, Linux, bare-metal x86_64 |

---

## Motivation

C is a powerful but verbose language. S removes common boilerplate:

| Pattern | C | S |
|---------|---|---|
| Function declaration | `int foo(int x)` | `fn foo(int x) -> int` |
| Immutable variable | `const int x = 5;` | `let int x = 5;` |
| Mutable variable | `int x = 5;` | `var int x = 5;` |
| Include | `#include <stdio.h>` | `use <stdio.h>` |
| 32-bit int | `int32_t` | `i32` |
| Unsigned byte | `uint8_t` | `u8` |
| String pointer | `char*` | `str` |
| Range loop | `for (int i = 0; i < 10; i++)` | `for i in 0..10` |
| Infinite loop | `while (1)` | `loop` |
| Null pointer | `NULL` | `null` |

---

## Toolchain

### The `sc` compiler

```bash
# Compile an S source file to a binary
sc hello.s

# Compile with explicit output name
sc hello.s -o hello

# Transpile only — write generated C to a file
sc hello.s -C hello.c

# Transpile only — print generated C to stdout
sc hello.s --emit-c

# Show help
sc --help
```

The `sc` compiler:
1. Tokenises the `.s` source file
2. Applies S→C transformations
3. Writes a temporary `.c` file
4. Invokes `gcc` to compile to a binary

### Build integration

Add to your `Makefile`:

```makefile
SC := $(shell which sc || echo tools/s-lang/sc)

my_program: my_program.s
	$(SC) $< -o $@
```

---

## Language Reference

### 1. Includes — `use`

```s
use <stdio.h>       // → #include <stdio.h>
use <stdint.h>      // → #include <stdint.h>
use "mylib.h"       // → #include "mylib.h"
```

S programs automatically get `#include <s.h>` (the S prelude) unless you
explicitly include it yourself.

---

### 2. Functions — `fn`

```s
fn name(param_type param, ...) -> return_type {
    // body
}
```

If the return type is omitted, `void` is assumed:

```s
fn greet(str name) -> void {
    println("Hello, %s!", name);
}

// Same as above — void is default
fn greet(str name) {
    println("Hello, %s!", name);
}
```

**Examples:**

```s
fn add(i32 a, i32 b) -> i32 {
    return a + b;
}

fn factorial(i64 n) -> i64 {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

fn main() -> i32 {
    println("Result: %d", add(3, 4));
    return 0;
}
```

**Transpiled to:**

```c
int32_t add(int32_t a, int32_t b) {
    return a + b;
}

int64_t factorial(int64_t n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main() {
    println("Result: %d", add(3, 4));
    return 0;
}
```

---

### 3. Variables — `let` and `var`

**`let` — immutable (const) binding:**

```s
let i32 x = 42;           // → const int32_t x = 42;
let str msg = "hello";    // → const char* msg = "hello";
let int count = 0;        // → const int count = 0;
```

**`var` — mutable variable:**

```s
var i32 total = 0;        // → int32_t total = 0;
var str name = null;      // → char* name = NULL;
var bool done = false;    // → bool done = false;
```

**Name-colon-type syntax (alternative):**

```s
let x: i32 = 10;          // → const int32_t x = 10;
var buf: u8* = null;      // → uint8_t* buf = NULL;
```

---

### 4. Type Aliases

| S type | C type |
|--------|--------|
| `i8`   | `int8_t` |
| `i16`  | `int16_t` |
| `i32`  | `int32_t` |
| `i64`  | `int64_t` |
| `u8`   | `uint8_t` |
| `u16`  | `uint16_t` |
| `u32`  | `uint32_t` |
| `u64`  | `uint64_t` |
| `f32`  | `float` |
| `f64`  | `double` |
| `str`  | `char*` |
| `byte` | `uint8_t` |
| `usize`| `size_t` |
| `isize`| `ssize_t` |
| `bool` | `bool` (from `<stdbool.h>`) |

---

### 5. Keywords

| S keyword | C equivalent | Notes |
|-----------|-------------|-------|
| `fn`      | Function declaration | `fn name(params) -> type` |
| `let`     | `const T name =` | Immutable binding |
| `var`     | `T name =` | Mutable variable |
| `use`     | `#include` | Include directive |
| `null`    | `NULL` | Null pointer constant |
| `true`    | `true` | Via `<stdbool.h>` |
| `false`   | `false` | Via `<stdbool.h>` |
| `loop`    | `while (1)` | Infinite loop |
| `in`      | (part of range-for) | `for x in a..b` |

All standard C keywords (`if`, `else`, `while`, `for`, `return`, `struct`,
`typedef`, `sizeof`, `break`, `continue`, `switch`, `case`, etc.) work as-is.

---

### 6. Control Flow

#### Range-based `for` loop

```s
for i in 0..10 {           // i = 0, 1, ..., 9  (exclusive end)
    println("%d", i);
}

for i in 1..=5 {           // i = 1, 2, 3, 4, 5  (inclusive end)
    println("%d", i);
}
```

Transpiles to:

```c
for (int i = 0; i < 10; i++) { ... }
for (int i = 1; i <= 5; i++) { ... }
```

The loop variable is declared as `int`. For larger ranges, use a standard C
`for` loop.

#### `loop` — infinite loop

```s
loop {
    // runs forever until break
    if (done) break;
}
```

Transpiles to:

```c
while (1) {
    if (done) break;
}
```

#### Standard C control flow (unchanged)

```s
while (condition) { ... }
do { ... } while (condition);
if (x > 0) { ... } else { ... }
switch (x) { case 1: ...; break; }
```

---

### 7. The S Prelude (`s.h`)

Every S program automatically includes `<s.h>`, which provides:

#### I/O macros

```s
println("Hello, %s! n=%d", name, n);     // printf + '\n'
print("value: %d", x);                   // printf (no newline)
eprint("warning: %s", msg);              // fprintf(stderr, ...)
eprintln("error: %s", err);              // fprintf(stderr, ...) + '\n'
```

#### Panic / assertion

```s
panic("something went wrong");           // abort with message
panic_if(ptr == null, "null pointer");   // conditional panic
unreachable();                           // mark unreachable code
```

#### Memory helpers

```s
var i32* arr = s_new(i32, 100);          // calloc(100, sizeof(i32))
s_free(arr);                             // free + NULL
s_zero(my_struct);                       // memset to 0
```

#### Array length

```s
var i32 nums[] = {1, 2, 3, 4, 5};
for i in 0..S_ARRAY_LEN(nums) {
    println("%d", nums[i]);
}
```

#### Utilities

```s
var i32 clamped = S_CLAMP(val, 0, 255);
var i32 lo = S_MIN(a, b);
var i32 hi = S_MAX(a, b);
S_SWAP(i32, a, b);                       // swap two variables
```

#### Bit manipulation

```s
var u32 mask = S_BIT(7);                 // 0x80
var u64 flag = S_BIT64(32);             // 0x100000000
var usize aligned = S_ALIGN_UP(size, 4096);
```

#### Result type

```s
fn open_file(str path) -> SResult {
    if (path == null) return S_ERR;
    return S_OK;
}

SResult r = open_file("data.txt");
if (s_err(r)) {
    eprintln("Failed to open file");
}
```

---

### 8. Structs

Structs are identical to C:

```s
typedef struct {
    i32  x, y;
    u32  width, height;
    str  title;
} Window;

fn window_area(Window *w) -> u32 {
    return (u32)w->width * w->height;
}

fn main() -> i32 {
    Window win = {0, 0, 800, 600, "My Window"};
    println("Area: %u", window_area(&win));
    return 0;
}
```

---

### 9. Pointers and Arrays

Pointer syntax is identical to C:

```s
fn sum_array(i32 *arr, i32 len) -> i32 {
    var i32 total = 0;
    for i in 0..len {
        total += arr[i];
    }
    return total;
}
```

---

### 10. Preprocessor

C preprocessor directives (`#define`, `#ifdef`, `#pragma`, etc.) work as-is:

```s
#define MAX_TASKS  256
#define KBASE      0xFFFFFFFF80000000ULL

#ifdef DEBUG
fn debug_log(str msg) -> void {
    eprintln("[DEBUG] %s", msg);
}
#else
fn debug_log(str msg) -> void { S_UNUSED(msg); }
#endif
```

---

## Complete Example

```s
/* sort.s — Bubble sort demonstration */

fn bubble_sort(i32 *arr, i32 n) -> void {
    for i in 0..n {
        for j in 0..(n - i - 1) {
            if (arr[j] > arr[j + 1]) {
                S_SWAP(i32, arr[j], arr[j + 1]);
            }
        }
    }
}

fn print_array(i32 *arr, i32 n) -> void {
    print("[");
    for i in 0..n {
        if (i > 0) print(", ");
        print("%d", arr[i]);
    }
    println("]");
}

fn main() -> i32 {
    var i32 data[] = {64, 34, 25, 12, 22, 11, 90};
    let i32 n = S_ARRAY_LEN(data);

    print("Before: "); print_array(data, n);
    bubble_sort(data, n);
    print("After:  "); print_array(data, n);

    return 0;
}
```

---

## Differences from C

1. **`fn` keyword** — functions use `fn` instead of `return_type name`
2. **`let`/`var`** — explicit immutability declaration
3. **`use`** — cleaner include syntax
4. **Range for loops** — `for i in 0..n` instead of `for (int i=0; i<n; i++)`
5. **`loop`** — `while (1)` shorthand
6. **`null`** — lowercase `NULL`
7. **Type aliases** — `i32`, `u8`, `str`, etc.
8. **Auto-prelude** — `<s.h>` always available
9. **Everything else** — identical to C11

S is **not** memory-safe; it retains C's pointer model and requires manual
memory management, just like C.

---

## Roadmap

- [ ] `defer` statement (run at end of scope)
- [ ] String interpolation `f"Hello, {name}!"`
- [ ] Improved type inference for `let`/`var` (like `auto`)
- [ ] Enum with associated values
- [ ] Module system (`mod`)
- [ ] Inline tests (`#[test]`)
- [ ] Cross-compilation support (`sc --target x86_64-elf`)
