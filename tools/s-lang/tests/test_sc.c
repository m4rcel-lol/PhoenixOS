/* test_sc.c — Unit tests for the S language transpiler
 *
 * Runs sc with --emit-c on small S snippets and checks the output.
 * Build:  gcc -O2 -Wall -std=c11 -o test_sc test_sc.c
 * Run:    ./test_sc [path/to/sc]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ── Mini test framework ──────────────────────────────────────────────────── */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define PASS(msg)  do { tests_run++; tests_passed++; printf("  PASS: %s\n", msg); } while(0)
#define FAIL(msg)  do { tests_run++; tests_failed++; printf("  FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; printf("  PASS: %s\n", msg); } \
    else      { tests_failed++; printf("  FAIL: %s  (line %d)\n", msg, __LINE__); } \
} while(0)

#define GROUP(name) printf("\n[TEST] %s\n", name)

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static char sc_path[512] = "./sc";

/* Write S source to a temp file, run sc --emit-c, return allocated output.
 * Returns NULL on error.                                                      */
static char *transpile(const char *s_source) {
    char tmp_in[64], tmp_out[64];
    snprintf(tmp_in,  sizeof(tmp_in),  "/tmp/sc_test_in_%d.s",  (int)getpid());
    snprintf(tmp_out, sizeof(tmp_out), "/tmp/sc_test_out_%d.c", (int)getpid());

    FILE *f = fopen(tmp_in, "w");
    if (!f) return NULL;
    fputs(s_source, f);
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s \"%s\" --emit-c > \"%s\" 2>/dev/null",
             sc_path, tmp_in, tmp_out);
    int rc = system(cmd);
    remove(tmp_in);

    if (rc != 0) { remove(tmp_out); return NULL; }

    FILE *out = fopen(tmp_out, "r");
    if (!out) return NULL;
    fseek(out, 0, SEEK_END);
    long sz = ftell(out);
    fseek(out, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(out); remove(tmp_out); return NULL; }
    fread(buf, 1, sz, out);
    buf[sz] = '\0';
    fclose(out);
    remove(tmp_out);
    return buf;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Tests                                                                        */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_use_directives(void) {
    GROUP("use → #include");

    char *out;

    out = transpile("use <stdio.h>\n");
    CHECK(out && strstr(out, "#include <stdio.h>"), "use <stdio.h> → #include <stdio.h>");
    free(out);

    out = transpile("use \"mylib.h\"\n");
    CHECK(out && strstr(out, "#include \"mylib.h\""), "use \"mylib.h\" → #include \"mylib.h\"");
    free(out);
}

static void test_fn_declarations(void) {
    GROUP("fn declarations");

    char *out;

    /* fn with return type */
    out = transpile("fn add(int a, int b) -> int { return a + b; }\n");
    CHECK(out && strstr(out, "int add(int a, int b)"), "fn → return_type name(params)");
    free(out);

    /* fn without return type → void */
    out = transpile("fn hello() { }\n");
    CHECK(out && strstr(out, "void hello()"), "fn (no return type) → void name()");
    free(out);

    /* fn with type aliases in params */
    out = transpile("fn foo(i32 x, u8 y) -> i64 { return (i64)x + y; }\n");
    CHECK(out && strstr(out, "int32_t x") && strstr(out, "uint8_t y"),
          "Type aliases in params (i32→int32_t, u8→uint8_t)");
    CHECK(out && strstr(out, "int64_t foo"), "Return type alias (i64→int64_t)");
    free(out);
}

static void test_let_var(void) {
    GROUP("let/var declarations");

    char *out;

    /* let → const */
    out = transpile("fn f() { let int x = 5; }\n");
    CHECK(out && strstr(out, "const int x = 5;"), "let int x = 5 → const int x = 5");
    free(out);

    /* var → mutable */
    out = transpile("fn f() { var int y = 10; }\n");
    CHECK(out && strstr(out, "int y = 10;"), "var int y = 10 → int y = 10");
    /* should NOT have 'const' before 'int y' */
    if (out) {
        /* 'const int y' must not appear */
        int bad = strstr(out, "const int y") != NULL;
        CHECK(!bad, "var must not emit 'const'");
    }
    free(out);

    /* let with type alias */
    out = transpile("fn f() { let i32 count = 0; }\n");
    CHECK(out && strstr(out, "const int32_t count = 0;"),
          "let i32 → const int32_t");
    free(out);
}

