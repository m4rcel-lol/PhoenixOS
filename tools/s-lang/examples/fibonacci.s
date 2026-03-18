/* fibonacci.s — Fibonacci sequence in S
 *
 * Compile:  sc fibonacci.s -o fibonacci
 * Run:      ./fibonacci [n]
 *
 * Demonstrates:
 *   - Recursive functions
 *   - Iterative functions
 *   - Command-line argument parsing
 *   - S type aliases (i64, i32)
 *   - for range loops
 */

/* Recursive Fibonacci (exponential time, for illustration) */
fn fib_rec(i32 n) -> i64 {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib_rec(n - 1) + fib_rec(n - 2);
}

/* Iterative Fibonacci (linear time) */
fn fib_iter(i32 n) -> i64 {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    var i64 a = 0;
    var i64 b = 1;
    for i in 2..=n {
        var i64 tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

/* Print the Fibonacci sequence up to n terms */
fn print_fib_table(i32 n) -> void {
    println("Fibonacci sequence (first %d terms):", n);
    println("%-6s %s", "n", "F(n)");
    println("%-6s %s", "------", "--------------------");
    for i in 0..=n {
        println("%-6d %lld", i, (long long)fib_iter(i));
    }
}

fn main(i32 argc, str *argv) -> i32 {
    var i32 terms = 20;

    if (argc > 1) {
        terms = atoi(argv[1]);
        if (terms < 1 || terms > 80) {
            eprintln("Usage: fibonacci [n]  (1 <= n <= 80)");
            return 1;
        }
    }

    print_fib_table(terms);

    println("\nVerification — F(%d) = %lld", terms, (long long)fib_iter(terms));
    return 0;
}
