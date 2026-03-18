/* hello.s — Hello World in S
 *
 * Compile:  sc hello.s -o hello
 * Run:      ./hello
 *
 * This example demonstrates:
 *   - use (include) directives
 *   - fn (function) declarations with return types
 *   - let (immutable binding)
 *   - var (mutable variable)
 *   - println macro
 *   - Type aliases: i32, str
 *   - for range loops
 *   - loop with break
 */

fn greet(str name) -> void {
    println("Hello, %s! Welcome to PhoenixOS.", name);
}

fn add(i32 a, i32 b) -> i32 {
    return a + b;
}

fn factorial(i32 n) -> i64 {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

fn main() -> i32 {
    /* Basic greeting */
    greet("world");
    greet("PhoenixOS");

    /* Variables */
    let i32 x = 10;
    let i32 y = 20;
    var i32 sum = add(x, y);
    println("Sum: %d + %d = %d", x, y, sum);

    /* Range-based for loop */
    println("Counting up:");
    for i in 1..=5 {
        println("  %d", i);
    }

    /* Factorial table */
    println("Factorials:");
    for n in 1..8 {
        println("  %d! = %lld", n, (long long)factorial(n));
    }

    /* loop with break */
    var i32 countdown = 5;
    println("Counting down:");
    loop {
        println("  %d", countdown);
        countdown--;
        if (countdown <= 0) break;
    }

    /* Null / boolean */
    str name = null;
    if (name == NULL) {
        println("Name is null (mapped from S 'null')");
    }

    bool flag = true;
    if (flag) {
        println("Flag is true");
    }

    println("Done!");
    return 0;
}