static void test_type_aliases(void) {
    GROUP("Type aliases");

    struct { const char *s_type; const char *c_type; } cases[] = {
        {"i8",    "int8_t"},
        {"i16",   "int16_t"},
        {"i32",   "int32_t"},
        {"i64",   "int64_t"},
        {"u8",    "uint8_t"},
        {"u16",   "uint16_t"},
        {"u32",   "uint32_t"},
        {"u64",   "uint64_t"},
        {"f32",   "float"},
        {"f64",   "double"},
        {"str",   "char*"},
        {"byte",  "uint8_t"},
        {"usize", "size_t"},
    };
    int n = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < n; i++) {
        char src[128];
        snprintf(src, sizeof(src), "fn f(%s x) { }\n", cases[i].s_type);
        char *out = transpile(src);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s → %s", cases[i].s_type, cases[i].c_type);
        CHECK(out && strstr(out, cases[i].c_type), msg);
        free(out);
    }
}

static void test_null_keyword(void) {
    GROUP("null → NULL");

    char *out = transpile("fn f() { char *p = null; }\n");
    CHECK(out && strstr(out, "NULL"), "null → NULL");
    free(out);
}

static void test_loop_keyword(void) {
    GROUP("loop → while(1)");

    char *out = transpile("fn f() { loop { break; } }\n");
    CHECK(out && strstr(out, "while (1)"), "loop → while (1)");
    free(out);
}

static void test_range_for(void) {
    GROUP("Range-based for loops");

    char *out;

    /* Exclusive range */
    out = transpile("fn f() { for i in 0..10 { } }\n");
    CHECK(out && strstr(out, "for (int i = 0; i < 10; i++)"),
          "for i in 0..10 → for (int i = 0; i < 10; i++)");
    free(out);

    /* Inclusive range */
    out = transpile("fn f() { for i in 1..=5 { } }\n");
    CHECK(out && strstr(out, "for (int i = 1; i <= 5; i++)"),
          "for i in 1..=5 → for (int i = 1; i <= 5; i++)");
    free(out);
}

static void test_passthrough(void) {
    GROUP("Pass-through (C constructs untouched)");

    char *out;

    /* Normal C for loop must be preserved */
    out = transpile("fn f() { for (int i = 0; i < 10; i++) { } }\n");
    CHECK(out && strstr(out, "for (int i = 0; i < 10; i++)"), "Standard C for loop preserved");
    free(out);

    /* Comments preserved */
    out = transpile("// This is a comment\nfn f() { }\n");
    CHECK(out && strstr(out, "// This is a comment"), "Line comments preserved");
    free(out);

    /* String literals preserved */
    out = transpile("fn f() { char *s = \"hello world\"; }\n");
    CHECK(out && strstr(out, "\"hello world\""), "String literals preserved");
    free(out);

    /* Struct definition preserved */
    out = transpile("typedef struct { int x; int y; } Point;\n");
    CHECK(out && strstr(out, "typedef struct"), "typedef struct preserved");
    free(out);
}

static void test_compile_examples(void) {
    GROUP("Example programs compile without errors");

    struct { const char *path; const char *name; } examples[] = {
        {"../examples/hello.s",       "hello.s"},
        {"../examples/fibonacci.s",   "fibonacci.s"},
        {"../examples/kernel-demo.s", "kernel-demo.s"},
    };
    int n = (int)(sizeof(examples) / sizeof(examples[0]));

    for (int i = 0; i < n; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s \"%s\" --emit-c > /dev/null 2>&1",
                 sc_path, examples[i].path);
        int rc = system(cmd);
        char msg[128];
        snprintf(msg, sizeof(msg), "%s transpiles OK", examples[i].name);
        CHECK(rc == 0, msg);
    }
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* Main                                                                         */
/* ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    if (argc > 1) {
        snprintf(sc_path, sizeof(sc_path), "%s", argv[1]);
    }

    printf("S Language Transpiler Tests\n");
    printf("Using sc: %s\n", sc_path);

    test_use_directives();
    test_fn_declarations();
    test_let_var();
    test_type_aliases();
    test_null_keyword();
    test_loop_keyword();
    test_range_for();
    test_passthrough();
    test_compile_examples();

    printf("\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}
